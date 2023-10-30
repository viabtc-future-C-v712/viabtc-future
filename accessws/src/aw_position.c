/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/27, create
 */

# include "aw_config.h"
# include "aw_position.h"
# include "aw_server.h"

static dict_t *dict_sub;
static rpc_clt *matchengine;
static nw_state *state_context;

struct position_key_ws {
    char        market[MARKET_NAME_MAX_LEN + 1];
    uint32_t    side;
};

static struct dict_user_key
{
    uint32_t user_id;
};

static struct sub_unit_ws {
    void *ses;
    struct position_key_ws position;
};

static struct state_data {
    uint32_t user_id;
    struct position_key_ws position;
};

static uint32_t dict_sub_hash_func(const void *key)
{
    return (uintptr_t)key;
}

static int dict_sub_key_compare(const void *key1, const void *key2)
{
    return (uintptr_t)key1 == (uintptr_t)key2 ? 0 : 1;
}

static void dict_sub_val_free(void *val)
{
    list_release(val);
}

static int list_node_compare(const void *value1, const void *value2)
{
    const struct sub_unit_ws *a = (const struct sub_unit_ws *)value1;
    const struct sub_unit_ws *b = (const struct sub_unit_ws *)value2;
    return memcmp(a, b, sizeof(struct sub_unit_ws));
}

static void *list_node_dup(void *value)
{
    struct sub_unit_ws *obj = malloc(sizeof(struct sub_unit_ws));
    memcpy(obj, value, sizeof(struct sub_unit_ws));
    return obj;
}

static void list_node_free(void *value)
{
    free(value);
}

static void on_timeout(nw_state_entry *entry)
{
    log_fatal("query position timeout, state id: %u", entry->id);
}

static void on_backend_connect(nw_ses *ses, bool result)
{
    rpc_clt *clt = ses->privdata;
    if (result) {
        log_info("connect %s:%s success", clt->name, nw_sock_human_addr(&ses->peer_addr));
    } else {
        log_info("connect %s:%s fail", clt->name, nw_sock_human_addr(&ses->peer_addr));
    }
}

static int on_position_query_reply(struct state_data *state, json_t *result)
{
    log_trace("user_id: %d ses id: tobe", state->user_id);
    struct dict_user_key key = {.user_id = state->user_id};
    dict_entry *entry = dict_find(dict_sub, &key);
    log_trace("user_entry: %p user_id: %d", (void*)entry, state->user_id);
    if (entry == NULL)
        return 0 ;

    json_t *params = json_array();
    json_array_append(params, result);

    list_t *list = entry->val;
    list_iter *iter = list_get_iterator(list, LIST_START_HEAD);
    list_node *node;
    while ((node = list_next(iter)) != NULL) {
        struct sub_unit_ws *unit = node->value;
        if (strcmp(unit->position.market, state->position.market) == 0 && unit->position.side == state->position.side) {
            log_trace("user entry: %p user_id: %d ses id: %d ", (void*)entry, state->user_id, ((nw_ses *)(unit->ses))->id);
            send_notify(unit->ses, "position.update", params);
        }
    }
    list_release_iterator(iter);
    json_decref(params);

    return 0;
}

static void on_backend_recv_pkg(nw_ses *ses, rpc_pkg *pkg)
{
    log_trace("user_id:  ses id: %d", ses->id);
    sds reply_str = sdsnewlen(pkg->body, pkg->body_size);
    log_trace("recv pkg from: %s, cmd: %u, sequence: %u, reply: %s",
            nw_sock_human_addr(&ses->peer_addr), pkg->command, pkg->sequence, reply_str);
    nw_state_entry *entry = nw_state_get(state_context, pkg->sequence);
    if (entry == NULL) {
        sdsfree(reply_str);
        return;
    }
    struct state_data *state = entry->data;

    json_t *reply = json_loadb(pkg->body, pkg->body_size, 0, NULL);
    if (reply == NULL) {
        sds hex = hexdump(pkg->body, pkg->body_size);
        log_fatal("invalid reply from: %s, cmd: %u, reply: \n%s", nw_sock_human_addr(&ses->peer_addr), pkg->command, hex);
        sdsfree(hex);
        sdsfree(reply_str);
        nw_state_del(state_context, pkg->sequence);
        return;
    }

    json_t *error = json_object_get(reply, "error");
    json_t *result = json_object_get(reply, "result");
    if (error == NULL || !json_is_null(error) || result == NULL) {
        log_error("error reply from: %s, cmd: %u, reply: %s", nw_sock_human_addr(&ses->peer_addr), pkg->command, reply_str);
        sdsfree(reply_str);
        json_decref(reply);
        nw_state_del(state_context, pkg->sequence);
        return;
    }

    int ret;
    switch (pkg->command) {
    case CMD_POSITION_QUERY:
        ret = on_position_query_reply(state, result);
        if (ret < 0) {
            log_error("on_position_query_reply fail: %d, reply: %s", ret, reply_str);
        }
        break;
    default:
        log_error("recv unknown command: %u from: %s", pkg->command, nw_sock_human_addr(&ses->peer_addr));
        break;
    }
    
    sdsfree(reply_str);
    json_decref(reply);
    nw_state_del(state_context, pkg->sequence);
}

static uint32_t dict_user_hash_function(const void *key)
{
    const struct dict_user_key *obj = key;
    return obj->user_id;
}

static int dict_user_key_compare(const void *key1, const void *key2)
{
    const struct dict_user_key *obj1 = key1;
    const struct dict_user_key *obj2 = key2;
    if (obj1->user_id == obj2->user_id)
    {
        return 0;
    }
    return 1;
}

int init_position(void)
{
    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_user_hash_function;
    dt.key_compare = dict_user_key_compare;
    dt.val_destructor = dict_sub_val_free;

    dict_sub = dict_create(&dt, 1024);
    if (dict_sub == NULL)
        return -__LINE__;

    rpc_clt_type ct;
    memset(&ct, 0, sizeof(ct));
    ct.on_connect = on_backend_connect;
    ct.on_recv_pkg = on_backend_recv_pkg;

    matchengine = rpc_clt_create(&settings.matchengine, &ct);
    if (matchengine == NULL)
        return -__LINE__;
    if (rpc_clt_start(matchengine) < 0)
        return -__LINE__;

    nw_state_type st;
    memset(&st, 0, sizeof(st));
    st.on_timeout = on_timeout;

    state_context = nw_state_create(&st, sizeof(struct state_data));
    if (state_context == NULL)
        return -__LINE__;

    return 0;
}

int position_subscribe(uint32_t user_id, nw_ses *ses, const char *market, uint32_t side)
{
    struct dict_user_key *key = (struct dict_user_key*)malloc(sizeof(struct dict_user_key));
    key->user_id = user_id;
    dict_entry *entry = dict_find(dict_sub, key);
    log_trace("user_entry: %p user_id: %d ses id: %d %p", (void*)entry, user_id, ses->id, (void*)ses);
    if (entry == NULL) {
        list_type lt;
        memset(&lt, 0, sizeof(lt));
        lt.dup = list_node_dup;
        lt.free = list_node_free;
        lt.compare = list_node_compare;
        list_t *list = list_create(&lt);
        if (list == NULL)
            return -__LINE__;
        entry = dict_add(dict_sub, key, list);
        if (entry == NULL)
            return -__LINE__;
    }

    list_t *list = entry->val;
    struct sub_unit_ws unit;
    memset(&unit, 0, sizeof(struct sub_unit_ws));
    unit.ses = ses;
    strncpy(unit.position.market, market, ASSET_NAME_MAX_LEN - 1);
    unit.position.side = side;

    if (list_find(list, &unit) != NULL){
        // list_node * node = list_find(list, &unit);
        return 0;
    }
    if (list_add_node_tail(list, &unit) == NULL)
        return -__LINE__;

    return 0;
}

int position_unsubscribe(uint32_t user_id, nw_ses *ses)
{
    struct dict_user_key *key = (struct dict_user_key*)malloc(sizeof(struct dict_user_key));
    key->user_id = user_id;
    dict_entry *entry = dict_find(dict_sub, key);
    log_trace("user_entry: %p user_id: %d ses id: %d", (void*)entry, user_id, ses->id);
    if (entry == NULL)
        return 0;

    list_t *list = entry->val;

    list_iter *iter;
    list_node *node;
    int flag = 0;
    while(!flag){
        iter = list_get_iterator(list, LIST_START_HEAD);
        while ((node = list_next(iter)) != NULL) {
            struct sub_unit_ws *unit = node->value;
            if (unit->ses == (void*)ses) {
                list_del(list, node);
                break;
            }
        }
        flag = 1;
        list_release_iterator(iter);
    }
    if (list->len == 0) {
        dict_delete(dict_sub, key);
    }

    return 0;
}

int position_on_update(uint32_t user_id, const char *market, uint32_t side)
{
    struct dict_user_key *key = (struct dict_user_key*)malloc(sizeof(struct dict_user_key));
    key->user_id = user_id;
    dict_entry *entry = dict_find(dict_sub, key);
    log_trace("user_entry: %p user_id: %d", (void*)entry, user_id);
    if (entry == NULL)
        return 0;
    log_trace("user_id: %d ses tobe", user_id);
    bool notify = false;
    list_t *list = entry->val;
    list_iter *iter = list_get_iterator(list, LIST_START_HEAD);
    list_node *node;
    while ((node = list_next(iter)) != NULL) {
        struct sub_unit_ws *unit = node->value;
        if (strcmp(unit->position.market, market) == 0 && unit->position.side == side) {
            log_trace("user_id: %d ses %d %p %s, %s, %d, %d", user_id, ((nw_ses *)(unit->ses))->id, (void*)unit->ses, unit->position.market, market, unit->position.side, side );
            notify = true;
            break;
        }
    }
    list_release_iterator(iter);
    if (!notify)
        return 0;

    json_t *trade_params = json_array();
    json_array_append_new(trade_params, json_integer(user_id));
    json_array_append_new(trade_params, json_string(market));
    json_array_append_new(trade_params, json_integer(side));

    nw_state_entry *state_entry = nw_state_add(state_context, settings.backend_timeout, 0);
    struct state_data *state = state_entry->data;
    state->user_id = user_id;
    strncpy(state->position.market, market, ASSET_NAME_MAX_LEN - 1);
    state->position.side = side;

    rpc_pkg pkg;
    memset(&pkg, 0, sizeof(pkg));
    pkg.pkg_type  = RPC_PKG_TYPE_REQUEST;
    pkg.command   = CMD_POSITION_QUERY;
    pkg.sequence  = state_entry->id;
    pkg.body      = json_dumps(trade_params, 0);
    pkg.body_size = strlen(pkg.body);
    log_trace("user_id: %d ses tobe", user_id);
    rpc_clt_send(matchengine, &pkg);
    log_trace("send request to %s, cmd: %u, sequence: %u, params: %s",
            nw_sock_human_addr(rpc_clt_peer_addr(matchengine)), pkg.command, pkg.sequence, (char *)pkg.body);
    free(pkg.body);
    json_decref(trade_params);

    return 0;
}
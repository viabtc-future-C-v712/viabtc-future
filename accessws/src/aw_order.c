/*
 * Description:
 *     History: yang@haipo.me, 2017/04/27, create
 */

# include "aw_config.h"
# include "aw_order.h"
# include "aw_server.h"

#define ORDER_SUB_TYPE_MARKET 1
#define ORDER_SUB_TYPE_ALL 2

static dict_t *dict_sub;

struct sub_unit {
    void *ses;
    char market[MARKET_NAME_MAX_LEN];
    int type;
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
    return memcmp(value1, value2, sizeof(struct sub_unit));
}

static void *list_node_dup(void *value)
{
    struct sub_unit *obj = malloc(sizeof(struct sub_unit));
    memcpy(obj, value, sizeof(struct sub_unit));
    return obj;
}

static void list_node_free(void *value)
{
    free(value);
}

int init_order(void)
{
    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_sub_hash_func;
    dt.key_compare = dict_sub_key_compare;
    dt.val_destructor = dict_sub_val_free;

    dict_sub = dict_create(&dt, 1024);
    if (dict_sub == NULL)
        return -__LINE__;

    return 0;
}

int order_subscribe(uint32_t user_id, nw_ses *ses, const char *market)
{
    void *key = (void *)(uintptr_t)user_id;
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
    struct sub_unit unit;
    memset(&unit, 0, sizeof(unit));
    unit.ses = ses;
    if (market) {
        strncpy(unit.market, market, MARKET_NAME_MAX_LEN - 1);
        unit.type = ORDER_SUB_TYPE_MARKET;
    } else {
        unit.type = ORDER_SUB_TYPE_ALL;
    }

    if (list_find(list, &unit) != NULL)
        return 0;
    if (list_add_node_tail(list, &unit) == NULL)
        return -__LINE__;

    return 0;
}

int order_unsubscribe(uint32_t user_id, nw_ses *ses)
{
    void *key = (void *)(uintptr_t)user_id;
    dict_entry *entry = dict_find(dict_sub, key);
    log_trace("user_entry: %p user_id: %d ses id: %d", (void*)entry, user_id, ses->id);
    if (entry == NULL)
        return 0;

    list_t *list = entry->val;

    list_iter *iter;
    list_node *node;
    int flag = 0;
    while(!flag){
        flag = 1;
        list_iter *iter = list_get_iterator(list, LIST_START_HEAD);
        while ((node = list_next(iter)) != NULL) {
            struct sub_unit *unit = node->value;
            if (unit->ses == (void*)ses) {
                list_del(list, node);
                flag = 0;
                break;
            }
        }
        list_release_iterator(iter);
    }

    if (list->len == 0) {
        dict_delete(dict_sub, key);
    }

    return 0;
}

int order_on_update(uint32_t user_id, int event, json_t *order)
{
    log_trace("user_id: %d", user_id);
    const char *market = json_string_value(json_object_get(order, "market"));
    if (market == NULL)
        return -__LINE__;

    void *key = (void *)(uintptr_t)user_id;
    dict_entry *entry = dict_find(dict_sub, key);
    if (entry == NULL)
        return 0;
    log_trace("user_id: %d", user_id);
    json_t *params = json_array();
    json_array_append_new(params, json_integer(event));
    json_array_append(params, order);

    list_t *list = entry->val;
    list_iter *iter = list_get_iterator(list, LIST_START_HEAD);
    list_node *node;
    while ((node = list_next(iter)) != NULL) {
        struct sub_unit *unit = node->value;
        if (unit->type == ORDER_SUB_TYPE_ALL) {
            log_trace("user_id: %d ses %d %p %s", user_id, ((nw_ses *)(unit->ses))->id, (void*)unit->ses, unit->market);
            send_notify(unit->ses, "order.update", params);
        } else {
            log_trace("market: %s", market);
            if (strcmp(unit->market, market) == 0) {
                log_trace("user_id: %d ses %d %p %s", user_id, ((nw_ses *)(unit->ses))->id, (void*)unit->ses, unit->market);
                send_notify(unit->ses, "order.update", params);
            }
        }
    }
    list_release_iterator(iter);
    json_decref(params);

    return 0;
}


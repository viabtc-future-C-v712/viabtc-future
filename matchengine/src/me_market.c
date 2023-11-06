/*
 * Description:
 *     History: yang@haipo.me, 2017/03/16, create
 */

#include "me_config.h"
#include "me_market.h"
#include "me_balance.h"
#include "me_position.h"
#include "me_deal.h"
#include "me_history.h"
#include "me_message.h"
#include "me_trade.h"

uint64_t order_id_start;
uint64_t deals_id_start;
dict_t *user_orders;

struct dict_user_key
{
    uint32_t user_id;
};

struct dict_order_key
{
    uint64_t order_id;
};

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

static void *dict_user_key_dup(const void *key)
{
    struct dict_user_key *obj = malloc(sizeof(struct dict_user_key));
    memcpy(obj, key, sizeof(struct dict_user_key));
    return obj;
}

static void dict_user_key_free(void *key)
{
    free(key);
}

static void dict_user_val_free(void *key)
{
    skiplist_release(key);
}

static uint32_t dict_order_hash_function(const void *key)
{
    return dict_generic_hash_function(key, sizeof(struct dict_order_key));
}

static int dict_order_key_compare(const void *key1, const void *key2)
{
    const struct dict_order_key *obj1 = key1;
    const struct dict_order_key *obj2 = key2;
    if (obj1->order_id == obj2->order_id)
    {
        return 0;
    }
    return 1;
}

static void *dict_order_key_dup(const void *key)
{
    struct dict_order_key *obj = malloc(sizeof(struct dict_order_key));
    memcpy(obj, key, sizeof(struct dict_order_key));
    return obj;
}

static void dict_order_key_free(void *key)
{
    free(key);
}

static int order_match_compare(const void *value1, const void *value2)
{
    const order_t *order1 = value1;
    const order_t *order2 = value2;

    if (order1->id == order2->id)
    {
        return 0;
    }
    if (order1->type != order2->type)
    {
        return 1;
    }

    int cmp;
    if (order1->side == 1 && order1->oper_type == 1 || order1->side == 2 && order1->oper_type == 2)
    {
        cmp = mpd_cmp(order1->price, order2->price, &mpd_ctx);
    }
    else
    {
        cmp = mpd_cmp(order2->price, order1->price, &mpd_ctx);
    }
    if (cmp != 0)
    {
        return cmp;
    }

    return order1->id > order2->id ? 1 : -1;
}

static int order_id_compare(const void *value1, const void *value2)
{
    const order_t *order1 = value1;
    const order_t *order2 = value2;
    if (order1->id == order2->id)
    {
        return 0;
    }

    return order1->id > order2->id ? -1 : 1;
}

int get_mm_order_type_by_source(const char *source)
{
    if (!source)
        return 0;

    if (strncmp(source, MM_SOURCE_STR_CANCEL, MM_SOURCE_STR_CANCEL_LEN) == 0)
        return MM_ORDER_TYPE_CANCEL;
    else if (strncmp(source, MM_SOURCE_STR, MM_SOURCE_STR_LEN) == 0)
        return MM_ORDER_TYPE_NORMAL;

    return 0;
}

void print_mm_order_info(const char *market, order_t *maker, order_t *taker)
{
    if (strncmp(market, "BTC_USDT", strlen("BTC_USDT")) == 0)
    {
        char *p_str = mpd_to_sci(maker->price, 0);
        char *a_str = mpd_to_sci(maker->amount, 0);
        char *t_p_str = mpd_to_sci(taker->price, 0);
        char *t_a_str = mpd_to_sci(taker->amount, 0);
        if (p_str && a_str && t_p_str && t_a_str)
        {
            log_info("type order price match %lf, maker id %" PRIu64 ", uid %u, price %s, amount %s, mm %d, source %s, taker id %" PRIu64 ", uid %u, price %s, amount %s, mm %d, source %s",
                     maker->update_time, maker->id, maker->user_id, p_str, a_str, maker->mm, maker->source, taker->id, taker->user_id, t_p_str, t_a_str, taker->mm, taker->source);
        }
        if (p_str)
            free(p_str);
        if (a_str)
            free(a_str);
        if (t_p_str)
            free(t_p_str);
        if (t_a_str)
            free(t_a_str);
    }
}

static void order_free(order_t *order)
{
    mpd_del(order->price);
    mpd_del(order->amount);
    mpd_del(order->taker_fee);
    mpd_del(order->maker_fee);
    mpd_del(order->left);
    mpd_del(order->freeze);
    mpd_del(order->deal_stock);
    mpd_del(order->deal_money);
    mpd_del(order->deal_fee);
    free(order->market);
    // free(order->source);
    free(order);
}

json_t *get_order_info(order_t *order)
{
    json_t *info = json_object();
    json_object_set_new(info, "id", json_integer(order->id));
    json_object_set_new(info, "market", json_string(order->market));
    json_object_set_new(info, "source", json_string(order->source));
    json_object_set_new(info, "type", json_integer(order->type));
    json_object_set_new(info, "isblast", json_integer(order->isblast));
    json_object_set_new(info, "side", json_integer(order->side));
    json_object_set_new(info, "user", json_integer(order->user_id));
    json_object_set_new(info, "ctime", json_real(order->create_time));
    json_object_set_new(info, "mtime", json_real(order->update_time));

    json_object_set_new(info, "oper_type", json_integer(order->oper_type));

    json_object_set_new_mpd(info, "price", order->price);
    json_object_set_new_mpd(info, "leverage", order->leverage);
    json_object_set_new_mpd(info, "trigger", order->trigger);
    json_object_set_new_mpd(info, "current_price", order->current_price);
    json_object_set_new_mpd(info, "amount", order->amount);
    json_object_set_new_mpd(info, "taker_fee", order->taker_fee);
    json_object_set_new_mpd(info, "maker_fee", order->maker_fee);
    json_object_set_new_mpd(info, "left", order->left);
    json_object_set_new_mpd(info, "deal_stock", order->deal_stock);
    json_object_set_new_mpd(info, "deal_money", order->deal_money);
    json_object_set_new_mpd(info, "deal_fee", order->deal_fee);

    return info;
}

// for mm
json_t *get_order_info_brief(order_t *order)
{
    json_t *info = json_object();
    json_object_set_new(info, "id", json_integer(order->id));
    json_object_set_new(info, "side", json_integer(order->side));
    json_object_set_new(info, "ctime", json_real(order->create_time));

    json_object_set_new_mpd(info, "price", order->price);
    json_object_set_new_mpd(info, "amount", order->amount);

    return info;
}

static int order_put(market_t *m, order_t *order)
{
    log_trace("%s %d", __FUNCTION__, order->type);
    if (order->type != MARKET_ORDER_TYPE_LIMIT)
        return -__LINE__;

    struct dict_order_key order_key = {.order_id = order->id};
    if (dict_add(m->orders, &order_key, order) == NULL)
        return -__LINE__;

    struct dict_user_key user_key = {.user_id = order->user_id};
    dict_entry *entry = dict_find(m->users, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        if (skiplist_insert(order_list, order) == NULL)
            return -__LINE__;
    }
    else
    {
        skiplist_type type;
        memset(&type, 0, sizeof(type));
        type.compare = order_id_compare;
        skiplist_t *order_list = skiplist_create(&type);
        if (order_list == NULL)
            return -__LINE__;
        if (skiplist_insert(order_list, order) == NULL)
            return -__LINE__;
        if (dict_add(m->users, &user_key, order_list) == NULL)
            return -__LINE__;
    }

    if (!order->mm)
    {
        entry = dict_find(user_orders, &user_key);
        if (entry)
        {
            skiplist_t *order_list = entry->val;
            if (skiplist_insert(order_list, order) == NULL)
                return -__LINE__;
        }
        else
        {
            skiplist_type type;
            memset(&type, 0, sizeof(type));
            type.compare = order_id_compare;
            skiplist_t *order_list = skiplist_create(&type);
            if (order_list == NULL)
                return -__LINE__;
            if (skiplist_insert(order_list, order) == NULL)
                return -__LINE__;
            if (dict_add(user_orders, &user_key, order_list) == NULL)
                return -__LINE__;
        }
    }

    if (order->side == MARKET_ORDER_SIDE_ASK)
    {
        if (skiplist_insert(m->asks, order) == NULL)
            return -__LINE__;
        mpd_copy(order->freeze, order->left, &mpd_ctx);
        if (balance_freeze(order->user_id, m->stock, order->left) == NULL)
            return -__LINE__;
    }
    else
    {
        if (skiplist_insert(m->bids, order) == NULL)
            return -__LINE__;
        mpd_t *result = mpd_new(&mpd_ctx);
        mpd_mul(result, order->price, order->left, &mpd_ctx);
        mpd_copy(order->freeze, result, &mpd_ctx);
        if (balance_freeze(order->user_id, m->money, result) == NULL)
        {
            mpd_del(result);
            return -__LINE__;
        }
        mpd_del(result);
    }

    return 0;
}

static int order_put_future(market_t *m, order_t *order)
{
    struct dict_order_key order_key = {.order_id = order->id};
    if (dict_add(m->orders, &order_key, order) == NULL)
        return -__LINE__;
    struct dict_user_key user_key = {.user_id = order->user_id};
    dict_entry *entry = dict_find(m->users, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        if (skiplist_insert(order_list, order) == NULL)
            return -__LINE__;
    }
    else
    {
        skiplist_type type;
        memset(&type, 0, sizeof(type));
        type.compare = order_id_compare;
        skiplist_t *order_list = skiplist_create(&type);
        if (order_list == NULL)
            return -__LINE__;
        if (skiplist_insert(order_list, order) == NULL)
            return -__LINE__;
        if (dict_add(m->users, &user_key, order_list) == NULL)
            return -__LINE__;
    }
    if (!order->mm)
    {
        entry = dict_find(user_orders, &user_key);
        if (entry)
        {
            skiplist_t *order_list = entry->val;
            if (skiplist_insert(order_list, order) == NULL)
                return -__LINE__;
        }
        else
        {
            skiplist_type type;
            memset(&type, 0, sizeof(type));
            type.compare = order_id_compare;
            skiplist_t *order_list = skiplist_create(&type);
            if (order_list == NULL)
                return -__LINE__;
            if (skiplist_insert(order_list, order) == NULL)
                return -__LINE__;
            if (dict_add(user_orders, &user_key, order_list) == NULL)
                return -__LINE__;
        }
    }
    if (order->side == 1 && order->oper_type == 1 || order->side == 2 && order->oper_type == 2)
    {
        if (order->type == 1)
        {
            if (skiplist_insert(m->asks, order) == NULL)
                return -__LINE__;
        }
        else
        {
            if (skiplist_insert(m->plan_asks, order) == NULL)
                return -__LINE__;
        }
    }
    else
    {
        if (order->type == 1)
        {
            if (skiplist_insert(m->bids, order) == NULL)
                return -__LINE__;
        }
        else
        {
            if (skiplist_insert(m->plan_bids, order) == NULL)
                return -__LINE__;
        }
    }
    return 0;
}

static int order_finish(bool real, market_t *m, order_t *order)
{
    if (order->side == MARKET_ORDER_SIDE_ASK)
    {
        skiplist_node *node = skiplist_find(m->asks, order);
        if (node)
        {
            skiplist_delete(m->asks, node);
        }
        if (mpd_cmp(order->freeze, mpd_zero, &mpd_ctx) > 0)
        {
            if (balance_unfreeze(order->user_id, m->stock, order->freeze) == NULL)
            {
                return -__LINE__;
            }
        }
    }
    else
    {
        skiplist_node *node = skiplist_find(m->bids, order);
        if (node)
        {
            skiplist_delete(m->bids, node);
        }
        if (mpd_cmp(order->freeze, mpd_zero, &mpd_ctx) > 0)
        {
            if (balance_unfreeze(order->user_id, m->money, order->freeze) == NULL)
            {
                return -__LINE__;
            }
        }
    }

    struct dict_order_key order_key = {.order_id = order->id};
    dict_delete(m->orders, &order_key);

    struct dict_user_key user_key = {.user_id = order->user_id};
    dict_entry *entry = dict_find(m->users, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        skiplist_node *node = skiplist_find(order_list, order);
        if (node)
        {
            skiplist_delete(order_list, node);
        }
    }

    entry = dict_find(user_orders, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        skiplist_node *node = skiplist_find(order_list, order);
        if (node)
        {
            skiplist_delete(order_list, node);
        }
    }

    if (real)
    {
        if (mpd_cmp(order->deal_stock, mpd_zero, &mpd_ctx) > 0)
        {
            if (!order->mm)
            {
                int ret = append_order_history(order);
                if (ret < 0)
                {
                    log_fatal("append_order_history fail: %d, order: %" PRIu64 "", ret, order->id);
                }
            }
        }
    }

    order_free(order);
    return 0;
}

int user_orders_list_create()
{
    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_user_hash_function;
    dt.key_compare = dict_user_key_compare;
    dt.key_dup = dict_user_key_dup;
    dt.key_destructor = dict_user_key_free;
    dt.val_destructor = dict_user_val_free;

    user_orders = dict_create(&dt, 1024);
    if (user_orders == NULL)
        return -__LINE__;

    return 0;
}

market_t *market_create(struct market *conf)
{
    if (!asset_exist(conf->stock) || !asset_exist(conf->money))
        return NULL;
    if (conf->stock_prec + conf->money_prec > asset_prec(conf->money))
        return NULL;
    if (conf->stock_prec + conf->fee_prec > asset_prec(conf->stock))
        return NULL;
    if (conf->money_prec + conf->fee_prec > asset_prec(conf->money))
        return NULL;

    market_t *m = malloc(sizeof(market_t));
    memset(m, 0, sizeof(market_t));
    m->name = strdup(conf->name);
    m->stock = strdup(conf->stock);
    m->money = strdup(conf->money);
    m->stock_prec = conf->stock_prec;
    m->money_prec = conf->money_prec;
    m->fee_prec = conf->fee_prec;
    m->min_amount = mpd_qncopy(conf->min_amount);
    m->latestPrice = mpd_new(&mpd_ctx);
    mpd_copy(m->latestPrice, mpd_zero, &mpd_ctx);
    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_user_hash_function;
    dt.key_compare = dict_user_key_compare;
    dt.key_dup = dict_user_key_dup;
    dt.key_destructor = dict_user_key_free;
    dt.val_destructor = dict_user_val_free;

    m->users = dict_create(&dt, 1024);
    if (m->users == NULL)
        return NULL;

    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_order_hash_function;
    dt.key_compare = dict_order_key_compare;
    dt.key_dup = dict_order_key_dup;
    dt.key_destructor = dict_order_key_free;

    m->orders = dict_create(&dt, 1024);
    if (m->orders == NULL)
        return NULL;

    skiplist_type lt;
    memset(&lt, 0, sizeof(lt));
    lt.compare = order_match_compare;

    m->asks = skiplist_create(&lt);
    m->bids = skiplist_create(&lt);
    m->plan_asks = skiplist_create(&lt);
    m->plan_bids = skiplist_create(&lt);
    if (m->asks == NULL || m->bids == NULL || m->plan_asks == NULL || m->plan_bids == NULL)
        return NULL;

    return m;
}

static int append_balance_trade_add(order_t *order, const char *asset, mpd_t *change, mpd_t *price, mpd_t *amount, order_t *deal_order)
{
    json_t *detail = json_object();
    json_object_set_new(detail, "m", json_string(order->market));
    json_object_set_new(detail, "i", json_integer(order->id));
    json_object_set_new_mpd(detail, "p", price);
    json_object_set_new_mpd(detail, "a", amount);
    json_object_set_new(detail, "du", json_integer(deal_order->user_id));
    json_object_set_new(detail, "di", json_integer(deal_order->id));
    char *detail_str = json_dumps(detail, JSON_SORT_KEYS);
    int ret = append_user_balance_history(order->update_time, order->user_id, asset, "trade", change, detail_str);
    free(detail_str);
    json_decref(detail);
    return ret;
}

static int append_balance_trade_sub(order_t *order, const char *asset, mpd_t *change, mpd_t *price, mpd_t *amount, order_t *deal_order)
{
    json_t *detail = json_object();
    json_object_set_new(detail, "m", json_string(order->market));
    json_object_set_new(detail, "i", json_integer(order->id));
    json_object_set_new_mpd(detail, "p", price);
    json_object_set_new_mpd(detail, "a", amount);
    json_object_set_new(detail, "du", json_integer(deal_order->user_id));
    json_object_set_new(detail, "di", json_integer(deal_order->id));
    char *detail_str = json_dumps(detail, JSON_SORT_KEYS);
    mpd_t *real_change = mpd_new(&mpd_ctx);
    mpd_copy_negate(real_change, change, &mpd_ctx);
    int ret = append_user_balance_history(order->update_time, order->user_id, asset, "trade", real_change, detail_str);
    mpd_del(real_change);
    free(detail_str);
    json_decref(detail);
    return ret;
}

static int append_balance_trade_fee(order_t *order, const char *asset, mpd_t *change, mpd_t *price, mpd_t *amount, mpd_t *fee_rate, order_t *deal_order)
{
    json_t *detail = json_object();
    json_object_set_new(detail, "m", json_string(order->market));
    json_object_set_new(detail, "i", json_integer(order->id));
    json_object_set_new_mpd(detail, "p", price);
    json_object_set_new_mpd(detail, "a", amount);
    json_object_set_new_mpd(detail, "f", fee_rate);
    json_object_set_new(detail, "du", json_integer(deal_order->user_id));
    json_object_set_new(detail, "di", json_integer(deal_order->id));
    char *detail_str = json_dumps(detail, JSON_SORT_KEYS);
    mpd_t *real_change = mpd_new(&mpd_ctx);
    mpd_copy_negate(real_change, change, &mpd_ctx);
    int ret = append_user_balance_history(order->update_time, order->user_id, asset, "trade", real_change, detail_str);
    mpd_del(real_change);
    free(detail_str);
    json_decref(detail);
    return ret;
}

static int execute_limit_ask_order(bool real, market_t *m, order_t *taker)
{
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);
    mpd_t *deal = mpd_new(&mpd_ctx);
    mpd_t *ask_fee = mpd_new(&mpd_ctx);
    mpd_t *bid_fee = mpd_new(&mpd_ctx);
    bool save_db = true;

    skiplist_node *node;
    skiplist_iter *iter = skiplist_get_iterator(m->bids);
    while ((node = skiplist_next(iter)) != NULL)
    {
        if (mpd_cmp(taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            break;
        }

        order_t *maker = node->value;
        if (mpd_cmp(taker->price, maker->price, &mpd_ctx) > 0)
        {
            break;
        }

        if (taker->mm > 0 && maker->mm == MM_ORDER_TYPE_CANCEL)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            // print_cancel_mm_order_info(maker);
            order_finish(real, m, maker);
            continue;
        }

        mpd_copy(price, maker->price, &mpd_ctx);
        if (mpd_cmp(taker->left, maker->left, &mpd_ctx) < 0)
        {
            mpd_copy(amount, taker->left, &mpd_ctx);
        }
        else
        {
            mpd_copy(amount, maker->left, &mpd_ctx);
        }

        mpd_mul(deal, price, amount, &mpd_ctx);
        mpd_mul(ask_fee, deal, taker->taker_fee, &mpd_ctx);
        mpd_mul(bid_fee, amount, maker->maker_fee, &mpd_ctx);

        save_db = true;
        if (taker->mm && maker->mm)
        {
            save_db = false;
        }

        taker->update_time = maker->update_time = current_timestamp();
        uint64_t deal_id = ++deals_id_start;
        if (real)
        {
            if (save_db)
                append_order_deal_history(taker->update_time, deal_id, taker, MARKET_ROLE_TAKER, maker, MARKET_ROLE_MAKER, price, amount, deal, ask_fee, bid_fee);
            push_deal_message(taker->update_time, m->name, taker, maker, price, amount, ask_fee, bid_fee, MARKET_ORDER_SIDE_ASK, deal_id, m->stock, m->money);
            print_mm_order_info(m->name, maker, taker);
        }

        mpd_sub(taker->left, taker->left, amount, &mpd_ctx);
        mpd_add(taker->deal_stock, taker->deal_stock, amount, &mpd_ctx);
        mpd_add(taker->deal_money, taker->deal_money, deal, &mpd_ctx);
        mpd_add(taker->deal_fee, taker->deal_fee, ask_fee, &mpd_ctx);

        balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_sub(taker, m->stock, amount, price, amount, maker);
        }
        balance_add(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_add(taker, m->money, deal, price, amount, maker);
        }
        if (mpd_cmp(ask_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, ask_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(taker, m->money, ask_fee, price, amount, taker->taker_fee, maker);
            }
        }

        mpd_sub(maker->left, maker->left, amount, &mpd_ctx);
        mpd_sub(maker->freeze, maker->freeze, deal, &mpd_ctx);
        mpd_add(maker->deal_stock, maker->deal_stock, amount, &mpd_ctx);
        mpd_add(maker->deal_money, maker->deal_money, deal, &mpd_ctx);
        mpd_add(maker->deal_fee, maker->deal_fee, bid_fee, &mpd_ctx);

        balance_sub(maker->user_id, BALANCE_TYPE_FREEZE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_sub(maker, m->money, deal, price, amount, taker);
        }
        balance_add(maker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_add(maker, m->stock, amount, price, amount, taker);
        }
        if (mpd_cmp(bid_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(maker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, bid_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(maker, m->stock, bid_fee, price, amount, maker->maker_fee, taker);
            }
        }

        if (mpd_cmp(maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            order_finish(real, m, maker);
        }
        else
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_UPDATE, maker, m);
            }
        }
    }
    skiplist_release_iterator(iter);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(deal);
    mpd_del(ask_fee);
    mpd_del(bid_fee);

    return 0;
}

static int execute_limit_bid_order(bool real, market_t *m, order_t *taker)
{
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);
    mpd_t *deal = mpd_new(&mpd_ctx);
    mpd_t *ask_fee = mpd_new(&mpd_ctx);
    mpd_t *bid_fee = mpd_new(&mpd_ctx);
    bool save_db = true;

    skiplist_node *node;
    skiplist_iter *iter = skiplist_get_iterator(m->asks);
    while ((node = skiplist_next(iter)) != NULL)
    {
        if (mpd_cmp(taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            break;
        }

        order_t *maker = node->value;
        if (mpd_cmp(taker->price, maker->price, &mpd_ctx) < 0)
        {
            break;
        }

        if (taker->mm > 0 && maker->mm == MM_ORDER_TYPE_CANCEL)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            // print_mm_order_info(maker);
            order_finish(real, m, maker);
            continue;
        }

        mpd_copy(price, maker->price, &mpd_ctx);
        if (mpd_cmp(taker->left, maker->left, &mpd_ctx) < 0)
        {
            mpd_copy(amount, taker->left, &mpd_ctx);
        }
        else
        {
            mpd_copy(amount, maker->left, &mpd_ctx);
        }

        mpd_mul(deal, price, amount, &mpd_ctx);
        mpd_mul(ask_fee, deal, maker->maker_fee, &mpd_ctx);
        mpd_mul(bid_fee, amount, taker->taker_fee, &mpd_ctx);

        save_db = true;
        if (taker->mm && maker->mm)
        {
            save_db = false;
        }

        taker->update_time = maker->update_time = current_timestamp();
        uint64_t deal_id = ++deals_id_start;
        if (real)
        {
            if (save_db)
                append_order_deal_history(taker->update_time, deal_id, maker, MARKET_ROLE_MAKER, taker, MARKET_ROLE_TAKER, price, amount, deal, ask_fee, bid_fee);
            push_deal_message(taker->update_time, m->name, maker, taker, price, amount, ask_fee, bid_fee, MARKET_ORDER_SIDE_BID, deal_id, m->stock, m->money);
            print_mm_order_info(m->name, maker, taker);
        }

        mpd_sub(taker->left, taker->left, amount, &mpd_ctx);
        mpd_add(taker->deal_stock, taker->deal_stock, amount, &mpd_ctx);
        mpd_add(taker->deal_money, taker->deal_money, deal, &mpd_ctx);
        mpd_add(taker->deal_fee, taker->deal_fee, bid_fee, &mpd_ctx);

        balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_sub(taker, m->money, deal, price, amount, maker);
        }
        balance_add(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_add(taker, m->stock, amount, price, amount, maker);
        }
        if (mpd_cmp(bid_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, bid_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(taker, m->stock, bid_fee, price, amount, taker->taker_fee, maker);
            }
        }

        mpd_sub(maker->left, maker->left, amount, &mpd_ctx);
        mpd_sub(maker->freeze, maker->freeze, amount, &mpd_ctx);
        mpd_add(maker->deal_stock, maker->deal_stock, amount, &mpd_ctx);
        mpd_add(maker->deal_money, maker->deal_money, deal, &mpd_ctx);
        mpd_add(maker->deal_fee, maker->deal_fee, ask_fee, &mpd_ctx);

        balance_sub(maker->user_id, BALANCE_TYPE_FREEZE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_sub(maker, m->stock, amount, price, amount, taker);
        }
        balance_add(maker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_add(maker, m->money, deal, price, amount, taker);
        }
        if (mpd_cmp(ask_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(maker->user_id, BALANCE_TYPE_AVAILABLE, m->money, ask_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(maker, m->money, ask_fee, price, amount, maker->maker_fee, taker);
            }
        }

        if (mpd_cmp(maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            order_finish(real, m, maker);
        }
        else
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_UPDATE, maker, m);
            }
        }
    }
    skiplist_release_iterator(iter);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(deal);
    mpd_del(ask_fee);
    mpd_del(bid_fee);

    return 0;
}

int market_put_limit_order(bool real, json_t **result, market_t *m, uint32_t user_id, uint32_t side, mpd_t *amount, mpd_t *price, mpd_t *taker_fee, mpd_t *maker_fee, const char *source)
{
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        mpd_t *balance = balance_get(user_id, BALANCE_TYPE_AVAILABLE, m->stock);
        if (!balance || mpd_cmp(balance, amount, &mpd_ctx) < 0)
        {
            return -1;
        }
    }
    else
    {
        mpd_t *balance = balance_get(user_id, BALANCE_TYPE_AVAILABLE, m->money);
        mpd_t *require = mpd_new(&mpd_ctx);
        mpd_mul(require, amount, price, &mpd_ctx);
        if (!balance || mpd_cmp(balance, require, &mpd_ctx) < 0)
        {
            mpd_del(require);
            return -1;
        }
        mpd_del(require);
    }

    if (mpd_cmp(amount, m->min_amount, &mpd_ctx) < 0)
    {
        return -2;
    }

    order_t *order = malloc(sizeof(order_t));
    if (order == NULL)
    {
        return -__LINE__;
    }

    order->id = ++order_id_start;
    order->type = MARKET_ORDER_TYPE_LIMIT;
    order->side = side;
    order->create_time = current_timestamp();
    order->update_time = order->create_time;
    order->market = strdup(m->name);
    order->source = strdup(source);
    order->user_id = user_id;
    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);
    // order->mm           = (strncmp(order->source, MM_SOURCE_STR, MM_SOURCE_STR_LEN) == 0) ? true : false;
    order->mm = get_mm_order_type_by_source(order->source);

    mpd_copy(order->price, price, &mpd_ctx);
    mpd_copy(order->amount, amount, &mpd_ctx);
    mpd_copy(order->taker_fee, taker_fee, &mpd_ctx);
    mpd_copy(order->maker_fee, maker_fee, &mpd_ctx);
    mpd_copy(order->left, amount, &mpd_ctx);
    mpd_copy(order->freeze, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_stock, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_money, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_fee, mpd_zero, &mpd_ctx);

    int ret;
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        ret = execute_limit_ask_order(real, m, order);
    }
    else
    {
        ret = execute_limit_bid_order(real, m, order);
    }
    if (ret < 0)
    {
        log_error("execute order: %" PRIu64 " fail: %d", order->id, ret);
        order_free(order);
        return -__LINE__;
    }

    if (mpd_cmp(order->left, mpd_zero, &mpd_ctx) == 0)
    {
        if (real)
        {
            if (!order->mm)
            {
                ret = append_order_history(order);
                if (ret < 0)
                {
                    log_fatal("append_order_history fail: %d, order: %" PRIu64 "", ret, order->id);
                }
            }
            push_order_message(ORDER_EVENT_FINISH, order, m);
            *result = get_order_info(order);
        }
        order_free(order);
    }
    else
    {
        if (real)
        {
            push_order_message(ORDER_EVENT_PUT, order, m);
            *result = get_order_info(order);
        }
        ret = order_put(m, order);
        if (ret < 0)
        {
            log_fatal("order_put fail: %d, order: %" PRIu64 "", ret, order->id);
        }
    }

    return 0;
}

static int execute_market_ask_order(bool real, market_t *m, order_t *taker)
{
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);
    mpd_t *deal = mpd_new(&mpd_ctx);
    mpd_t *ask_fee = mpd_new(&mpd_ctx);
    mpd_t *bid_fee = mpd_new(&mpd_ctx);
    bool save_db = true;

    skiplist_node *node;
    skiplist_iter *iter = skiplist_get_iterator(m->bids);
    while ((node = skiplist_next(iter)) != NULL)
    {
        if (mpd_cmp(taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            break;
        }

        order_t *maker = node->value;
        if (taker->mm > 0 && maker->mm == MM_ORDER_TYPE_CANCEL)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            // print_cancel_mm_order_info(maker);
            order_finish(real, m, maker);
            continue;
        }
        mpd_copy(price, maker->price, &mpd_ctx);
        if (mpd_cmp(taker->left, maker->left, &mpd_ctx) < 0)
        {
            mpd_copy(amount, taker->left, &mpd_ctx);
        }
        else
        {
            mpd_copy(amount, maker->left, &mpd_ctx);
        }

        mpd_mul(deal, price, amount, &mpd_ctx);
        mpd_mul(ask_fee, deal, taker->taker_fee, &mpd_ctx);
        mpd_mul(bid_fee, amount, maker->maker_fee, &mpd_ctx);

        save_db = true;
        if (taker->mm && maker->mm)
        {
            save_db = false;
        }

        taker->update_time = maker->update_time = current_timestamp();
        uint64_t deal_id = ++deals_id_start;
        if (real)
        {
            if (save_db)
                append_order_deal_history(taker->update_time, deal_id, taker, MARKET_ROLE_TAKER, maker, MARKET_ROLE_MAKER, price, amount, deal, ask_fee, bid_fee);
            push_deal_message(taker->update_time, m->name, taker, maker, price, amount, ask_fee, bid_fee, MARKET_ORDER_SIDE_ASK, deal_id, m->stock, m->money);
        }

        mpd_sub(taker->left, taker->left, amount, &mpd_ctx);
        mpd_add(taker->deal_stock, taker->deal_stock, amount, &mpd_ctx);
        mpd_add(taker->deal_money, taker->deal_money, deal, &mpd_ctx);
        mpd_add(taker->deal_fee, taker->deal_fee, ask_fee, &mpd_ctx);

        balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_sub(taker, m->stock, amount, price, amount, maker);
        }
        balance_add(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_add(taker, m->money, deal, price, amount, maker);
        }
        if (mpd_cmp(ask_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, ask_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(taker, m->money, ask_fee, price, amount, taker->taker_fee, maker);
            }
        }

        mpd_sub(maker->left, maker->left, amount, &mpd_ctx);
        mpd_sub(maker->freeze, maker->freeze, deal, &mpd_ctx);
        mpd_add(maker->deal_stock, maker->deal_stock, amount, &mpd_ctx);
        mpd_add(maker->deal_money, maker->deal_money, deal, &mpd_ctx);
        mpd_add(maker->deal_fee, maker->deal_fee, bid_fee, &mpd_ctx);

        balance_sub(maker->user_id, BALANCE_TYPE_FREEZE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_sub(maker, m->money, deal, price, amount, taker);
        }
        balance_add(maker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_add(maker, m->stock, amount, price, amount, taker);
        }
        if (mpd_cmp(bid_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(maker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, bid_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(maker, m->stock, bid_fee, price, amount, maker->maker_fee, taker);
            }
        }

        if (mpd_cmp(maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            order_finish(real, m, maker);
        }
        else
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_UPDATE, maker, m);
            }
        }
    }
    skiplist_release_iterator(iter);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(deal);
    mpd_del(ask_fee);
    mpd_del(bid_fee);

    return 0;
}

static int execute_market_bid_order(bool real, market_t *m, order_t *taker)
{
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);
    mpd_t *deal = mpd_new(&mpd_ctx);
    mpd_t *ask_fee = mpd_new(&mpd_ctx);
    mpd_t *bid_fee = mpd_new(&mpd_ctx);
    mpd_t *result = mpd_new(&mpd_ctx);
    bool save_db = true;

    skiplist_node *node;
    skiplist_iter *iter = skiplist_get_iterator(m->asks);
    while ((node = skiplist_next(iter)) != NULL)
    {
        if (mpd_cmp(taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            break;
        }

        order_t *maker = node->value;
        if (taker->mm > 0 && maker->mm == MM_ORDER_TYPE_CANCEL)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            // print_cancel_mm_order_info(maker);
            order_finish(real, m, maker);
            continue;
        }
        mpd_copy(price, maker->price, &mpd_ctx);

        mpd_div(amount, taker->left, price, &mpd_ctx);
        mpd_rescale(amount, amount, -m->stock_prec, &mpd_ctx);
        while (true)
        {
            mpd_mul(result, amount, price, &mpd_ctx);
            if (mpd_cmp(result, taker->left, &mpd_ctx) > 0)
            {
                mpd_set_i32(result, -m->stock_prec, &mpd_ctx);
                mpd_pow(result, mpd_ten, result, &mpd_ctx);
                mpd_sub(amount, amount, result, &mpd_ctx);
            }
            else
            {
                break;
            }
        }

        if (mpd_cmp(amount, maker->left, &mpd_ctx) > 0)
        {
            mpd_copy(amount, maker->left, &mpd_ctx);
        }
        if (mpd_cmp(amount, mpd_zero, &mpd_ctx) == 0)
        {
            break;
        }

        mpd_mul(deal, price, amount, &mpd_ctx);
        mpd_mul(ask_fee, deal, maker->maker_fee, &mpd_ctx);
        mpd_mul(bid_fee, amount, taker->taker_fee, &mpd_ctx);

        save_db = true;
        if (taker->mm && maker->mm)
        {
            save_db = false;
        }

        taker->update_time = maker->update_time = current_timestamp();
        uint64_t deal_id = ++deals_id_start;
        if (real)
        {
            if (save_db)
                append_order_deal_history(taker->update_time, deal_id, maker, MARKET_ROLE_MAKER, taker, MARKET_ROLE_TAKER, price, amount, deal, ask_fee, bid_fee);
            push_deal_message(taker->update_time, m->name, maker, taker, price, amount, ask_fee, bid_fee, MARKET_ORDER_SIDE_BID, deal_id, m->stock, m->money);
        }

        mpd_sub(taker->left, taker->left, deal, &mpd_ctx);
        mpd_add(taker->deal_stock, taker->deal_stock, amount, &mpd_ctx);
        mpd_add(taker->deal_money, taker->deal_money, deal, &mpd_ctx);
        mpd_add(taker->deal_fee, taker->deal_fee, bid_fee, &mpd_ctx);

        balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_sub(taker, m->money, deal, price, amount, maker);
        }
        balance_add(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_add(taker, m->stock, amount, price, amount, maker);
        }
        if (mpd_cmp(bid_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(taker->user_id, BALANCE_TYPE_AVAILABLE, m->stock, bid_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(taker, m->stock, bid_fee, price, amount, taker->taker_fee, maker);
            }
        }

        mpd_sub(maker->left, maker->left, amount, &mpd_ctx);
        mpd_sub(maker->freeze, maker->freeze, amount, &mpd_ctx);
        mpd_add(maker->deal_stock, maker->deal_stock, amount, &mpd_ctx);
        mpd_add(maker->deal_money, maker->deal_money, deal, &mpd_ctx);
        mpd_add(maker->deal_fee, maker->deal_fee, ask_fee, &mpd_ctx);

        balance_sub(maker->user_id, BALANCE_TYPE_FREEZE, m->stock, amount);
        if (real && save_db)
        {
            append_balance_trade_sub(maker, m->stock, amount, price, amount, taker);
        }
        balance_add(maker->user_id, BALANCE_TYPE_AVAILABLE, m->money, deal);
        if (real && save_db)
        {
            append_balance_trade_add(maker, m->money, deal, price, amount, taker);
        }
        if (mpd_cmp(ask_fee, mpd_zero, &mpd_ctx) > 0)
        {
            balance_sub(maker->user_id, BALANCE_TYPE_AVAILABLE, m->money, ask_fee);
            if (real && save_db)
            {
                append_balance_trade_fee(maker, m->money, ask_fee, price, amount, maker->maker_fee, taker);
            }
        }

        if (mpd_cmp(maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_FINISH, maker, m);
            }
            order_finish(real, m, maker);
        }
        else
        {
            if (real)
            {
                push_order_message(ORDER_EVENT_UPDATE, maker, m);
            }
        }
    }
    skiplist_release_iterator(iter);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(deal);
    mpd_del(ask_fee);
    mpd_del(bid_fee);
    mpd_del(result);

    return 0;
}


int check_position_order_mode(uint32_t user_id, market_t *market){
    //检查order

    skiplist_t *order_list = market_get_order_list(market, user_id);
    if(order_list){
        skiplist_iter *iter = skiplist_get_iterator(order_list);
        skiplist_node *node;
        if (iter && (node = skiplist_next(iter)) != NULL){// 如果有order存在，就不能设置
            return -1;
        }
    }
    //检查position
    if(get_position(user_id, market->name, BULL) || get_position(user_id, market->name, BEAR))// 如果有position存在，就不能设置
        return -1;
    return 0;
}

int market_put_order_common(void *args_)
{
    args_t *args = (args_t *)args_;
    // 限价单及计划委托单需要输入委托价格
    if (args->Type == 1 || args->Type == 2)
    {
        if (!args->entrustPrice || mpd_cmp(args->entrustPrice, mpd_zero, &mpd_ctx) <= 0)
        {
            args->msg = "限价单及计划委托单需要输入委托价格";
            return -1;
        }
        if (args->Type == 2)
        {
            if (!args->triggerPrice || mpd_cmp(args->triggerPrice, mpd_zero, &mpd_ctx) <= 0)
            {
                args->msg = "限价单及计划委托单需要输入委托价格";
                return -2;
            }
        }
    }
    int ret = 0;
    if (args->bOpen)
    {
        ret = market_put_order_open((void *)args);
    }
    else
    {
        ret = market_put_order_close((void *)args);
    }
    on_planner(args->real);        // 处理计划委托
    force_liquidation(args->real); // 处理爆仓
    return ret;
}

mpd_t *calcPriAmout(mpd_t *volume, mpd_t *Price, mpd_t *leverage)
{
    mpd_t *priAmout = mpd_new(&mpd_ctx);
    mpd_mul(priAmout, volume, Price, &mpd_ctx);
    mpd_t *q = mpd_new(&mpd_ctx);
    mpd_t *r = mpd_new(&mpd_ctx);
    mpd_divmod(q, r, priAmout, leverage, &mpd_ctx);
    mpd_del(priAmout);
    mpd_del(r);
    return q;
}

mpd_t *calcFee(mpd_t *volume, mpd_t *Price, mpd_t *feeRate)
{
    mpd_t *Amout = mpd_new(&mpd_ctx);
    mpd_t *fee = mpd_new(&mpd_ctx);
    mpd_mul(Amout, volume, Price, &mpd_ctx);
    mpd_mul(fee, Amout, feeRate, &mpd_ctx);
    mpd_del(Amout);
    return fee;
}

// todo: 判断用户持仓中全仓的个数
int getSumCrossCount(uint32_t user_id)
{
    int count = 0;
    for (size_t i = 0; i < settings.market_num; ++i)
    {
        position_t *position = get_position(user_id, settings.markets[i].name, 1);
        if (position && position->pattern == 2)
            count++;
        position = get_position(user_id, settings.markets[i].name, 2);
        if (position && position->pattern == 2)
            count++;
    }
    return count;
}

mpd_t *getPartPNL(position_t *position, mpd_t *latestPrice)
{
    mpd_t *positionTotal = mpd_new(&mpd_ctx);
    mpd_add(positionTotal, position->position, position->frozen, &mpd_ctx);
    mpd_t *PNL = mpd_new(&mpd_ctx);

    if (position->side == BULL) // 平多
    {
        mpd_sub(PNL, latestPrice, position->price, &mpd_ctx);
        mpd_div(PNL, PNL, position->price, &mpd_ctx);
        mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
    }
    else // 平空
    {
        mpd_sub(PNL, position->price, latestPrice, &mpd_ctx);
        mpd_div(PNL, PNL, position->price, &mpd_ctx);
        mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
    }

    mpd_del(positionTotal);
    return PNL;
}

mpd_t *getPNL(position_t *position, mpd_t *latestPrice)
{
    mpd_t *positionTotal = mpd_new(&mpd_ctx);
    mpd_add(positionTotal, position->position, position->frozen, &mpd_ctx);
    mpd_t *PNL = mpd_new(&mpd_ctx);
    if (position->side == BULL) // 开多
    {
        if (mpd_cmp(latestPrice, position->price, &mpd_ctx) >= 0)
        { // 盈利
            mpd_sub(PNL, latestPrice, position->price, &mpd_ctx);
            mpd_div(PNL, PNL, position->price, &mpd_ctx);
            mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
            mpd_add(PNL, PNL, position->principal, &mpd_ctx);
        }
        else
        { // 亏损
            mpd_sub(PNL, position->price, latestPrice, &mpd_ctx);
            mpd_div(PNL, PNL, position->price, &mpd_ctx);
            mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
            mpd_sub(PNL, position->principal, PNL, &mpd_ctx);
        }
    }
    else
    { // 开空
        if (mpd_cmp(latestPrice, position->price, &mpd_ctx) <= 0)
        { // 盈利
            mpd_sub(PNL, position->price, latestPrice, &mpd_ctx);
            mpd_div(PNL, PNL, position->price, &mpd_ctx);
            mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
            mpd_add(PNL, PNL, position->principal, &mpd_ctx);
        }
        else
        { // 亏损
            mpd_sub(PNL, latestPrice, position->price, &mpd_ctx);
            mpd_div(PNL, PNL, position->price, &mpd_ctx);
            mpd_mul(PNL, PNL, positionTotal, &mpd_ctx);
            mpd_sub(PNL, position->principal, PNL, &mpd_ctx);
        }
    }
    mpd_del(positionTotal);
    return PNL;
}

mpd_t *getSumPNL(uint32_t user_id)
{
    mpd_t *totalPNL = mpd_new(&mpd_ctx);
    mpd_copy(totalPNL, mpd_zero, &mpd_ctx);
    for (size_t i = 0; i < settings.market_num; ++i)
    {
        position_t *position = get_position(user_id, settings.markets[i].name, 1);
        if (position && position->pattern == 2)
        {
            market_t *market = get_market(settings.markets[i].name);
            mpd_t *PNL = getPNL(position, market->latestPrice);
            mpd_add(totalPNL, totalPNL, PNL, &mpd_ctx);
        }
        position = get_position(user_id, settings.markets[i].name, 2);
        if (position && position->pattern == 2)
        {
            market_t *market = get_market(settings.markets[i].name);
            mpd_t *PNL = getPNL(position, market->latestPrice);
            mpd_add(totalPNL, totalPNL, PNL, &mpd_ctx);
        }
    }
    return totalPNL;
}

int adjustOrder(deal_t *deal)
{
    mpd_t *deal_stock = mpd_new(&mpd_ctx);
    // taker order
    mpd_sub(deal->taker->left, deal->taker->left, deal->amount, &mpd_ctx);
    mpd_div(deal_stock, deal->amount, deal->price, &mpd_ctx);
    mpd_add(deal->taker->deal_stock, deal->taker->deal_stock, deal_stock, &mpd_ctx);
    mpd_div(deal->taker_priAmount, deal->amount, deal->taker->leverage, &mpd_ctx);
    mpd_add(deal->taker->deal_money, deal->taker->deal_money, deal->deal, &mpd_ctx);
    mpd_mul(deal->taker_fee, deal->amount, deal->taker->taker_fee, &mpd_ctx);
    mpd_add(deal->taker->deal_fee, deal->taker->deal_fee, deal->taker_fee, &mpd_ctx);

    // maker order
    mpd_sub(deal->maker->left, deal->maker->left, deal->amount, &mpd_ctx);
    mpd_div(deal_stock, deal->amount, deal->price, &mpd_ctx);
    mpd_add(deal->maker->deal_stock, deal->maker->deal_stock, deal_stock, &mpd_ctx);
    mpd_div(deal->maker_priAmount, deal->amount, deal->maker->leverage, &mpd_ctx);
    mpd_add(deal->maker->deal_money, deal->maker->deal_money, deal->deal, &mpd_ctx);
    mpd_mul(deal->maker_fee, deal->amount, deal->maker->maker_fee, &mpd_ctx);
    mpd_add(deal->maker->deal_fee, deal->maker->deal_fee, deal->maker_fee, &mpd_ctx);

    mpd_del(deal_stock);
    return 0;
}

// 当一个仓位有变动，全部推送，
int push_position_message(position_t *position)
{
    push_position_message_(position);
    if (position->side == BULL)
    {
        position = get_position(position->user_id, position->market, BEAR);
        if (position)
        {
            push_position_message_(position);
        }
    }
    else
    {
        position = get_position(position->user_id, position->market, BULL);
        if (position)
        {
            push_position_message_(position);
        }
    }
}

int adjustPosition(deal_t *deal)
{
    mpd_t *sum = mpd_new(&mpd_ctx);
    mpd_t *money = mpd_new(&mpd_ctx);
    mpd_t *total = mpd_new(&mpd_ctx);
    mpd_t *newPrice = mpd_new(&mpd_ctx);
    mpd_t *totalPosition = mpd_new(&mpd_ctx);
    position_t *position;
    // log_stderr("%s", mpd_to_sci(deal->price, 0));
    // taker position
    position = get_position(deal->taker->user_id, deal->taker->market, deal->taker->side);
    if (!position)
    {
        position = initPosition(deal->taker->user_id, deal->taker->market, deal->taker->pattern);
        position->side = deal->taker->side;
        mpd_copy(position->leverage, deal->taker->leverage, &mpd_ctx);
        mpd_copy(position->position, deal->amount, &mpd_ctx);
        mpd_copy(position->price, deal->price, &mpd_ctx);
        mpd_copy(position->frozen, mpd_zero, &mpd_ctx);
        mpd_div(position->principal, deal->amount, position->leverage, &mpd_ctx);
        mpd_sub(position->principal, position->principal, deal->taker_fee, &mpd_ctx);
        add_position(deal->taker->user_id, deal->taker->market, deal->taker->side, position);
        // log_stderr("%s", mpd_to_sci(position->price, 0));
    }
    else
    {
        mpd_add(totalPosition, position->frozen, position->position, &mpd_ctx);
        mpd_mul(sum, position->price, totalPosition, &mpd_ctx);

        if (deal->taker->oper_type == 1)
        {
            mpd_add(total, deal->deal, sum, &mpd_ctx);
            // 交易仓位 + 可用仓位 + 冻结仓位
            mpd_add(totalPosition, deal->amount, totalPosition, &mpd_ctx);
            mpd_add(position->principal, position->principal, deal->taker_priAmount, &mpd_ctx);
            mpd_sub(position->principal, position->principal, deal->taker_fee, &mpd_ctx);
            // 计算融合价
            mpd_div(newPrice, total, totalPosition, &mpd_ctx);

            mpd_add(position->position, position->position, deal->amount, &mpd_ctx);
            mpd_copy(position->price, newPrice, &mpd_ctx);
            // log_stderr("%s", mpd_to_sci(position->price, 0));
        }
        else
        {
            mpd_sub(total, sum, deal->deal, &mpd_ctx);
            // log_stderr("%s %s %s", mpd_to_sci(total, 0), mpd_to_sci(deal->deal, 0), mpd_to_sci(sum, 0));
            // log_stderr("%s %s %s", mpd_to_sci(totalPosition, 0), mpd_to_sci(deal->amount, 0), mpd_to_sci(totalPosition, 0));
            mpd_t *amount = mpd_new(&mpd_ctx);
            mpd_add(amount, position->position, position->frozen, &mpd_ctx);
            // 计算总权益
            deal->taker_PNL = getPNL(position, deal->price);
            // 计算部分权益
            deal->t_PNL = getPartPNL(position, deal->price);

            mpd_add(deal->taker->pnl, deal->taker->pnl, deal->t_PNL, &mpd_ctx);
            //
            // 计算交易部份权益
            mpd_mul(deal->taker_PNL, deal->taker_PNL, deal->amount, &mpd_ctx);
            mpd_div(deal->taker_PNL, deal->taker_PNL, amount, &mpd_ctx);
            // 平仓释放的保证金计算
            mpd_mul(amount, deal->amount, position->principal, &mpd_ctx);
            mpd_div(deal->taker_priAmount, amount, totalPosition, &mpd_ctx);

            mpd_del(amount);
            mpd_sub(position->principal, position->principal, deal->taker_priAmount, &mpd_ctx);

            mpd_sub(totalPosition, totalPosition, deal->amount, &mpd_ctx);
            if (mpd_cmp(totalPosition, mpd_zero, &mpd_ctx) == 0)
            {
                mpd_copy(position->frozen, mpd_zero, &mpd_ctx);
                mpd_copy(position->position, mpd_zero, &mpd_ctx);
            }
            else
            {
                // mpd_div(newPrice, total, totalPosition, &mpd_ctx);
                // 下单时已冻结仓位，这里只需减少冻结仓位
                mpd_sub(position->frozen, position->frozen, deal->amount, &mpd_ctx);
                // log_stderr("%s %s", mpd_to_sci(position->frozen, 0), mpd_to_sci(deal->amount, 0));
                // mpd_copy(position->price, newPrice, &mpd_ctx);
                // log_stderr("%s", mpd_to_sci(position->price, 0));
            }
        }
    }
    if (deal->real){
        push_position_message(position);
    }
    if (mpd_cmp(position->position, mpd_zero, &mpd_ctx) == 0 && mpd_cmp(position->frozen, mpd_zero, &mpd_ctx) == 0){
        del_position(position->user_id, position->market, position->side);
    }
    // maker position
    position = get_position(deal->maker->user_id, deal->maker->market, deal->maker->side);
    if (!position)
    {
        position = initPosition(deal->maker->user_id, deal->maker->market, deal->maker->pattern);
        position->side = deal->maker->side;
        mpd_copy(position->leverage, deal->maker->leverage, &mpd_ctx);
        mpd_copy(position->position, deal->amount, &mpd_ctx);
        mpd_copy(position->price, deal->price, &mpd_ctx);
        mpd_copy(position->frozen, mpd_zero, &mpd_ctx);
        mpd_div(position->principal, deal->amount, position->leverage, &mpd_ctx);
        mpd_sub(position->principal, position->principal, deal->maker_fee, &mpd_ctx);
        add_position(deal->maker->user_id, deal->maker->market, deal->maker->side, position);
        // log_stderr("%s", mpd_to_sci(deal->price, 0));
        // log_stderr("%s", mpd_to_sci(position->price, 0));
    }
    else
    {
        mpd_add(totalPosition, position->frozen, position->position, &mpd_ctx);
        mpd_mul(sum, position->price, totalPosition, &mpd_ctx);
        // log_stderr("%s %s %s", mpd_to_sci(sum, 0), mpd_to_sci(position->price, 0), mpd_to_sci(totalPosition, 0));
        if (deal->maker->oper_type == 1)
        {
            mpd_add(total, deal->deal, sum, &mpd_ctx);
            // log_stderr("%s %s %s", mpd_to_sci(total, 0), mpd_to_sci(deal->deal, 0), mpd_to_sci(sum, 0));
            // 交易仓位 + 可用仓位 + 冻结仓位
            mpd_add(totalPosition, deal->amount, totalPosition, &mpd_ctx);
            // log_stderr("%s %s %s", mpd_to_sci(totalPosition, 0), mpd_to_sci(deal->amount, 0), mpd_to_sci(totalPosition, 0));
            mpd_add(position->principal, position->principal, deal->maker_priAmount, &mpd_ctx);
            mpd_sub(position->principal, position->principal, deal->maker_fee, &mpd_ctx);
            // 计算融合价
            mpd_div(newPrice, total, totalPosition, &mpd_ctx);

            mpd_add(position->position, position->position, deal->amount, &mpd_ctx);
            mpd_copy(position->price, newPrice, &mpd_ctx);
            // log_stderr("%s ", mpd_to_sci(position->price, 0));
        }
        else
        {
            mpd_sub(total, sum, deal->deal, &mpd_ctx);
            mpd_t *amount = mpd_new(&mpd_ctx);
            mpd_add(amount, position->position, position->frozen, &mpd_ctx);
            // 计算总权益
            deal->maker_PNL = getPNL(position, deal->price);
            deal->m_PNL = getPartPNL(position, deal->price);
            mpd_add(deal->maker->pnl, deal->maker->pnl, deal->m_PNL, &mpd_ctx);

            // 计算交易部份权益
            mpd_mul(deal->maker_PNL, deal->maker_PNL, deal->amount, &mpd_ctx);
            mpd_div(deal->maker_PNL, deal->maker_PNL, amount, &mpd_ctx);
            // 平仓释放的保证金计算
            mpd_mul(amount, deal->amount, position->principal, &mpd_ctx);
            mpd_div(deal->maker_priAmount, amount, totalPosition, &mpd_ctx);
            mpd_del(amount);
            mpd_sub(position->principal, position->principal, deal->maker_priAmount, &mpd_ctx);

            mpd_sub(totalPosition, totalPosition, deal->amount, &mpd_ctx);
            if (mpd_cmp(totalPosition, mpd_zero, &mpd_ctx) == 0)
            {
                mpd_copy(position->frozen, mpd_zero, &mpd_ctx);
                mpd_copy(position->position, mpd_zero, &mpd_ctx);
            }
            else
            {
                // mpd_div(newPrice, total, totalPosition, &mpd_ctx);
                // maker 只可能是限价单，下单时已冻结仓位，这里只需减少冻结仓位
                mpd_sub(position->frozen, position->frozen, deal->amount, &mpd_ctx);
                // log_stderr("%s %s", mpd_to_sci(position->frozen, 0), mpd_to_sci(deal->amount, 0));
                // mpd_copy(position->price, newPrice, &mpd_ctx);
                // log_stderr("%s", mpd_to_sci(position->price, 0));
            }
        }
    }
    if (deal->real){
        push_position_message(position);
    }
    if (mpd_cmp(position->position, mpd_zero, &mpd_ctx) == 0 && mpd_cmp(position->frozen, mpd_zero, &mpd_ctx) == 0){
        del_position(position->user_id, position->market, position->side);
    }
    mpd_del(sum);
    mpd_del(money);
    mpd_del(total);
    mpd_del(newPrice);
    mpd_del(totalPosition);
    return 0;
}

int adjustBalance(deal_t *deal)
{
    // taker
    if (deal->taker->oper_type == 1)
    { // open
        log_debug("%s 余额减去 %s", __FUNCTION__, mpd_to_sci(deal->taker_priAmount, 0));
        balance_unfreeze(deal->taker->user_id, deal->market->money, deal->taker_priAmount);
        balance_sub(deal->taker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->taker_priAmount);
        mpd_sub(deal->taker->freeze, deal->taker->freeze, deal->taker_priAmount, &mpd_ctx);
    }
    else
    { // close
        log_debug("%s 余额加上权益 %s 减去费用 %s", __FUNCTION__, mpd_to_sci(deal->taker_PNL, 0), mpd_to_sci(deal->taker_fee, 0));
        balance_add(deal->taker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->taker_PNL);
        balance_sub(deal->taker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->taker_fee);
    }

    // maker
    if (deal->maker->oper_type == 1)
    { // open

        log_debug("%s 余额减去 %s", __FUNCTION__, mpd_to_sci(deal->maker_priAmount, 0));
        balance_unfreeze(deal->maker->user_id, deal->market->money, deal->maker_priAmount);
        balance_sub(deal->maker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->maker_priAmount);
        mpd_sub(deal->maker->freeze, deal->maker->freeze, deal->maker_priAmount, &mpd_ctx);
    }
    else
    { // close
        log_debug("%s 余额加上权益 %s 减去费用 %s", __FUNCTION__, mpd_to_sci(deal->maker_PNL, 0), mpd_to_sci(deal->maker_fee, 0));
        balance_add(deal->maker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->maker_PNL);
        balance_sub(deal->maker->user_id, BALANCE_TYPE_AVAILABLE, deal->market->money, deal->maker_fee);
    }
    if (deal->real)
    {
        append_balance_trade_sub(deal->taker, deal->market->money, deal->amount, deal->price, deal->amount, deal->maker);
        append_balance_trade_add(deal->taker, deal->market->money, deal->amount, deal->price, deal->amount, deal->maker);
        append_balance_trade_fee(deal->taker, deal->market->money, deal->amount, deal->price, deal->amount, deal->maker->maker_fee, deal->maker);

        append_balance_trade_sub(deal->maker, deal->market->money, deal->amount, deal->price, deal->amount, deal->taker);
        append_balance_trade_add(deal->maker, deal->market->money, deal->amount, deal->price, deal->amount, deal->taker);
        append_balance_trade_fee(deal->maker, deal->market->money, deal->amount, deal->price, deal->amount, deal->taker->taker_fee, deal->taker);
    }
    return 0;
}

int execute_order_open_imp(deal_t *deal)
{
    // log_stderr("%s", mpd_to_sci(deal->price, 0));
    // 调整order
    adjustOrder(deal);
    // 调整position
    adjustPosition(deal);
    // 调整balance
    adjustBalance(deal);

    uint64_t deal_id = ++deals_id_start;

    deal->taker->update_time = deal->maker->update_time = current_timestamp();
    if (deal->real)
    {
        if (deal->taker->side == 1 && deal->taker->oper_type == 1 || deal->taker->side == 2 && deal->taker->oper_type == 2)
        {
            // 添加记录
            append_order_deal_history_future(deal->taker->update_time, deal_id, deal->taker, MARKET_ROLE_TAKER, deal->maker, MARKET_ROLE_MAKER, deal->price, deal->amount, deal->deal, deal->taker_fee, deal->maker_fee, deal->t_PNL, deal->m_PNL);
            // 发送消息
            push_deal_message(deal->taker->update_time, deal->taker->market, deal->taker, deal->maker, deal->price, deal->amount, deal->taker_fee, deal->maker_fee, MARKET_ORDER_SIDE_ASK, deal_id, deal->market->stock, deal->market->money);
        }
        else
        {
            append_order_deal_history_future(deal->maker->update_time, deal_id, deal->maker, MARKET_ROLE_MAKER, deal->taker, MARKET_ROLE_TAKER, deal->price, deal->amount, deal->deal, deal->taker_fee, deal->maker_fee, deal->t_PNL, deal->m_PNL);
            push_deal_message(deal->maker->update_time, deal->maker->market, deal->maker, deal->taker, deal->price, deal->amount, deal->taker_fee, deal->maker_fee, MARKET_ORDER_SIDE_ASK, deal_id, deal->market->stock, deal->market->money);
        }
    }
    log_debug("%s", __FUNCTION__);
    return 0;
}

static int order_finish_future(bool real, market_t *m, order_t *order)
{
    if (order->side == 1 && order->oper_type == 1 || order->side == 2 && order->oper_type == 2)
    {
        if (order->type == MARKET_ORDER_TYPE_PLAN)
        {
            skiplist_node *node = skiplist_find(m->plan_asks, order);
            if (node)
            {
                skiplist_delete(m->plan_asks, node);
            }
        }
        else
        {
            skiplist_node *node = skiplist_find(m->asks, order);
            if (node)
            {
                skiplist_delete(m->asks, node);
            }
        }
    }
    else
    {
        if (order->type == MARKET_ORDER_TYPE_PLAN)
        {
            skiplist_node *node = skiplist_find(m->plan_bids, order);
            if (node)
            {
                skiplist_delete(m->plan_bids, node);
            }
        }
        else
        {
            skiplist_node *node = skiplist_find(m->bids, order);
            if (node)
            {
                skiplist_delete(m->bids, node);
            }
        }
    }

    struct dict_order_key order_key = {.order_id = order->id};
    dict_delete(m->orders, &order_key);

    struct dict_user_key user_key = {.user_id = order->user_id};
    dict_entry *entry = dict_find(m->users, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        skiplist_node *node = skiplist_find(order_list, order);
        if (node)
        {
            skiplist_delete(order_list, node);
        }
    }

    entry = dict_find(user_orders, &user_key);
    if (entry)
    {
        skiplist_t *order_list = entry->val;
        skiplist_node *node = skiplist_find(order_list, order);
        if (node)
        {
            skiplist_delete(order_list, node);
        }
    }

    if (real)
    {
        if (mpd_cmp(order->deal_stock, mpd_zero, &mpd_ctx) > 0)
        {
            if (!order->mm)
            {
                int ret = append_order_history(order);
                if (ret < 0)
                {
                    log_fatal("append_order_history fail: %d, order: %" PRIu64 "", ret, order->id);
                }
            }
        }
    }
    log_debug("order record %p", (void *)order);
    order_free(order);
    return 0;
}

// 撮合正式开始
int execute_order(uint32_t real, market_t *market, uint32_t direction, order_t *taker)
{
    log_trace("match start order id %d", taker->id);
    bool save_db = true;
    skiplist_node *node;
    skiplist_iter *iter;
    int bSellOrBuy = SELL;
    if (taker->oper_type == 1)
    {
        if (direction == 1)
        {
            iter = skiplist_get_iterator(market->bids);
            bSellOrBuy = SELL;
            log_debug("%s %d 开空 %d", __FUNCTION__, taker->user_id, taker->left);
        }
        else
        {
            iter = skiplist_get_iterator(market->asks);
            bSellOrBuy = BUY;
            log_debug("%s %d 开多 %d", __FUNCTION__, taker->user_id, taker->left);
        }
    }
    else
    {
        if (direction == 2)
        {
            iter = skiplist_get_iterator(market->bids);
            bSellOrBuy = SELL;
            log_debug("%s %d 平多 %d", __FUNCTION__, taker->user_id, taker->left);
        }
        else
        {
            bSellOrBuy = BUY;
            iter = skiplist_get_iterator(market->asks);
            log_debug("%s %d 平空 %d", __FUNCTION__, taker->user_id, taker->left);
        }
    }
    while ((node = skiplist_next(iter)) != NULL)
    {
        if (mpd_cmp(taker->left, mpd_zero, &mpd_ctx) == 0){
            log_debug("处理完毕");
            break;
        }
        order_t *maker = node->value;
        deal_t *deal = initDeal();

        // 判断价格
        if (taker->type == 0)
        { // taker 市价
            if (maker->type == 0)
            { // maker 市价
                continue;
            }
            else if (maker->type == 1)
            { // maker 限价
                mpd_copy(deal->price, maker->price, &mpd_ctx);
            }
        }
        else if (taker->type == 1)
        { // taker 限价
            if (maker->type == 0)
            { // maker 市价
                mpd_copy(deal->price, taker->price, &mpd_ctx);
            }
            else if (maker->type == 1)
            { // maker 限价
                if (bSellOrBuy == SELL)
                { // taker 卖出
                    if (mpd_cmp(taker->price, maker->price, &mpd_ctx) <= 0)
                    {                                                  // 卖出价要不大于买入价
                        mpd_copy(deal->price, maker->price, &mpd_ctx); // 先出价优先
                    }
                    else
                    {
                        log_debug("价格不匹配");
                        break;
                    }
                }
                else
                { // taker 买入
                    if (mpd_cmp(taker->price, maker->price, &mpd_ctx) >= 0)
                    {                                                  // 卖出价要不大于买入价
                        mpd_copy(deal->price, maker->price, &mpd_ctx); // 先出价优先
                    }
                    else
                    {
                        log_debug("价格不匹配");
                        break;
                    }
                }
            }
        }
        // 判断数量 (买入和卖出中的小者)
        if (mpd_cmp(taker->left, maker->left, &mpd_ctx) < 0)
        {
            mpd_copy(deal->amount, taker->left, &mpd_ctx);
        }
        else
        {
            mpd_copy(deal->amount, maker->left, &mpd_ctx);
        }
        deal->maker = maker;
        deal->taker = taker;
        deal->real = real;
        deal->market = market;
        mpd_copy(market->latestPrice, deal->price, &mpd_ctx);
        mpd_mul(deal->deal, deal->price, deal->amount, &mpd_ctx);
        log_debug("deal price:%s", mpd_to_sci(deal->price, 0));
        log_debug("deal amount:%s", mpd_to_sci(deal->amount, 0));
        execute_order_open_imp(deal);
        if (mpd_cmp(deal->taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
                push_order_message(ORDER_EVENT_FINISH, deal->taker, market);
        }
        else
        {
            if (real)
                push_order_message(ORDER_EVENT_UPDATE, deal->taker, market);
        }
        if (mpd_cmp(deal->maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            if (real)
                push_order_message(ORDER_EVENT_FINISH, deal->maker, market);
        }
        else
        {
            if (real)
                push_order_message(ORDER_EVENT_UPDATE, deal->maker, market);
        }
        log_debug("%s %d", __FUNCTION__, direction);
        // 清理处理完毕的order
        if (mpd_cmp(deal->maker->left, mpd_zero, &mpd_ctx) == 0)
        {
            log_debug("order record %p left%s", (void *)deal->maker, mpd_to_sci(deal->maker->left, 0));
            order_finish_future(real, market, deal->maker);
            deal->maker = NULL;
        }
        if (mpd_cmp(deal->taker->left, mpd_zero, &mpd_ctx) == 0)
        {
            log_debug("order record %p left%s", (void *)deal->taker, mpd_to_sci(deal->taker->left, 0));
            order_finish_future(real, market, deal->taker);
            taker = NULL;
            break;
        }
    }
    skiplist_release_iterator(iter);
    // 处理未完成的order
    if (taker != 0)
    {
        log_debug("%s ordery type %d", __FUNCTION__, taker->type);
        if (taker->type == MARKET_ORDER_TYPE_LIMIT)
        { // || args->taker->type == MARKET_ORDER_TYPE_MARKET
            if (real)
            {
                push_order_message(ORDER_EVENT_PUT, taker, market);
            }
            int ret = order_put_future(market, taker);
            if (ret < 0)
            {
                log_fatal("order_put_future fail: %d, order: %" PRIu64 "", ret, taker->id);
            }
        }
        else if (taker->type == MARKET_ORDER_TYPE_MARKET)
        {
            // log_trace("order record");
            int ret = market_cancel_order(real, NULL, market, taker);
        }
    }
    return 0;
}

order_t *initOrder(args_t *args)
{
    order_t *order = malloc(sizeof(order_t));
    order->id = ++order_id_start;
    order->type = args->Type;
    order->isblast = 0;
    order->side = args->direction;
    order->pattern = args->pattern;
    order->oper_type = 0;
    order->create_time = current_timestamp();
    order->update_time = order->create_time;
    order->user_id = args->user_id;
    order->market = strdup(args->market->name);
    order->relate_order = 0;
    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->leverage = mpd_new(&mpd_ctx);
    order->trigger = mpd_new(&mpd_ctx);
    order->current_price = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->pnl = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);
    order->source = "";
    order->mm = 0;
    // order->source       = strdup(source);

    mpd_copy(order->price, args->entrustPrice, &mpd_ctx);
    mpd_copy(order->amount, args->volume, &mpd_ctx);
    mpd_copy(order->leverage, args->leverage, &mpd_ctx);
    mpd_copy(order->trigger, args->triggerPrice, &mpd_ctx);
    mpd_copy(order->current_price, args->market->latestPrice, &mpd_ctx);
    mpd_copy(order->taker_fee, args->taker_fee_rate, &mpd_ctx);
    mpd_copy(order->maker_fee, args->maker_fee_rate, &mpd_ctx);
    mpd_copy(order->left, args->volume, &mpd_ctx);
    mpd_copy(order->freeze, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_stock, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_money, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_fee, mpd_zero, &mpd_ctx);
    log_debug("order record %p", (void *)order);
    return order;
}

int checkPriAndFee(uint32_t pattern, uint32_t user_id, mpd_t *balance, mpd_t *priAndFee)
{
    if (pattern == 1)
    { // 逐仓
        if (mpd_cmp(balance, priAndFee, &mpd_ctx) < 0)
            return -1;
    }
    if (pattern == 2)
    { // 全仓
        if (getSumCrossCount(user_id))
        {
            mpd_t *totalPNL = getSumPNL(user_id);
            if (mpd_cmp(totalPNL, mpd_zero, &mpd_ctx) < 0)
            {
                mpd_add(totalPNL, totalPNL, balance, &mpd_ctx);
                if (mpd_cmp(totalPNL, priAndFee, &mpd_ctx) < 0)
                    return -1;
            }
            else
            {
                if (mpd_cmp(balance, priAndFee, &mpd_ctx) < 0)
                    return -1;
            }
        }
        else
        {
            if (mpd_cmp(balance, priAndFee, &mpd_ctx) < 0)
                return -1;
        }
    }
    return 0;
}

// 开仓
int market_put_order_open(void *args_)
{
    args_t *args = (args_t *)args_;
    // 检查钱包
    mpd_t *balance = balance_get(args->user_id, BALANCE_TYPE_AVAILABLE, args->market->money);
    if (!balance || mpd_cmp(balance, mpd_zero, &mpd_ctx) <= 0)
    {
        args->msg = "balance not enough";
        return -1;
    }
    // 检查买入数量
    if (mpd_cmp(args->volume, mpd_one, &mpd_ctx) < 0 || !mpd_isinteger(args->volume)){
        args->msg = "volume error";
        return -1;
    }

    position_mode_t *position_mode;
    position_mode = get_position_mode(args->user_id, args->market->name);
    // 计算保证金 如果以前有仓位，参照以前的仓位？
    position_t *position = NULL;
    position = get_position(args->user_id, args->market->name, BULL);
    if(!position) position = get_position(args->user_id, args->market->name, BEAR);

    if (position)
    {
        mpd_copy(args->leverage, position->leverage, &mpd_ctx);
        mpd_div(args->priAmount, args->volume, args->leverage, &mpd_ctx);
    }
    else
    {
        if(!position_mode){
            add_position_mode(args->user_id, args->market->name, args->pattern, args->leverage);
            position_mode = get_position_mode(args->user_id, args->market->name);
        }else{
            if(!check_position_order_mode(args->user_id, args->market)){// 如果之前没有下过单，或没有持有仓位，再可以修改
                log_trace("position pattern %d %d", position_mode->pattern, args->pattern);
                position_mode->pattern = args->pattern;
                mpd_copy(position_mode->leverage, args->leverage, &mpd_ctx);
            }
        }
        mpd_div(args->priAmount, args->volume, position_mode->leverage, &mpd_ctx);
    }
    // 判断仓位模式
    log_trace("position pattern %d %d", position_mode->pattern, args->pattern);
    if(position_mode->pattern != args->pattern){
        args->msg = "check position pattern fail";
        return -1;
    }
    log_debug("%s 需要保证金 %s", __FUNCTION__, mpd_to_sci(args->priAmount, 0));

    // 计算开仓手续费
    mpd_mul(args->fee, args->volume, args->taker_fee_rate, &mpd_ctx);

    // 计算余额 是否大于保证金
    mpd_copy(args->priAndFee, args->priAmount, &mpd_ctx);
    if (checkPriAndFee(args->pattern, args->user_id, balance, args->priAndFee)){
        args->msg = "checkPriAndFee fail";
        return -1;
    }

    args->taker = initOrder(args);
    args->taker->oper_type = 1;
    if (args->taker == NULL)
        return -__LINE__;

    // 在此处冻结资金 仅限价单需要
    if (args->Type == 1)
    {
        if (balance_freeze(args->taker->user_id, args->market->money, args->priAndFee) == NULL)
        {
            return -__LINE__;
        }
        mpd_copy(args->taker->freeze, args->priAndFee, &mpd_ctx);
    }
    if (args->Type == 1 || args->Type == 0)
    {
        execute_order(args->real, args->market, args->direction, args->taker);
    }
    else
    { // 计划委托
        if (args->real)
            push_order_message(ORDER_EVENT_PUT, args->taker, args->market);
        order_put_future(args->market, args->taker);
    }
    return 0;
}

int market_put_order_close(void *args_)
{
    args_t *args = (args_t *)args_;
    // 检查买入数量
    if (mpd_cmp(args->volume, mpd_one, &mpd_ctx) < 0 || !mpd_isinteger(args->volume))
    {
        args->msg = "volume 需大于0，且是整数";
        return -3;
    }

    // 检查仓位
    position_t *position = get_position(args->user_id, args->market->name, args->direction);
    if (!position)
    {
        args->msg = "仓位不存在";
        return -4;
    }
    if (mpd_cmp(position->position, args->volume, &mpd_ctx) < 0)
    {
        args->msg = "仓位不足";
        return -5;
    }

    // 限价或市价下单时，需要冻结仓位，计划委托下单则无需冻结
    if (args->Type == 1 || args->Type == 0)
    {
        mpd_add(position->frozen, position->frozen, args->volume, &mpd_ctx);
        mpd_sub(position->position, position->position, args->volume, &mpd_ctx);
    }

    mpd_copy(args->leverage, position->leverage, &mpd_ctx);
    args->taker = initOrder(args);
    args->taker->oper_type = 2;
    if (args->taker == NULL)
        return -__LINE__;

    if (args->Type == 1 || args->Type == 0)
    {
        execute_order(args->real, args->market, args->direction, args->taker);
    }
    else
    { // 计划委托
        if (args->real)
            push_order_message(ORDER_EVENT_PUT, args->taker, args->market);
        order_put_future(args->market, args->taker);
    }
    return 0;
}

int market_put_market_order(bool real, json_t **result, market_t *m, uint32_t user_id, uint32_t side, mpd_t *amount, mpd_t *taker_fee, const char *source)
{
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        mpd_t *balance = balance_get(user_id, BALANCE_TYPE_AVAILABLE, m->stock);
        if (!balance || mpd_cmp(balance, amount, &mpd_ctx) < 0)
        {
            return -1;
        }

        skiplist_iter *iter = skiplist_get_iterator(m->bids);
        skiplist_node *node = skiplist_next(iter);
        if (node == NULL)
        {
            skiplist_release_iterator(iter);
            return -3;
        }
        skiplist_release_iterator(iter);

        if (mpd_cmp(amount, m->min_amount, &mpd_ctx) < 0)
        {
            return -2;
        }
    }
    else
    {
        mpd_t *balance = balance_get(user_id, BALANCE_TYPE_AVAILABLE, m->money);
        if (!balance || mpd_cmp(balance, amount, &mpd_ctx) < 0)
        {
            return -1;
        }

        skiplist_iter *iter = skiplist_get_iterator(m->asks);
        skiplist_node *node = skiplist_next(iter);
        if (node == NULL)
        {
            skiplist_release_iterator(iter);
            return -3;
        }
        skiplist_release_iterator(iter);

        order_t *order = node->value;
        mpd_t *require = mpd_new(&mpd_ctx);
        mpd_mul(require, order->price, m->min_amount, &mpd_ctx);
        if (mpd_cmp(amount, require, &mpd_ctx) < 0)
        {
            mpd_del(require);
            return -2;
        }
        mpd_del(require);
    }

    order_t *order = malloc(sizeof(order_t));
    if (order == NULL)
    {
        return -__LINE__;
    }

    order->id = ++order_id_start;
    order->type = MARKET_ORDER_TYPE_MARKET;
    order->side = side;
    order->create_time = current_timestamp();
    order->update_time = order->create_time;
    order->market = strdup(m->name);
    order->source = strdup(source);
    order->user_id = user_id;
    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);
    // order->mm           = (strncmp(order->source, MM_SOURCE_STR, MM_SOURCE_STR_LEN) == 0) ? true : false;
    order->mm = get_mm_order_type_by_source(order->source);

    mpd_copy(order->price, mpd_zero, &mpd_ctx);
    mpd_copy(order->amount, amount, &mpd_ctx);
    mpd_copy(order->taker_fee, taker_fee, &mpd_ctx);
    mpd_copy(order->maker_fee, mpd_zero, &mpd_ctx);
    mpd_copy(order->left, amount, &mpd_ctx);
    mpd_copy(order->freeze, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_stock, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_money, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_fee, mpd_zero, &mpd_ctx);

    int ret;
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        ret = execute_market_ask_order(real, m, order);
    }
    else
    {
        ret = execute_market_bid_order(real, m, order);
    }
    if (ret < 0)
    {
        log_error("execute order: %" PRIu64 " fail: %d", order->id, ret);
        order_free(order);
        return -__LINE__;
    }

    if (real)
    {
        if (!order->mm)
        {
            int ret = append_order_history(order);
            if (ret < 0)
            {
                log_fatal("append_order_history fail: %d, order: %" PRIu64 "", ret, order->id);
            }
        }
        push_order_message(ORDER_EVENT_FINISH, order, m);
        *result = get_order_info(order);
    }

    order_free(order);
    return 0;
}

int market_cancel_order(bool real, json_t **result, market_t *m, order_t *order)
{
    if (real)
    {
        push_order_message(ORDER_EVENT_FINISH, order, m);
        if (result)
            *result = get_order_info(order);
    }

    if (order->oper_type == 1 && mpd_cmp(order->freeze, mpd_zero, &mpd_ctx) > 0)
    {
        if (balance_unfreeze(order->user_id, m->money, order->freeze) == NULL)
        {
            log_trace("order->freeze %s", mpd_to_sci(order->freeze, 0));
            return -__LINE__;
        }
    }

    if (order->oper_type == 2 && mpd_cmp(order->left, mpd_zero, &mpd_ctx) > 0)
    {
        position_t *position = get_position(order->user_id, order->market, order->side);
        mpd_sub(position->frozen, position->frozen, order->left, &mpd_ctx);
        mpd_add(position->position, position->position, order->left, &mpd_ctx);
    }
    // log_trace("order record");
    order_finish_future(real, m, order);
    return 0;
}

int market_cancel_all_order(market_t *m)
{
    order_t *order = NULL;
    skiplist_node *node = NULL;
    skiplist_iter *iter = skiplist_get_iterator(m->asks);
    while ((node = skiplist_next(iter)) != NULL)
    {
        order = node->value;
        push_order_message(ORDER_EVENT_FINISH, order, m);
        order_finish(true, m, order);
    }
    skiplist_release_iterator(iter);

    iter = skiplist_get_iterator(m->bids);
    while ((node = skiplist_next(iter)) != NULL)
    {
        order = node->value;
        push_order_message(ORDER_EVENT_FINISH, order, m);
        order_finish(true, m, order);
    }
    skiplist_release_iterator(iter);

    return 0;
}

int market_put_order(market_t *m, order_t *order)
{
    return order_put_future(m, order);
}

order_t *market_get_order(market_t *m, uint64_t order_id)
{
    struct dict_order_key key = {.order_id = order_id};
    dict_entry *entry = dict_find(m->orders, &key);
    if (entry)
    {
        return entry->val;
    }
    return NULL;
}

skiplist_t *market_get_order_list(market_t *m, uint32_t user_id)
{
    if(!m->users) return NULL;
    struct dict_user_key key = {.user_id = user_id};
    dict_entry *entry = dict_find(m->users, &key);
    if (entry)
    {
        return entry->val;
    }
    return NULL;
}

skiplist_t *market_get_user_order_list(uint32_t user_id)
{
    struct dict_user_key key = {.user_id = user_id};
    dict_entry *entry = dict_find(user_orders, &key);
    if (entry)
    {
        return entry->val;
    }
    return NULL;
}

int market_get_status(market_t *m, size_t *ask_count, mpd_t *ask_amount, size_t *bid_count, mpd_t *bid_amount)
{
    *ask_count = m->asks->len;
    *bid_count = m->bids->len;
    mpd_copy(ask_amount, mpd_zero, &mpd_ctx);
    mpd_copy(bid_amount, mpd_zero, &mpd_ctx);

    skiplist_node *node;
    skiplist_iter *iter = skiplist_get_iterator(m->asks);
    while ((node = skiplist_next(iter)) != NULL)
    {
        order_t *order = node->value;
        mpd_add(ask_amount, ask_amount, order->left, &mpd_ctx);
    }
    skiplist_release_iterator(iter);

    iter = skiplist_get_iterator(m->bids);
    while ((node = skiplist_next(iter)) != NULL)
    {
        order_t *order = node->value;
        mpd_add(bid_amount, bid_amount, order->left, &mpd_ctx);
    }
    skiplist_release_iterator(iter);

    return 0;
}

sds market_status(sds reply)
{
    reply = sdscatprintf(reply, "order last ID: %" PRIu64 "\n", order_id_start);
    reply = sdscatprintf(reply, "deals last ID: %" PRIu64 "\n", deals_id_start);
    return reply;
}

/*
 * Description:
 *     History: yang@haipo.me, 2017/03/16, create
 */

#include "me_config.h"
#include "me_server.h"
#include "me_balance.h"
#include "me_position.h"
#include "me_deal.h"
#include "me_update.h"
#include "me_market.h"
#include "me_trade.h"
#include "me_operlog.h"
#include "me_history.h"
#include "me_message.h"
#include "me_persist.h"
#include "me_cli.h"

static rpc_svr *svr;
static dict_t *dict_cache;
static nw_timer cache_timer;

struct cache_val
{
    double time;
    json_t *result;
};

static int reply_json(nw_ses *ses, rpc_pkg *pkg, const json_t *json)
{
    char *message_data;
    if (settings.debug)
    {
        message_data = json_dumps(json, JSON_INDENT(4));
    }
    else
    {
        message_data = json_dumps(json, 0);
    }
    if (message_data == NULL)
        return -__LINE__;
    log_debug("connection: %s send: %s", nw_sock_human_addr(&ses->peer_addr), message_data);

    rpc_pkg reply;
    memcpy(&reply, pkg, sizeof(reply));
    reply.pkg_type = RPC_PKG_TYPE_REPLY;
    reply.body = message_data;
    reply.body_size = strlen(message_data);
    rpc_send(ses, &reply);
    free(message_data);

    return 0;
}

static int reply_error(nw_ses *ses, rpc_pkg *pkg, int code, const char *message)
{
    json_t *error = json_object();
    json_object_set_new(error, "code", json_integer(code));
    json_object_set_new(error, "message", json_string(message));

    json_t *reply = json_object();
    json_object_set_new(reply, "error", error);
    json_object_set_new(reply, "result", json_null());
    json_object_set_new(reply, "id", json_integer(pkg->req_id));

    int ret = reply_json(ses, pkg, reply);
    json_decref(reply);

    return ret;
}

static int reply_error_invalid_argument(nw_ses *ses, rpc_pkg *pkg)
{
    return reply_error(ses, pkg, 1, "invalid argument");
}

static int reply_error_internal_error(nw_ses *ses, rpc_pkg *pkg)
{
    return reply_error(ses, pkg, 2, "internal error");
}

static int reply_error_service_unavailable(nw_ses *ses, rpc_pkg *pkg)
{
    return reply_error(ses, pkg, 3, "service unavailable");
}

static int reply_error_other(nw_ses *ses, rpc_pkg *pkg, const char *msg)
{
    return reply_error(ses, pkg, 4, msg);
}

static int reply_result(nw_ses *ses, rpc_pkg *pkg, json_t *result)
{
    json_t *reply = json_object();
    json_object_set_new(reply, "error", json_null());
    json_object_set(reply, "result", result);
    json_object_set_new(reply, "id", json_integer(pkg->req_id));

    int ret = reply_json(ses, pkg, reply);
    json_decref(reply);

    return ret;
}

static int reply_success(nw_ses *ses, rpc_pkg *pkg)
{
    json_t *result = json_object();
    json_object_set_new(result, "status", json_string("success"));

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static bool process_cache(nw_ses *ses, rpc_pkg *pkg, sds *cache_key)
{
    sds key = sdsempty();
    key = sdscatprintf(key, "%u", pkg->command);
    key = sdscatlen(key, pkg->body, pkg->body_size);
    dict_entry *entry = dict_find(dict_cache, key);
    if (entry == NULL)
    {
        *cache_key = key;
        return false;
    }

    struct cache_val *cache = entry->val;
    double now = current_timestamp();
    if ((now - cache->time) > settings.cache_timeout)
    {
        dict_delete(dict_cache, key);
        *cache_key = key;
        return false;
    }

    reply_result(ses, pkg, cache->result);
    sdsfree(key);
    return true;
}

static int add_cache(sds cache_key, json_t *result)
{
    struct cache_val cache;
    cache.time = current_timestamp();
    cache.result = result;
    json_incref(result);
    dict_replace(dict_cache, cache_key, &cache);

    return 0;
}

static int on_cmd_balance_query(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    size_t request_size = json_array_size(params);
    if (request_size == 0)
        return reply_error_invalid_argument(ses, pkg);

    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));
    if (user_id == 0)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    if (request_size == 1)
    {
        for (size_t i = 0; i < settings.asset_num; ++i)
        {
            const char *asset = settings.assets[i].name;
            json_t *unit = json_object();
            int prec_save = asset_prec(asset);
            int prec_show = asset_prec_show(asset);

            mpd_t *available = balance_get(user_id, BALANCE_TYPE_AVAILABLE, asset);
            if (available)
            {
                if (prec_save != prec_show)
                {
                    mpd_t *show = mpd_qncopy(available);
                    mpd_rescale(show, show, -prec_show, &mpd_ctx);
                    json_object_set_new_mpd(unit, "available", show);
                    mpd_del(show);
                }
                else
                {
                    json_object_set_new_mpd(unit, "available", available);
                }
            }
            else
            {
                json_object_set_new(unit, "available", json_string("0"));
            }

            mpd_t *freeze = balance_get(user_id, BALANCE_TYPE_FREEZE, asset);
            if (freeze)
            {
                if (prec_save != prec_show)
                {
                    mpd_t *show = mpd_qncopy(freeze);
                    mpd_rescale(show, show, -prec_show, &mpd_ctx);
                    json_object_set_new_mpd(unit, "freeze", show);
                    mpd_del(show);
                }
                else
                {
                    json_object_set_new_mpd(unit, "freeze", freeze);
                }
            }
            else
            {
                json_object_set_new(unit, "freeze", json_string("0"));
            }

            json_object_set_new(result, asset, unit);
        }
    }
    else
    {
        for (size_t i = 1; i < request_size; ++i)
        {
            const char *asset = json_string_value(json_array_get(params, i));
            if (!asset || !asset_exist(asset))
            {
                json_decref(result);
                return reply_error_invalid_argument(ses, pkg);
            }
            json_t *unit = json_object();
            int prec_save = asset_prec(asset);
            int prec_show = asset_prec_show(asset);

            mpd_t *available = balance_get(user_id, BALANCE_TYPE_AVAILABLE, asset);
            if (available)
            {
                if (prec_save != prec_show)
                {
                    mpd_t *show = mpd_qncopy(available);
                    mpd_rescale(show, show, -prec_show, &mpd_ctx);
                    json_object_set_new_mpd(unit, "available", show);
                    mpd_del(show);
                }
                else
                {
                    json_object_set_new_mpd(unit, "available", available);
                }
            }
            else
            {
                json_object_set_new(unit, "available", json_string("0"));
            }

            mpd_t *freeze = balance_get(user_id, BALANCE_TYPE_FREEZE, asset);
            if (freeze)
            {
                if (prec_save != prec_show)
                {
                    mpd_t *show = mpd_qncopy(freeze);
                    mpd_rescale(show, show, -prec_show, &mpd_ctx);
                    json_object_set_new_mpd(unit, "freeze", show);
                    mpd_del(show);
                }
                else
                {
                    json_object_set_new_mpd(unit, "freeze", freeze);
                }
            }
            else
            {
                json_object_set_new(unit, "freeze", json_string("0"));
            }

            json_object_set_new(result, asset, unit);
        }
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_balance_update(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    log_vip("on_cmd_balance_update");
    if (json_array_size(params) != 6)
    {
        log_vip("reply_error_invalid_argument");
        return reply_error_invalid_argument(ses, pkg);
    }
    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // asset
    if (!json_is_string(json_array_get(params, 1)))
    {
        log_vip("reply_error_invalid_argument");
        return reply_error_invalid_argument(ses, pkg);
    }
    const char *asset = json_string_value(json_array_get(params, 1));
    int prec = asset_prec_show(asset);
    if (prec < 0)
    {
        log_vip("reply_error_invalid_argument");
        return reply_error_invalid_argument(ses, pkg);
    }
    // business
    if (!json_is_string(json_array_get(params, 2)))
    {
        log_vip("reply_error_invalid_argument b");
        return reply_error_invalid_argument(ses, pkg);
    }
    const char *business = json_string_value(json_array_get(params, 2));

    // business_id
    if (!json_is_integer(json_array_get(params, 3)))
    {
        log_vip("reply_error_invalid_argument bid");
        return reply_error_invalid_argument(ses, pkg);
    }
    uint64_t business_id = json_integer_value(json_array_get(params, 3));

    // change
    if (!json_is_string(json_array_get(params, 4)))
    {
        log_vip("reply_error_invalid_argument c");
        return reply_error_invalid_argument(ses, pkg);
    }
    mpd_t *change = decimal(json_string_value(json_array_get(params, 4)), prec);
    if (change == NULL)
    {
        log_vip("reply_error_invalid_argument cd");
        return reply_error_invalid_argument(ses, pkg);
    }
    // detail
    json_t *detail = json_array_get(params, 5);
    if (!json_is_object(detail))
    {
        mpd_del(change);
        log_vip("reply_error_invalid_argument d");
        return reply_error_invalid_argument(ses, pkg);
    }

    int ret = update_user_balance(true, user_id, asset, business, business_id, change, detail);
    mpd_del(change);
    if (ret == -1)
    {
        return reply_error(ses, pkg, 10, "repeat update");
    }
    else if (ret == -2)
    {
        return reply_error(ses, pkg, 11, "balance not enough");
    }
    else if (ret < 0)
    {
        return reply_error_internal_error(ses, pkg);
    }

    append_operlog("update_balance", params);
    return reply_success(ses, pkg);
}

static int on_cmd_balance_unfreeze(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 3)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // asset
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *asset = json_string_value(json_array_get(params, 1));
    int prec = asset_prec_show(asset);
    if (prec < 0)
        return reply_error_invalid_argument(ses, pkg);

    // amount
    if (!json_is_string(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    mpd_t *amount = decimal(json_string_value(json_array_get(params, 2)), prec);
    if (amount == NULL)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *res = balance_unfreeze(user_id, asset, amount);
    if (!res)
    {
        return reply_error_internal_error(ses, pkg);
    }

    return reply_success(ses, pkg);
}

static int on_cmd_asset_list(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    json_t *result = json_array();
    for (int i = 0; i < settings.asset_num; ++i)
    {
        json_t *asset = json_object();
        json_object_set_new(asset, "name", json_string(settings.assets[i].name));
        json_object_set_new(asset, "prec", json_integer(settings.assets[i].prec_show));
        json_array_append_new(result, asset);
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static json_t *get_asset_summary(const char *name)
{
    size_t available_count;
    size_t freeze_count;
    mpd_t *total = mpd_new(&mpd_ctx);
    mpd_t *available = mpd_new(&mpd_ctx);
    mpd_t *freeze = mpd_new(&mpd_ctx);
    balance_status(name, total, &available_count, available, &freeze_count, freeze);

    json_t *obj = json_object();
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new_mpd(obj, "total_balance", total);
    json_object_set_new(obj, "available_count", json_integer(available_count));
    json_object_set_new_mpd(obj, "available_balance", available);
    json_object_set_new(obj, "freeze_count", json_integer(freeze_count));
    json_object_set_new_mpd(obj, "freeze_balance", freeze);

    mpd_del(total);
    mpd_del(available);
    mpd_del(freeze);

    return obj;
}

static int on_cmd_asset_summary(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    json_t *result = json_array();
    if (json_array_size(params) == 0)
    {
        for (int i = 0; i < settings.asset_num; ++i)
        {
            json_array_append_new(result, get_asset_summary(settings.assets[i].name));
        }
    }
    else
    {
        for (int i = 0; i < json_array_size(params); ++i)
        {
            const char *asset = json_string_value(json_array_get(params, i));
            if (asset == NULL)
                goto invalid_argument;
            if (!asset_exist(asset))
                goto invalid_argument;
            json_array_append_new(result, get_asset_summary(asset));
        }
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    json_decref(result);
    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_push_deals(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 7)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // side
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *amount = NULL;
    mpd_t *price = NULL;
    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 3)))
        goto invalid_argument;
    amount = decimal(json_string_value(json_array_get(params, 3)), market->stock_prec);
    if (amount == NULL || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // price
    if (!json_is_string(json_array_get(params, 4)))
        goto invalid_argument;
    price = decimal(json_string_value(json_array_get(params, 4)), market->money_prec);
    if (price == NULL || mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // taker fee
    if (!json_is_string(json_array_get(params, 5)))
        goto invalid_argument;
    taker_fee = decimal(json_string_value(json_array_get(params, 5)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // maker fee
    if (!json_is_string(json_array_get(params, 6)))
        goto invalid_argument;
    maker_fee = decimal(json_string_value(json_array_get(params, 6)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    uint64_t aid = ++order_id_start;
    uint64_t bid = ++order_id_start;
    uint64_t dealId = ++deals_id_start;
    push_deal_message_extra(current_timestamp(), market->name, aid, bid, user_id, user_id, price, amount,
                            maker_fee, taker_fee, side, dealId, market->stock, market->money);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(taker_fee);
    mpd_del(maker_fee);

    json_t *result = json_object();
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

// int market_put_limit_order_extra(bool real, json_t *result, market_t *m, uint32_t user_id, json_t *row, mpd_t *taker_fee, mpd_t *maker_fee, const char *source)
int market_put_limit_order_extra(bool real, json_t *result, market_t *m, uint32_t user_id, json_t *row, mpd_t *taker_fee, mpd_t *maker_fee)
{
    mpd_t *amount = NULL;
    mpd_t *price = NULL;
    uint32_t side = 0;
    json_t *info = json_object();

    if (!json_is_array(row))
        goto invalid_argument;

    // amount
    if (!json_is_string(json_array_get(row, 0)))
        goto invalid_argument;
    amount = decimal(json_string_value(json_array_get(row, 0)), m->stock_prec);
    if (amount == NULL || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // price
    if (!json_is_string(json_array_get(row, 1)))
        goto invalid_argument;
    price = decimal(json_string_value(json_array_get(row, 1)), m->money_prec);
    if (price == NULL || mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // side
    side = json_integer_value(json_array_get(row, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        goto invalid_argument;

    // source
    if (!json_is_string(json_array_get(row, 3)))
        goto invalid_argument;
    const char *source = json_string_value(json_array_get(row, 3));
    if (strlen(source) >= SOURCE_MAX_LEN)
        goto invalid_argument;

    json_t *o_i = NULL;
    int ret = market_put_limit_order(real, &o_i, m, user_id, side, amount, price, taker_fee, maker_fee, source);
    if (result)
    {
        if (ret == 0)
        {
            json_object_set_new(info, "code", json_integer(0));
            json_object_set_new(info, "message", json_string("success"));
            json_object_set_new(info, "order", o_i);
        }
        else if (ret == -1)
        {
            json_object_set_new(info, "code", json_integer(10));
            json_object_set_new(info, "message", json_string("balance not enough"));
        }
        else if (ret == -2)
        {
            json_object_set_new(info, "code", json_integer(11));
            json_object_set_new(info, "message", json_string("amount too small"));
        }
        else if (ret < 0)
        {
            json_object_set_new(info, "code", json_integer(2));
            json_object_set_new(info, "message", json_string("internal error"));
        }

        json_array_append_new(result, info);
    }
    else
    {
        json_decref(info);
    }

    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);

    return 0;

invalid_argument:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);

    if (result)
    {
        json_object_set_new(info, "code", json_integer(1));
        json_object_set_new(info, "message", json_string("invalid argument"));

        json_array_append_new(result, info);
    }
    else
    {
        json_decref(info);
    }

    return 0;
}

static int on_cmd_order_put_limit_batch(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 5)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // amount price side list
    json_t *aps_list = json_array_get(params, 2);
    if (!json_is_array(aps_list))
        return reply_error_invalid_argument(ses, pkg);

    size_t a_size = json_array_size(aps_list);
    if (a_size > PUT_LIMIT_ORDER_BATCH_MAX_NUM)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;
    // taker fee
    if (!json_is_string(json_array_get(params, 3)))
        goto invalid_argument;
    taker_fee = decimal(json_string_value(json_array_get(params, 3)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // maker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto invalid_argument;
    maker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // // source
    // if (!json_is_string(json_array_get(params, 5)))
    //     goto invalid_argument;
    // const char *source = json_string_value(json_array_get(params, 5));
    // if (strlen(source) >= SOURCE_MAX_LEN)
    //     goto invalid_argument;

    json_t *result = json_array();
    for (uint32_t i = 0; i < a_size; ++i)
    {
        market_put_limit_order_extra(true, result, market, user_id, json_array_get(aps_list, i), taker_fee, maker_fee);
    }

    mpd_del(taker_fee);
    mpd_del(maker_fee);

    append_operlog("limit_order_batch", params);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_order_update_limit(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 9)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // cancel order_id
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint64_t order_id = json_integer_value(json_array_get(params, 2));

    // side
    if (!json_is_integer(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 3));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *amount = NULL;
    mpd_t *price = NULL;
    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 4)))
        goto invalid_argument;
    amount = decimal(json_string_value(json_array_get(params, 4)), market->stock_prec);
    if (amount == NULL || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // price
    if (!json_is_string(json_array_get(params, 5)))
        goto invalid_argument;
    price = decimal(json_string_value(json_array_get(params, 5)), market->money_prec);
    if (price == NULL || mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // taker fee
    if (!json_is_string(json_array_get(params, 6)))
        goto invalid_argument;
    taker_fee = decimal(json_string_value(json_array_get(params, 6)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // maker fee
    if (!json_is_string(json_array_get(params, 7)))
        goto invalid_argument;
    maker_fee = decimal(json_string_value(json_array_get(params, 7)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // source
    if (!json_is_string(json_array_get(params, 8)))
        goto invalid_argument;
    const char *source = json_string_value(json_array_get(params, 8));
    if (strlen(source) >= SOURCE_MAX_LEN)
        goto invalid_argument;

    order_t *order = market_get_order(market, order_id);
    if (order != NULL && order->user_id != user_id)
    {
        market_cancel_order(true, NULL, market, order);
    }

    json_t *result = NULL;
    int ret = market_put_limit_order(true, &result, market, user_id, side, amount, price, taker_fee, maker_fee, source);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(taker_fee);
    mpd_del(maker_fee);

    if (ret == -1)
    {
        return reply_error(ses, pkg, 10, "balance not enough");
    }
    else if (ret == -2)
    {
        return reply_error(ses, pkg, 11, "amount too small");
    }
    else if (ret < 0)
    {
        log_fatal("market_update_limit_order fail: %d", ret);
        return reply_error_internal_error(ses, pkg);
    }

    append_operlog("update_limit_order", params);
    ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_order_update_limit_batch(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 6)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    json_t *order_id_list = json_array_get(params, 2);
    if (!json_is_array(order_id_list))
        return reply_error_invalid_argument(ses, pkg);

    // amount price side source list
    json_t *aps_list = json_array_get(params, 3);
    if (!json_is_array(aps_list))
        return reply_error_invalid_argument(ses, pkg);

    size_t a_size = json_array_size(aps_list);
    if (a_size > PUT_LIMIT_ORDER_BATCH_MAX_NUM)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;
    // taker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto invalid_argument;
    taker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // maker fee
    if (!json_is_string(json_array_get(params, 5)))
        goto invalid_argument;
    maker_fee = decimal(json_string_value(json_array_get(params, 5)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // source
    // if (!json_is_string(json_array_get(params, 6)))
    //     goto invalid_argument;
    // const char *source = json_string_value(json_array_get(params, 6));
    // if (strlen(source) >= SOURCE_MAX_LEN)
    //     goto invalid_argument;

    // cancel orders list
    if (json_array_size(order_id_list) > 0)
    {
        for (uint32_t i = 0; i < json_array_size(order_id_list); ++i)
        {
            json_t *row = json_array_get(order_id_list, i);
            if (!json_is_integer(row))
                continue;

            uint64_t o_id = json_integer_value(row);
            order_t *order = market_get_order(market, o_id);
            if (order == NULL)
            {
                continue;
            }
            if (order->user_id != user_id)
            {
                continue;
            }

            market_cancel_order(true, NULL, market, order);
        }
    }

    json_t *result = json_array();
    for (uint32_t i = 0; i < a_size; ++i)
    {
        market_put_limit_order_extra(true, result, market, user_id, json_array_get(aps_list, i), taker_fee, maker_fee);
    }

    mpd_del(taker_fee);
    mpd_del(maker_fee);

    append_operlog("update_limit_order_batch", params);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_order_put_limit(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    log_vip("on_cmd_order_put_limit");
    if (json_array_size(params) != 8)
    {
        log_vip("on_cmd_order_put_limit 0");
        return reply_error_invalid_argument(ses, pkg);
    }
    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
    {
        log_vip("on_cmd_order_put_limit 1");
        return reply_error_invalid_argument(ses, pkg);
    }
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
    {
        log_vip("on_cmd_order_put_limit 2");
        return reply_error_invalid_argument(ses, pkg);
    }
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
    {
        log_vip("on_cmd_order_put_limit 3");
        return reply_error_invalid_argument(ses, pkg);
    }
    // side
    if (!json_is_integer(json_array_get(params, 2)))
    {
        log_vip("on_cmd_order_put_limit 4");
        return reply_error_invalid_argument(ses, pkg);
    }
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
    {
        log_vip("on_cmd_order_put_limit 5");
        return reply_error_invalid_argument(ses, pkg);
    }

    mpd_t *amount = NULL;
    mpd_t *price = NULL;
    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 3)))
    {
        log_vip("on_cmd_order_put_limit 5.1");
        goto invalid_argument;
    }
    amount = decimal(json_string_value(json_array_get(params, 3)), market->stock_prec);
    if (amount == NULL || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
    {
        log_vip("on_cmd_order_put_limit 6");
        goto invalid_argument;
    }

    // price
    if (!json_is_string(json_array_get(params, 4)))
    {
        log_vip("on_cmd_order_put_limit 6.1");
        goto invalid_argument;
    }
    price = decimal(json_string_value(json_array_get(params, 4)), market->money_prec);
    if (price == NULL || mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
    {
        log_vip("on_cmd_order_put_limit 7");
        goto invalid_argument;
    }

    // taker fee
    if (!json_is_string(json_array_get(params, 5)))
    {
        log_vip("on_cmd_order_put_limit 7.1");
        goto invalid_argument;
    }
    taker_fee = decimal(json_string_value(json_array_get(params, 5)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
    {
        log_vip("on_cmd_order_put_limit 8");
        goto invalid_argument;
    }

    // maker fee
    if (!json_is_string(json_array_get(params, 6)))
    {
        log_vip("on_cmd_order_put_limit 8.1");
        goto invalid_argument;
    }
    maker_fee = decimal(json_string_value(json_array_get(params, 6)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
    {
        log_vip("on_cmd_order_put_limit 9");
        goto invalid_argument;
    }

    // source
    if (!json_is_string(json_array_get(params, 7)))
        goto invalid_argument;
    const char *source = json_string_value(json_array_get(params, 7));
    if (strlen(source) >= SOURCE_MAX_LEN)
    {
        log_vip("on_cmd_order_put_limit 10");
        goto invalid_argument;
    }

    json_t *result = NULL;
    int ret = market_put_limit_order(true, &result, market, user_id, side, amount, price, taker_fee, maker_fee, source);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(taker_fee);
    mpd_del(maker_fee);

    if (ret == -1)
    {
        return reply_error(ses, pkg, 10, "balance not enough");
    }
    else if (ret == -2)
    {
        return reply_error(ses, pkg, 11, "amount too small");
    }
    else if (ret < 0)
    {
        log_fatal("market_put_limit_order fail: %d", ret);
        return reply_error_internal_error(ses, pkg);
    }

    append_operlog("limit_order", params);
    ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_order_put_market(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 6)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // side
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return reply_error_invalid_argument(ses, pkg);

    mpd_t *amount = NULL;
    mpd_t *taker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 3)))
        goto invalid_argument;
    amount = decimal(json_string_value(json_array_get(params, 3)), market->stock_prec);
    if (amount == NULL || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto invalid_argument;

    // taker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto invalid_argument;
    taker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto invalid_argument;

    // source
    if (!json_is_string(json_array_get(params, 5)))
        goto invalid_argument;
    const char *source = json_string_value(json_array_get(params, 5));
    if (strlen(source) >= SOURCE_MAX_LEN)
        goto invalid_argument;

    json_t *result = NULL;
    int ret = market_put_market_order(true, &result, market, user_id, side, amount, taker_fee, source);

    mpd_del(amount);
    mpd_del(taker_fee);

    if (ret == -1)
    {
        return reply_error(ses, pkg, 10, "balance not enough");
    }
    else if (ret == -2)
    {
        return reply_error(ses, pkg, 11, "amount too small");
    }
    else if (ret == -3)
    {
        return reply_error(ses, pkg, 12, "no enough trader");
    }
    else if (ret < 0)
    {
        log_fatal("market_put_limit_order fail: %d", ret);
        return reply_error_internal_error(ses, pkg);
    }

    append_operlog("market_order", params);
    ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    if (amount)
        mpd_del(amount);
    if (taker_fee)
        mpd_del(taker_fee);

    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_order_query(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 4)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // offset
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 2));

    // limit
    if (!json_is_integer(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 3));
    if (limit > ORDER_LIST_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "limit", json_integer(limit));
    json_object_set_new(result, "offset", json_integer(offset));

    json_t *orders = json_array();
    skiplist_t *order_list = market_get_order_list(market, user_id);
    if (order_list == NULL)
    {
        json_object_set_new(result, "total", json_integer(0));
    }
    else
    {
        json_object_set_new(result, "total", json_integer(order_list->len));
        if (offset < order_list->len)
        {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            for (size_t i = 0; i < offset; i++)
            {
                if (skiplist_next(iter) == NULL)
                    break;
            }
            size_t index = 0;
            while ((node = skiplist_next(iter)) != NULL && index < limit)
            {
                index++;
                order_t *order = node->value;
                json_array_append_new(orders, get_order_info(order));
            }
            skiplist_release_iterator(iter);
        }
    }

    json_object_set_new(result, "records", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_query_brief(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 4)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // offset
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 2));

    // limit
    if (!json_is_integer(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 3));
    if (limit > ORDER_LIST_BRIEF_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "limit", json_integer(limit));
    json_object_set_new(result, "offset", json_integer(offset));

    json_t *orders = json_array();
    skiplist_t *order_list = market_get_order_list(market, user_id);
    if (order_list == NULL)
    {
        json_object_set_new(result, "total", json_integer(0));
    }
    else
    {
        json_object_set_new(result, "total", json_integer(order_list->len));
        if (offset < order_list->len)
        {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            for (size_t i = 0; i < offset; i++)
            {
                if (skiplist_next(iter) == NULL)
                    break;
            }
            size_t index = 0;
            while ((node = skiplist_next(iter)) != NULL && index < limit)
            {
                index++;
                // order_t *order = node->value;
                json_array_append_new(orders, get_order_info_brief(node->value));
            }
            skiplist_release_iterator(iter);
        }
    }

    json_object_set_new(result, "records", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_query_all(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 3)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // offset
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 1));

    // limit
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 2));
    if (limit > ORDER_LIST_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "limit", json_integer(limit));
    json_object_set_new(result, "offset", json_integer(offset));

    json_t *orders = json_array();
    skiplist_t *order_list = market_get_user_order_list(user_id);
    if (order_list == NULL)
    {
        json_object_set_new(result, "total", json_integer(0));
    }
    else
    {
        json_object_set_new(result, "total", json_integer(order_list->len));
        if (offset < order_list->len)
        {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            for (size_t i = 0; i < offset; i++)
            {
                if (skiplist_next(iter) == NULL)
                    break;
            }
            size_t index = 0;
            while ((node = skiplist_next(iter)) != NULL && index < limit)
            {
                index++;
                order_t *order = node->value;
                json_array_append_new(orders, get_order_info(order));
            }
            skiplist_release_iterator(iter);
        }
    }

    json_object_set_new(result, "records", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_query_alluser(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 2)
        return reply_error_invalid_argument(ses, pkg);
    // offset
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 0));

    // limit
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 1));
    if (limit > ORDER_LIST_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "limit", json_integer(limit));
    json_object_set_new(result, "offset", json_integer(offset));

    json_t *orders = json_array();

    dict_iterator *user_orders_iter = dict_get_iterator(user_orders);
    dict_entry *user_orders_entry;
    size_t index = 0;
    size_t total = 0;
    while ((user_orders_entry = dict_next(user_orders_iter)) != NULL)
    {
        skiplist_t *order_list = user_orders_entry->val;
        if (order_list)
        {
            total += order_list->len;
        }
    }
    dict_release_iterator(user_orders_iter);
    json_object_set_new(result, "total", json_integer(total));

    user_orders_iter = dict_get_iterator(user_orders);
    while ((user_orders_entry = dict_next(user_orders_iter)) != NULL)
    {
        skiplist_t *order_list = user_orders_entry->val;
        if (order_list)
        {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            for (; index < offset; index++)
            {
                if (skiplist_next(iter) == NULL)
                    break;
            }
            while ((node = skiplist_next(iter)) != NULL && index < offset + limit)
            {
                index++;
                order_t *order = node->value;
                json_array_append_new(orders, get_order_info(order));
            }
            skiplist_release_iterator(iter);
        }
    }
    dict_release_iterator(user_orders_iter);
    json_object_set_new(result, "records", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_cancel(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 3)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // order_id
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint64_t order_id = json_integer_value(json_array_get(params, 2));

    order_t *order = market_get_order(market, order_id);
    if (order == NULL)
    {
        return reply_error(ses, pkg, 10, "order not found");
    }
    if (order->user_id != user_id)
    {
        return reply_error(ses, pkg, 11, "user not match");
    }

    json_t *result = NULL;
    int ret = market_cancel_order(true, &result, market, order);
    if (ret < 0)
    {
        log_fatal("cancel order: %" PRIu64 " fail: %d", order->id, ret);
        return reply_error_internal_error(ses, pkg);
    }

    append_operlog("cancel_order", params);
    ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_cancel_batch(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    int param_size = json_array_size(params);
    if (param_size != 3 && param_size != 2)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_array();
    if (param_size == 2)
    { // cancel all
        market_get_order_list(market, user_id);
        skiplist_t *order_list = market_get_order_list(market, user_id);
        if (order_list)
        {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            while ((node = skiplist_next(iter)) != NULL)
            {
                market_cancel_order(true, NULL, market, node->value);
            }
            skiplist_release_iterator(iter);
        }
    }
    else
    { // cancel by order id list
        json_t *order_id_list = json_array_get(params, 2);
        if (!json_is_array(order_id_list))
            return reply_error_invalid_argument(ses, pkg);

        for (uint32_t i = 0; i < json_array_size(order_id_list); ++i)
        {
            json_t *row = json_array_get(order_id_list, i);
            if (!json_is_integer(row))
                continue;

            uint64_t o_id = json_integer_value(row);
            order_t *order = market_get_order(market, o_id);
            if (order == NULL)
            {
                continue;
            }
            if (order->user_id != user_id)
            {
                continue;
            }

            market_cancel_order(true, NULL, market, order);

            json_array_append_new(result, json_integer(o_id));
        }
    }

    append_operlog("cancel_order_batch", params);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_book(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 4)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // side
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 1));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return reply_error_invalid_argument(ses, pkg);

    // offset
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 2));

    // limit
    if (!json_is_integer(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 3));
    if (limit > ORDER_BOOK_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "offset", json_integer(offset));
    json_object_set_new(result, "limit", json_integer(limit));

    uint64_t total;
    skiplist_iter *iter;
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        iter = skiplist_get_iterator(market->asks);
        total = market->asks->len;
        json_object_set_new(result, "total", json_integer(total));
    }
    else
    {
        iter = skiplist_get_iterator(market->bids);
        total = market->bids->len;
        json_object_set_new(result, "total", json_integer(total));
    }

    json_t *orders = json_array();
    if (offset < total)
    {
        for (size_t i = 0; i < offset; i++)
        {
            if (skiplist_next(iter) == NULL)
                break;
        }
        size_t index = 0;
        skiplist_node *node;
        while ((node = skiplist_next(iter)) != NULL && index < limit)
        {
            index++;
            order_t *order = node->value;
            json_array_append_new(orders, get_order_info(order));
        }
    }
    skiplist_release_iterator(iter);

    json_object_set_new(result, "orders", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static bool check_order_by_time(order_t *order, uint64_t start_time, uint64_t end_time)
{
    int cnt = 0;

    if (order)
    {
        if (start_time)
        {
            if (order->create_time >= start_time)
                cnt++;
        }
        else
        {
            cnt++;
        }

        if (end_time)
        {
            if (end_time > start_time && order->create_time <= end_time)
                cnt++;
        }
        else
        {
            cnt++;
        }
    }

    return cnt == 2;
}

static int on_cmd_order_book_ext(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 7)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // side
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 1));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return reply_error_invalid_argument(ses, pkg);

    // offset
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    size_t offset = json_integer_value(json_array_get(params, 2));

    // limit
    if (!json_is_integer(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 3));
    if (limit > ORDER_BOOK_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    // user_id
    if (!json_is_integer(json_array_get(params, 4)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 4));

    // start time
    if (!json_is_integer(json_array_get(params, 5)))
        return reply_error_invalid_argument(ses, pkg);
    uint64_t start_time = json_integer_value(json_array_get(params, 5));

    // end time
    if (!json_is_integer(json_array_get(params, 6)))
        return reply_error_invalid_argument(ses, pkg);
    uint64_t end_time = json_integer_value(json_array_get(params, 6));
    if (end_time && start_time > end_time)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_object_set_new(result, "offset", json_integer(offset));
    json_object_set_new(result, "limit", json_integer(limit));

    uint64_t total = 0;
    skiplist_iter *iter;
    skiplist_node *node;
    order_t *order;
    skiplist_t *list;
    if (side == MARKET_ORDER_SIDE_ASK)
    {
        list = market->asks;
    }
    else
    {
        list = market->bids;
    }
    iter = skiplist_get_iterator(list);
    while ((node = skiplist_next(iter)) != NULL)
    {
        order = node->value;
        if (order->user_id == user_id && check_order_by_time(order, start_time, end_time))
        {
            total++;
        }
    }
    json_object_set_new(result, "total", json_integer(total));
    skiplist_release_iterator(iter);

    json_t *orders = json_array();
    if (offset < total)
    {
        iter = skiplist_get_iterator(list);
        size_t index = 0;
        while ((node = skiplist_next(iter)) != NULL && index < offset)
        {
            order = node->value;
            if (order->user_id == user_id && check_order_by_time(order, start_time, end_time))
            {
                index++;
            }
        }
        index = 0;
        while ((node = skiplist_next(iter)) != NULL && index < limit)
        {
            order = node->value;
            if (order->user_id == user_id && check_order_by_time(order, start_time, end_time))
            {
                index++;
                json_array_append_new(orders, get_order_info(order));
            }
        }
        skiplist_release_iterator(iter);
    }

    json_object_set_new(result, "orders", orders);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static json_t *get_depth(market_t *market, size_t limit)
{
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);

    json_t *asks = json_array();
    skiplist_iter *iter = skiplist_get_iterator(market->asks);
    skiplist_node *node = skiplist_next(iter);
    size_t index = 0;
    while (node && index < limit)
    {
        index++;
        order_t *order = node->value;
        mpd_copy(price, order->price, &mpd_ctx);
        mpd_copy(amount, order->left, &mpd_ctx);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order = node->value;
            if (mpd_cmp(price, order->price, &mpd_ctx) == 0)
            {
                mpd_add(amount, amount, order->left, &mpd_ctx);
            }
            else
            {
                break;
            }
        }
        json_t *info = json_array();
        json_array_append_new_mpd(info, price);
        json_array_append_new_mpd(info, amount);
        json_array_append_new(asks, info);
    }
    skiplist_release_iterator(iter);

    json_t *bids = json_array();
    iter = skiplist_get_iterator(market->bids);
    node = skiplist_next(iter);
    index = 0;
    while (node && index < limit)
    {
        index++;
        order_t *order = node->value;
        mpd_copy(price, order->price, &mpd_ctx);
        mpd_copy(amount, order->left, &mpd_ctx);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order = node->value;
            if (mpd_cmp(price, order->price, &mpd_ctx) == 0)
            {
                mpd_add(amount, amount, order->left, &mpd_ctx);
            }
            else
            {
                break;
            }
        }
        json_t *info = json_array();
        json_array_append_new_mpd(info, price);
        json_array_append_new_mpd(info, amount);
        json_array_append_new(bids, info);
    }
    skiplist_release_iterator(iter);

    mpd_del(price);
    mpd_del(amount);

    json_t *result = json_object();
    json_object_set_new(result, "asks", asks);
    json_object_set_new(result, "bids", bids);

    return result;
}

static json_t *get_depth_merge(market_t *market, size_t limit, mpd_t *interval)
{
    mpd_t *q = mpd_new(&mpd_ctx);
    mpd_t *r = mpd_new(&mpd_ctx);
    mpd_t *price = mpd_new(&mpd_ctx);
    mpd_t *amount = mpd_new(&mpd_ctx);

    json_t *asks = json_array();
    skiplist_iter *iter = skiplist_get_iterator(market->asks);
    skiplist_node *node = skiplist_next(iter);
    size_t index = 0;
    while (node && index < limit)
    {
        index++;
        order_t *order = node->value;
        mpd_divmod(q, r, order->price, interval, &mpd_ctx);
        mpd_mul(price, q, interval, &mpd_ctx);
        if (mpd_cmp(r, mpd_zero, &mpd_ctx) != 0)
        {
            mpd_add(price, price, interval, &mpd_ctx);
        }
        mpd_copy(amount, order->left, &mpd_ctx);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order = node->value;
            if (mpd_cmp(price, order->price, &mpd_ctx) >= 0)
            {
                mpd_add(amount, amount, order->left, &mpd_ctx);
            }
            else
            {
                break;
            }
        }
        json_t *info = json_array();
        json_array_append_new_mpd(info, price);
        json_array_append_new_mpd(info, amount);
        json_array_append_new(asks, info);
    }
    skiplist_release_iterator(iter);

    json_t *bids = json_array();
    iter = skiplist_get_iterator(market->bids);
    node = skiplist_next(iter);
    index = 0;
    while (node && index < limit)
    {
        index++;
        order_t *order = node->value;
        mpd_divmod(q, r, order->price, interval, &mpd_ctx);
        mpd_mul(price, q, interval, &mpd_ctx);
        mpd_copy(amount, order->left, &mpd_ctx);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order = node->value;
            if (mpd_cmp(price, order->price, &mpd_ctx) <= 0)
            {
                mpd_add(amount, amount, order->left, &mpd_ctx);
            }
            else
            {
                break;
            }
        }

        json_t *info = json_array();
        json_array_append_new_mpd(info, price);
        json_array_append_new_mpd(info, amount);
        json_array_append_new(bids, info);
    }
    skiplist_release_iterator(iter);

    mpd_del(q);
    mpd_del(r);
    mpd_del(price);
    mpd_del(amount);

    json_t *result = json_object();
    json_object_set_new(result, "asks", asks);
    json_object_set_new(result, "bids", bids);

    return result;
}

static int on_cmd_order_book_depth(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 3)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // limit
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    size_t limit = json_integer_value(json_array_get(params, 1));
    if (limit > ORDER_BOOK_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    // interval
    if (!json_is_string(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    mpd_t *interval = decimal(json_string_value(json_array_get(params, 2)), market->money_prec);
    if (!interval)
        return reply_error_invalid_argument(ses, pkg);
    if (mpd_cmp(interval, mpd_zero, &mpd_ctx) < 0)
    {
        mpd_del(interval);
        return reply_error_invalid_argument(ses, pkg);
    }

    sds cache_key = NULL;
    if (process_cache(ses, pkg, &cache_key))
    {
        mpd_del(interval);
        return 0;
    }

    json_t *result = NULL;
    if (mpd_cmp(interval, mpd_zero, &mpd_ctx) == 0)
    {
        result = get_depth(market, limit);
    }
    else
    {
        result = get_depth_merge(market, limit, interval);
    }
    mpd_del(interval);

    if (result == NULL)
    {
        sdsfree(cache_key);
        return reply_error_internal_error(ses, pkg);
    }

    add_cache(cache_key, result);
    sdsfree(cache_key);

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_order_detail(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 2)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // order_id
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    uint64_t order_id = json_integer_value(json_array_get(params, 1));

    order_t *order = market_get_order(market, order_id);
    json_t *result = NULL;
    if (order == NULL)
    {
        result = json_null();
    }
    else
    {
        result = get_order_info(order);
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_market_list(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    json_t *result = json_array();
    for (int i = 0; i < settings.market_num; ++i)
    {
        json_t *market = json_object();
        json_object_set_new(market, "name", json_string(settings.markets[i].name));
        json_object_set_new(market, "stock", json_string(settings.markets[i].stock));
        json_object_set_new(market, "money", json_string(settings.markets[i].money));
        json_object_set_new(market, "fee_prec", json_integer(settings.markets[i].fee_prec));
        json_object_set_new(market, "stock_prec", json_integer(settings.markets[i].stock_prec));
        json_object_set_new(market, "money_prec", json_integer(settings.markets[i].money_prec));
        json_object_set_new_mpd(market, "min_amount", settings.markets[i].min_amount);
        json_array_append_new(result, market);
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static json_t *get_market_summary(const char *name)
{
    size_t ask_count;
    size_t bid_count;
    mpd_t *ask_amount = mpd_new(&mpd_ctx);
    mpd_t *bid_amount = mpd_new(&mpd_ctx);
    market_t *market = get_market(name);
    market_get_status(market, &ask_count, ask_amount, &bid_count, bid_amount);

    json_t *obj = json_object();
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "ask_count", json_integer(ask_count));
    json_object_set_new_mpd(obj, "ask_amount", ask_amount);
    json_object_set_new(obj, "bid_count", json_integer(bid_count));
    json_object_set_new_mpd(obj, "bid_amount", bid_amount);

    mpd_del(ask_amount);
    mpd_del(bid_amount);

    return obj;
}

static int on_cmd_market_summary(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    json_t *result = json_array();
    if (json_array_size(params) == 0)
    {
        for (int i = 0; i < settings.market_num; ++i)
        {
            json_array_append_new(result, get_market_summary(settings.markets[i].name));
        }
    }
    else
    {
        for (int i = 0; i < json_array_size(params); ++i)
        {
            const char *market = json_string_value(json_array_get(params, i));
            if (market == NULL)
                goto invalid_argument;
            if (get_market(market) == NULL)
                goto invalid_argument;
            json_array_append_new(result, get_market_summary(market));
        }
    }

    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;

invalid_argument:
    json_decref(result);
    return reply_error_invalid_argument(ses, pkg);
}

static int on_cmd_market_add_asset(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 2)
        return reply_error_invalid_argument(ses, pkg);

    // asset
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *asset = json_string_value(json_array_get(params, 0));
    if (strlen(asset) > ASSET_NAME_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    // prec show
    if (!json_is_integer(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    int prec_show = json_integer_value(json_array_get(params, 1));
    if (prec_show > MAX_ASSET_PREC_SHOW)
        return reply_error_invalid_argument(ses, pkg);

    if (asset_exist(asset))
    { // update
        // check prec of related market
        if (check_market_prec_by_asset(asset, prec_show) < 0)
            return reply_error_invalid_argument(ses, pkg);

        // add balance struct
        add_balance(asset, MAX_ASSET_PREC_SAVE, prec_show);

        // update config file
        add_asset_into_config(asset, MAX_ASSET_PREC_SAVE, prec_show, false);
    }
    else
    { // add asset
        // add balance struct
        add_balance(asset, MAX_ASSET_PREC_SAVE, prec_show);

        // update config file
        add_asset_into_config(asset, MAX_ASSET_PREC_SAVE, prec_show, true);
    }

    return reply_success(ses, pkg);
}

static int on_cmd_market_del_asset(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 1)
        return reply_error_invalid_argument(ses, pkg);

    // asset
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *asset = json_string_value(json_array_get(params, 0));
    if (strlen(asset) > ASSET_NAME_MAX_LEN)
        return reply_error_invalid_argument(ses, pkg);

    if (asset_exist(asset))
    { // delete
        // cancel all related order
        // delete related market
        del_market_by_asset(asset);
        // del_market_from_config_by_asset(asset);

        // delete balance
        del_balance(asset);

        // update config file
        del_asset_from_config(asset);

        make_slice(time(NULL), 600);
    }
    else
    {
        return reply_error_invalid_argument(ses, pkg);
    }

    return reply_success(ses, pkg);
}

static int on_cmd_market_add_market(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 7)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));

    // stock
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *stock = json_string_value(json_array_get(params, 1));
    if (strlen(stock) > ASSET_NAME_MAX_LEN || !asset_exist(stock))
        return reply_error_invalid_argument(ses, pkg);

    // stock prec
    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    int stock_prec = json_integer_value(json_array_get(params, 2));
    if (stock_prec > MAX_ASSET_PREC_SHOW)
        return reply_error_invalid_argument(ses, pkg);

    // money
    if (!json_is_string(json_array_get(params, 3)))
        return reply_error_invalid_argument(ses, pkg);
    const char *money = json_string_value(json_array_get(params, 3));
    if (strlen(money) > ASSET_NAME_MAX_LEN || !asset_exist(money))
        return reply_error_invalid_argument(ses, pkg);

    // money prec
    if (!json_is_integer(json_array_get(params, 4)))
        return reply_error_invalid_argument(ses, pkg);
    int money_prec = json_integer_value(json_array_get(params, 4));
    if (money_prec > MAX_ASSET_PREC_SHOW)
        return reply_error_invalid_argument(ses, pkg);

    // fee prec
    if (!json_is_integer(json_array_get(params, 5)))
        return reply_error_invalid_argument(ses, pkg);
    int fee_prec = json_integer_value(json_array_get(params, 5));
    if (fee_prec > MAX_MARKET_PREC_FEE)
        return reply_error_invalid_argument(ses, pkg);

    if (stock_prec + money_prec > asset_prec(money))
        return reply_error_invalid_argument(ses, pkg);
    if (stock_prec + fee_prec > asset_prec(stock))
        return reply_error_invalid_argument(ses, pkg);
    if (money_prec + fee_prec > asset_prec(money))
        return reply_error_invalid_argument(ses, pkg);

    // min amount
    if (!json_is_string(json_array_get(params, 6)))
        return reply_error_invalid_argument(ses, pkg);
    mpd_t *min_amount = decimal(json_string_value(json_array_get(params, 6)), 0);
    if (min_amount == NULL || mpd_cmp(min_amount, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(min_amount, mpd_one, &mpd_ctx) >= 0)
        return reply_error_invalid_argument(ses, pkg);

    market_t *market = get_market(market_name);
    if (market)
    { // update
        // cancel all related order
        market_cancel_all_order(market);

        // update market struct
        if (market->stock)
            free(market->stock);
        market->stock = strdup(stock);
        if (market->money)
            free(market->money);
        market->money = strdup(money);
        market->stock_prec = stock_prec;
        market->money_prec = money_prec;
        market->fee_prec = fee_prec;
        if (market->min_amount)
            mpd_del(market->min_amount);
        market->min_amount = mpd_qncopy(min_amount);

        // update config file
        add_market_into_config(market_name, stock, money, stock_prec, money_prec, fee_prec, min_amount, false);
    }
    else
    { // add market
        // add market struct
        struct market m_conf;
        m_conf.name = (char *)market_name;
        m_conf.stock = (char *)stock;
        m_conf.money = (char *)money;
        m_conf.stock_prec = stock_prec;
        m_conf.money_prec = money_prec;
        m_conf.fee_prec = fee_prec;
        m_conf.min_amount = min_amount;
        add_market(&m_conf);

        // update config file
        add_market_into_config(market_name, stock, money, stock_prec, money_prec, fee_prec, min_amount, true);
    }

    mpd_del(min_amount);
    return reply_success(ses, pkg);
}

static int on_cmd_market_del_market(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    if (json_array_size(params) != 1)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 0));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_invalid_argument(ses, pkg);

    // cancel order and delete market
    market_cancel_all_order(market);
    // free market struct
    del_market(market);

    // update config file
    del_market_from_config(market_name);

    make_slice(time(NULL), 600);

    return reply_success(ses, pkg);
}

static int on_cmd_make_slice(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    make_slice(time(NULL), 0);

    return reply_success(ses, pkg);
}
// todo:合约开仓代码入口
static int on_cmd_order_open(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    args_t *args = initOpenArgs(params); // 参数构造
    if (!args)
        return reply_error_invalid_argument(ses, pkg);
    args->real = 1;
    args->bOpen = 1;
    int ret = market_put_order_common(args);

    if (ret)
    {
        return reply_error_other(ses, pkg, args->msg);
    }
    else
    {
        append_operlog("order_open", params);
        on_planner();//处理计划委托
        return reply_success(ses, pkg);
    }
}

static int on_cmd_order_close(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    args_t *args = initCloseArgs(params);
    if (!args)
        return reply_error_invalid_argument(ses, pkg);
    args->real = 1;
    args->bOpen = 0;
    int ret = market_put_order_common(args);

    if (ret){
        return reply_error_other(ses, pkg, args->msg);
    }
    else{
        append_operlog("order_close", params);
        on_planner();//处理计划委托
        return reply_success(ses, pkg);
    }
}

static int on_cmd_position_query(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    size_t request_size = json_array_size(params);
    if (request_size != 3)
        return reply_error_invalid_argument(ses, pkg);

    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));
    if (user_id == 0)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_other(ses, pkg, "market不存在");

    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side == 0)
        return reply_error_invalid_argument(ses, pkg);

    json_t *result = json_object();
    json_t *positions = json_array();

    position_t *position = get_position(user_id, market_name, side);
    if (position)
    {
        json_t *unit = json_object();
        json_object_set_new(unit, "user_id", json_integer(position->user_id));
        json_object_set_new(unit, "market", json_string(position->market));
        json_object_set_new(unit, "side", json_integer(position->side));
        json_object_set_new(unit, "pattern", json_integer(position->pattern));
        json_object_set_new_mpd(unit, "leverage", position->leverage);
        json_object_set_new_mpd(unit, "position", position->position);
        json_object_set_new_mpd(unit, "frozen", position->frozen);
        json_object_set_new_mpd(unit, "price", position->price);
        json_object_set_new_mpd(unit, "principal", position->principal);
        json_array_append_new(positions, unit);
        json_object_set_new(result, "count", json_integer(1));
    }
    else
    {
        json_object_set_new(result, "count", json_integer(0));
    }
    json_object_set_new(result, "postions", positions);
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_position_query_all(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    size_t request_size = json_array_size(params);
    if (request_size != 2)
        return reply_error_invalid_argument(ses, pkg);

    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));
    if (user_id == 0)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = NULL;
    if(strlen(market_name) != 0){
        market = get_market(market_name);
        if (market == NULL)
            return reply_error_other(ses, pkg, "market不存在");
    }

    json_t *result = json_object();
    json_t *positions = json_array();
    int count = 0;
    for (size_t i = 0; i < settings.market_num; ++i) {
        position_t *position = get_position(user_id, settings.markets[i].name, BEAR);
        if (position)
        {
            json_t *unit = json_object();
            json_object_set_new(unit, "user_id", json_integer(position->user_id));
            json_object_set_new(unit, "market", json_string(position->market));
            json_object_set_new(unit, "side", json_integer(position->side));
            json_object_set_new(unit, "pattern", json_integer(position->pattern));
            json_object_set_new_mpd(unit, "leverage", position->leverage);
            json_object_set_new_mpd(unit, "position", position->position);
            json_object_set_new_mpd(unit, "frozen", position->frozen);
            json_object_set_new_mpd(unit, "price", position->price);
            json_object_set_new_mpd(unit, "principal", position->principal);
            json_array_append_new(positions, unit);
            count += 1;
        }
        position = get_position(user_id, settings.markets[i].name, BULL);
        if (position)
        {
            json_t *unit = json_object();
            json_object_set_new(unit, "user_id", json_integer(position->user_id));
            json_object_set_new(unit, "market", json_string(position->market));
            json_object_set_new(unit, "side", json_integer(position->side));
            json_object_set_new(unit, "pattern", json_integer(position->pattern));
            json_object_set_new_mpd(unit, "leverage", position->leverage);
            json_object_set_new_mpd(unit, "position", position->position);
            json_object_set_new_mpd(unit, "frozen", position->frozen);
            json_object_set_new_mpd(unit, "price", position->price);
            json_object_set_new_mpd(unit, "principal", position->principal);
            json_array_append_new(positions, unit);
            count += 1;
        }
    }
    json_object_set_new(result, "postions", positions);
    json_object_set_new(result, "count", json_integer(count));
    int ret = reply_result(ses, pkg, result);
    json_decref(result);
    return ret;
}

static int on_cmd_position_adjust_principal(nw_ses *ses, rpc_pkg *pkg, json_t *params)
{
    size_t request_size = json_array_size(params);
    if (request_size != 4)
        return reply_error_invalid_argument(ses, pkg);

    if (!json_is_integer(json_array_get(params, 0)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t user_id = json_integer_value(json_array_get(params, 0));
    if (user_id == 0)
        return reply_error_invalid_argument(ses, pkg);

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return reply_error_invalid_argument(ses, pkg);
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return reply_error_other(ses, pkg, "market不存在");

    if (!json_is_integer(json_array_get(params, 2)))
        return reply_error_invalid_argument(ses, pkg);
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side == 0)
        return reply_error_invalid_argument(ses, pkg);

    // change
    if (!json_is_string(json_array_get(params, 3)))
    {
        log_vip("reply_error_invalid_argument c");
        return reply_error_invalid_argument(ses, pkg);
    }
    mpd_t *change = decimal(json_string_value(json_array_get(params, 3)), market->money_prec);
    if (change == NULL)
    {
        log_vip("reply_error_invalid_argument cd");
        return reply_error_invalid_argument(ses, pkg);
    }

    position_t *position = get_position(user_id, market_name, side);
    if (position){
        int ret = mpd_cmp(change, mpd_zero, &mpd_ctx);
        if( ret > 0 ){
            //检查余额
            mpd_t *balance = balance_get(user_id, BALANCE_TYPE_AVAILABLE, market->money);
            if (!balance || mpd_cmp(balance, change, &mpd_ctx) < 0){
                reply_error_other(ses, pkg, "balance not enough");
            }
            //调整余额
            balance_sub(user_id, BALANCE_TYPE_AVAILABLE, market->money, change);
            //调整保证金
            mpd_add(position->principal, position->principal, change, &mpd_ctx);
            reply_success(ses, pkg);
        }else if(ret < 0){
            mpd_abs(change, change, &mpd_ctx);
            //检查保证金
            mpd_t *principal = mpd_new(&mpd_ctx);
            mpd_div(principal, position->position, position->leverage, &mpd_ctx);
            mpd_add(principal, principal, change, &mpd_ctx);
            if (!mpd_cmp(position->principal, principal, &mpd_ctx) < 0){
                reply_error_other(ses, pkg, "balance not principal");
            }
            //调整余额
            balance_add(user_id, BALANCE_TYPE_AVAILABLE, market->money, change);
            //调整保证金
            mpd_sub(position->principal, position->principal, change, &mpd_ctx);
            reply_success(ses, pkg);
        }else{
            reply_success(ses, pkg);
        }
    }
    else{
        return reply_error_other(ses, pkg, "position 不存在");
    }
}

static void svr_on_recv_pkg(nw_ses *ses, rpc_pkg *pkg)
{
    json_t *params = json_loadb(pkg->body, pkg->body_size, 0, NULL);
    if (params == NULL || !json_is_array(params))
    {
        goto decode_error;
    }
    sds params_str = sdsnewlen(pkg->body, pkg->body_size);

    int ret;
    switch (pkg->command)
    {
    case CMD_BALANCE_QUERY:
        log_trace("from: %s cmd balance query, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_balance_query(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_balance_query %s fail: %d", params_str, ret);
        }
        break;
    case CMD_BALANCE_UPDATE:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd balance update, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_balance_update(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_balance_update %s fail: %d", params_str, ret);
        }
        break;
    case CMD_BALANCE_UNFREEZE:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd balance unfreeze, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_balance_unfreeze(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_balance_unfreeze %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ASSET_LIST:
        log_trace("from: %s cmd asset list, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_asset_list(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_asset_list %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ASSET_SUMMARY:
        log_trace("from: %s cmd asset summary, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_asset_summary(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_asset_summary %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_PUSH_DEALS:
        log_trace("from: %s cmd push deals, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_push_deals(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_push_deals %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_PUT_LIMIT:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order put limit, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_put_limit(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_put_limit %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_PUT_LIMIT_BATCH:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order put limit batch, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_put_limit_batch(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_put_limit_batch %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_UPDATE_LIMIT:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order update limit, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_update_limit(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_update_limit %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_UPDATE_LIMIT_BATCH:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order update limit batch, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_update_limit_batch(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_update_limit_batch %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_PUT_MARKET:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order put market, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_put_market(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_put_market %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_QUERY:
        log_trace("from: %s cmd order query, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_query(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_query %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_QUERY_BRIEF:
        log_trace("from: %s cmd order query BRIEF, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_query_brief(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_query_brief %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_QUERY_ALL:
        log_trace("from: %s cmd order query all, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_query_all(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_query_all %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_CANCEL:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order cancel, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_cancel(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_cancel %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_CANCEL_BATCH:
        if (is_operlog_block() || is_history_block() || is_message_block())
        {
            log_fatal("service unavailable, operlog: %d, history: %d, message: %d",
                      is_operlog_block(), is_history_block(), is_message_block());
            reply_error_service_unavailable(ses, pkg);
            goto cleanup;
        }
        log_trace("from: %s cmd order cancel batch, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_cancel_batch(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_cancel_batch %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_BOOK:
        log_trace("from: %s cmd order book, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_book(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_book %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_BOOK_EXT:
        log_trace("from: %s cmd order book extra, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_book_ext(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_book_ext %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_BOOK_DEPTH:
        log_trace("from: %s cmd order book depth, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_book_depth(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_book_depth %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_DETAIL:
        log_trace("from: %s cmd order detail, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_detail(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_detail %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_LIST:
        log_trace("from: %s cmd market list, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_list(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_list %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_SUMMARY:
        log_trace("from: %s cmd market summary, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_summary(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_summary%s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_ADD_ASSET:
        log_trace("from: %s cmd market add asset, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_add_asset(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_add_asset %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_DEL_ASSET:
        log_trace("from: %s cmd market delete asset, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_del_asset(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_del_asset %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_ADD_MARKET:
        log_trace("from: %s cmd market add market, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_add_market(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_add_market %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MARKET_DEL_MARKET:
        log_trace("from: %s cmd market delete market, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_market_del_market(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_market_del_market %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MATCHENGINE_MAKE_SLICE:
        log_trace("from: %s cmd matchengine makeslice, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_make_slice(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_make_slice %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_OPEN:
        log_trace("from: %s cmd matchengine order open, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_open(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_open %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_CLOSE:
        log_trace("from: %s cmd matchengine order close, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_close(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_close %s fail: %d", params_str, ret);
        }
        break;
    case CMD_ORDER_QUERY_ALLUSER:
        log_trace("from: %s cmd order query alluser, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_order_query_alluser(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_order_query_alluser %s fail: %d", params_str, ret);
        }
        break;
    case CMD_POSITION_QUERY:
        log_trace("from: %s cmd matchengine position query, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_position_query(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_position_query %s fail: %d", params_str, ret);
        }
        break;
    case CMD_POSITION_QUERY_ALL:
        log_trace("from: %s cmd matchengine position query all, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_position_query_all(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_position_query_all %s fail: %d", params_str, ret);
        }
        break;
    case CMD_POSITION_ADJUST_PRINCIPAL:
        log_trace("from: %s cmd matchengine position adjust principal, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        ret = on_cmd_position_adjust_principal(ses, pkg, params);
        if (ret < 0)
        {
            log_error("on_cmd_position_adjust_principal %s fail: %d", params_str, ret);
        }
        break;
    case CMD_MATCHENGINE_SUICIDE:
        log_trace("from: %s cmd matchengine suicide, sequence: %u params: %s", nw_sock_human_addr(&ses->peer_addr), pkg->sequence, params_str);
        size_t request_size = json_array_size(params);
        if (request_size != 1)
            return reply_error_invalid_argument(ses, pkg);

        if (!json_is_integer(json_array_get(params, 0)))
            return reply_error_invalid_argument(ses, pkg);
        int status = json_integer_value(json_array_get(params, 0));
        reply_success(ses, pkg);
        sleep(1);//等待operlog数据写完
        exit(status);
        break;
    default:
        log_error("from: %s unknown command: %u", nw_sock_human_addr(&ses->peer_addr), pkg->command);
        break;
    }

cleanup:
    sdsfree(params_str);
    json_decref(params);
    return;

decode_error:
    if (params)
    {
        json_decref(params);
    }
    sds hex = hexdump(pkg->body, pkg->body_size);
    log_error("connection: %s, cmd: %u decode params fail, params data: \n%s",
              nw_sock_human_addr(&ses->peer_addr), pkg->command, hex);
    sdsfree(hex);
    rpc_svr_close_clt(svr, ses);

    return;
}

static void svr_on_new_connection(nw_ses *ses)
{
    log_trace("new connection: %s", nw_sock_human_addr(&ses->peer_addr));
}

static void svr_on_connection_close(nw_ses *ses)
{
    log_trace("connection: %s close", nw_sock_human_addr(&ses->peer_addr));
}

static uint32_t cache_dict_hash_function(const void *key)
{
    return dict_generic_hash_function(key, sdslen((sds)key));
}

static int cache_dict_key_compare(const void *key1, const void *key2)
{
    return sdscmp((sds)key1, (sds)key2);
}

static void *cache_dict_key_dup(const void *key)
{
    return sdsdup((const sds)key);
}

static void cache_dict_key_free(void *key)
{
    sdsfree(key);
}

static void *cache_dict_val_dup(const void *val)
{
    struct cache_val *obj = malloc(sizeof(struct cache_val));
    memcpy(obj, val, sizeof(struct cache_val));
    return obj;
}

static void cache_dict_val_free(void *val)
{
    struct cache_val *obj = val;
    json_decref(obj->result);
    free(val);
}

static void on_cache_timer(nw_timer *timer, void *privdata)
{
    dict_clear(dict_cache);
}

int init_server(void)
{
    rpc_svr_type type;
    memset(&type, 0, sizeof(type));
    type.on_recv_pkg = svr_on_recv_pkg;
    type.on_new_connection = svr_on_new_connection;
    type.on_connection_close = svr_on_connection_close;

    svr = rpc_svr_create(&settings.svr, &type);
    if (svr == NULL)
        return -__LINE__;
    if (rpc_svr_start(svr) < 0)
        return -__LINE__;

    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = cache_dict_hash_function;
    dt.key_compare = cache_dict_key_compare;
    dt.key_dup = cache_dict_key_dup;
    dt.key_destructor = cache_dict_key_free;
    dt.val_dup = cache_dict_val_dup;
    dt.val_destructor = cache_dict_val_free;

    dict_cache = dict_create(&dt, 64);
    if (dict_cache == NULL)
        return -__LINE__;

    nw_timer_set(&cache_timer, 60, true, on_cache_timer, NULL);
    nw_timer_start(&cache_timer);

    return 0;
}

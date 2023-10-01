/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/04, create
 */

# include "ut_mysql.h"
# include "me_trade.h"
# include "me_market.h"
# include "me_update.h"
# include "me_balance.h"
# include "me_position.h"

int load_orders(MYSQL *conn, const char *table)
{
    size_t query_limit = 1000;
    uint64_t last_id = 0;
    while (true) {
        sds sql = sdsempty();
        sql = sdscatprintf(sql, "SELECT \
        `id`, \
        `t`, \
        `side`, \
        `oper_type`, \
        `create_time`, \
        `update_time`, \
        `user_id`, \
        `market`, \
        `relate_order`, \
        `price`,\
        `amount`,\
        `leverage`, \
        `trigger`, \
        `taker_fee`, \
        `maker_fee`, \
        `left`, \
        `freeze`, \
        `deal_stock`, \
        `deal_money`, \
        `deal_fee`, \
        `source` FROM `%s` "
        "WHERE `id` > %"PRIu64" ORDER BY `id` LIMIT %zu", table, last_id, query_limit);
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret != 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
        sdsfree(sql);

        MYSQL_RES *result = mysql_store_result(conn);
        size_t num_rows = mysql_num_rows(result);
        for (size_t i = 0; i < num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(result);
            last_id = strtoull(row[0], NULL, 0);
            market_t *market = get_market(row[6]);
            if (market == NULL)
                continue;

            order_t *order = malloc(sizeof(order_t));
            memset(order, 0, sizeof(order_t));
            order->id = strtoull(row[0], NULL, 0);
            order->type = strtoul(row[1], NULL, 0);
            order->side = strtoul(row[2], NULL, 0);
            order->oper_type = strtoul(row[3], NULL, 0);
            order->create_time = strtod(row[4], NULL);
            order->update_time = strtod(row[5], NULL);
            order->user_id = strtoul(row[6], NULL, 0);
            order->market = strdup(row[7]);
            order->relate_order = strtoull(row[8], NULL, 0);
            order->price = decimal(row[9], market->money_prec);
            order->amount = decimal(row[10], market->stock_prec);
            order->leverage = decimal(row[11], market->stock_prec);
            order->trigger = decimal(row[12], market->stock_prec);
            order->taker_fee = decimal(row[13], market->fee_prec);
            order->maker_fee = decimal(row[14], market->fee_prec);
            order->left = decimal(row[15], market->stock_prec);
            order->freeze = decimal(row[16], 0);
            order->deal_stock = decimal(row[17], 0);
            order->deal_money = decimal(row[18], 0);
            order->deal_fee = decimal(row[19], 0);
            order->source = strdup(row[20]);
            // order->mm = (strncmp(order->source, MM_SOURCE_STR, MM_SOURCE_STR_LEN) == 0) ? true : false;
            order->mm = get_mm_order_type_by_source(order->source);

            if (!order->market || !order->source || !order->price || !order->amount || !order->taker_fee || !order->maker_fee || !order->left ||
                    !order->freeze || !order->deal_stock || !order->deal_money || !order->deal_fee) {
                log_error("get order detail of order id: %"PRIu64" fail", order->id);
                mysql_free_result(result);
                return -__LINE__;
            }

            market_put_order(market, order);
        }
        mysql_free_result(result);

        if (num_rows < query_limit)
            break;
    }

    return 0;
}

int load_balance(MYSQL *conn, const char *table)
{
    size_t query_limit = 1000;
    uint64_t last_id = 0;
    while (true) {
        sds sql = sdsempty();
        sql = sdscatprintf(sql, "SELECT `id`, `user_id`, `asset`, `t`, `balance` FROM `%s` "
                "WHERE `id` > %"PRIu64" ORDER BY id LIMIT %zu", table, last_id, query_limit);
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret != 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
        sdsfree(sql);

        MYSQL_RES *result = mysql_store_result(conn);
        size_t num_rows = mysql_num_rows(result);
        for (size_t i = 0; i < num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(result);
            last_id = strtoull(row[0], NULL, 0);
            uint32_t user_id = strtoul(row[1], NULL, 0);
            const char *asset = row[2];
            if (!asset_exist(asset)) {
                continue;
            }
            uint32_t type = strtoul(row[3], NULL, 0);
            mpd_t *balance = decimal(row[4], asset_prec(asset));
            balance_set(user_id, type, asset, balance);
        }
        mysql_free_result(result);

        if (num_rows < query_limit)
            break;
    }

    return 0;
}

int load_position(MYSQL *conn, const char *table)
{
    size_t query_limit = 1000;
    uint64_t last_id = 0;
    while (true) {
        sds sql = sdsempty();
        sql = sdscatprintf(sql, "SELECT \
        `id`, \
        `user_id`, \
        `market`, \
        `side`, \
        `pattern`, \
        `leverage`, \
        `position`, \
        `frozen`, \
        `price`, \
        `principal` FROM `%s` "
                "WHERE `id` > %"PRIu64" ORDER BY id LIMIT %zu", table, last_id, query_limit);
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret != 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
        sdsfree(sql);

        MYSQL_RES *result = mysql_store_result(conn);
        size_t num_rows = mysql_num_rows(result);
        for (size_t i = 0; i < num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(result);
            last_id = strtoull(row[0], NULL, 0);
            uint32_t user_id = strtoul(row[1], NULL, 0);
            market_t *market = get_market(row[6]);
            if (market == NULL)
                continue;

            position_t *position = malloc(sizeof(position_t));
            memset(position, 0, sizeof(position_t));
            position->id = strtoull(row[0], NULL, 0);
            position->user_id = strtoull(row[1], NULL, 0);
            position->market = strdup(row[2]);
            position->side = strtoul(row[3], NULL, 0);
            position->pattern = strtoul(row[4], NULL, 0);
            position->leverage = decimal(row[5], 0);
            position->position = decimal(row[6], 0);
            position->frozen = decimal(row[7], 0);
            position->price = decimal(row[8], 0);
            position->principal = decimal(row[9], 0);
            add_position(user_id, position->market, position->side, position);
        }
        mysql_free_result(result);

        if (num_rows < query_limit)
            break;
    }

    return 0;
}

static int load_update_balance(json_t *params)
{
    if (json_array_size(params) != 6)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // asset
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *asset = json_string_value(json_array_get(params, 1));
    int prec = asset_prec(asset);
    if (prec < 0)
        return 0;

    // business
    if (!json_is_string(json_array_get(params, 2)))
        return -__LINE__;
    const char *business = json_string_value(json_array_get(params, 2));

    // business_id
    if (!json_is_integer(json_array_get(params, 3)))
        return -__LINE__;
    uint64_t business_id = json_integer_value(json_array_get(params, 3));

    // change
    if (!json_is_string(json_array_get(params, 4)))
        return -__LINE__;
    mpd_t *change = decimal(json_string_value(json_array_get(params, 4)), prec);
    if (change == NULL)
        return -__LINE__;

    // detail
    json_t *detail = json_array_get(params, 5);
    if (!json_is_object(detail)) {
        mpd_del(change);
        return -__LINE__;
    }

    int ret = update_user_balance(false, user_id, asset, business, business_id, change, detail);
    mpd_del(change);

    if (ret < 0) {
        return -__LINE__;
    }

    return 0;
}

static int load_limit_order(json_t *params)
{
    if (json_array_size(params) != 8)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    // side
    if (!json_is_integer(json_array_get(params, 2)))
        return -__LINE__;
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return -__LINE__;

    mpd_t *amount = NULL;
    mpd_t *price  = NULL;
    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 3)))
        goto error;
    amount = decimal(json_string_value(json_array_get(params, 3)), market->stock_prec);
    if (amount == NULL)
        goto error;
    if (mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto error;

    // price 
    if (!json_is_string(json_array_get(params, 4)))
        goto error;
    price = decimal(json_string_value(json_array_get(params, 4)), market->money_prec);
    if (price == NULL) 
        goto error;
    if (mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
        goto error;

    // taker fee
    if (!json_is_string(json_array_get(params, 5)))
        goto error;
    taker_fee = decimal(json_string_value(json_array_get(params, 5)), market->fee_prec);
    if (taker_fee == NULL)
        goto error;
    if (mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // maker fee
    if (!json_is_string(json_array_get(params, 6)))
        goto error;
    maker_fee = decimal(json_string_value(json_array_get(params, 6)), market->fee_prec);
    if (maker_fee == NULL)
        goto error;
    if (mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // source
    if (!json_is_string(json_array_get(params, 7)))
        goto error;
    const char *source = json_string_value(json_array_get(params, 7));
    if (strlen(source) > SOURCE_MAX_LEN)
        goto error;

    int ret = market_put_limit_order(false, NULL, market, user_id, side, amount, price, taker_fee, maker_fee, source);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(taker_fee);
    mpd_del(maker_fee);

    return ret;

error:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return -__LINE__;
}

static int load_update_limit_order(json_t *params)
{
    if (json_array_size(params) != 9)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    // cancel order_id
    if (!json_is_integer(json_array_get(params, 2)))
        return -__LINE__;
    uint64_t order_id = json_integer_value(json_array_get(params, 2));

    // side
    if (!json_is_integer(json_array_get(params, 3)))
        return -__LINE__;
    uint32_t side = json_integer_value(json_array_get(params, 3));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return -__LINE__;

    mpd_t *amount = NULL;
    mpd_t *price  = NULL;
    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 4)))
        goto error;
    amount = decimal(json_string_value(json_array_get(params, 4)), market->stock_prec);
    if (amount == NULL)
        goto error;
    if (mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto error;

    // price
    if (!json_is_string(json_array_get(params, 5)))
        goto error;
    price = decimal(json_string_value(json_array_get(params, 5)), market->money_prec);
    if (price == NULL)
        goto error;
    if (mpd_cmp(price, mpd_zero, &mpd_ctx) <= 0)
        goto error;

    // taker fee
    if (!json_is_string(json_array_get(params, 6)))
        goto error;
    taker_fee = decimal(json_string_value(json_array_get(params, 6)), market->fee_prec);
    if (taker_fee == NULL)
        goto error;
    if (mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // maker fee
    if (!json_is_string(json_array_get(params, 7)))
        goto error;
    maker_fee = decimal(json_string_value(json_array_get(params, 7)), market->fee_prec);
    if (maker_fee == NULL)
        goto error;
    if (mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // source
    if (!json_is_string(json_array_get(params, 8)))
        goto error;
    const char *source = json_string_value(json_array_get(params, 8));
    if (strlen(source) > SOURCE_MAX_LEN)
        goto error;

    order_t *order = market_get_order(market, order_id);
    if (order != NULL && order->user_id != user_id) {
        market_cancel_order(false, NULL, market, order);
    }

    int ret = market_put_limit_order(false, NULL, market, user_id, side, amount, price, taker_fee, maker_fee, source);

    mpd_del(amount);
    mpd_del(price);
    mpd_del(taker_fee);
    mpd_del(maker_fee);

    return ret;

error:
    if (amount)
        mpd_del(amount);
    if (price)
        mpd_del(price);
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return -__LINE__;
}

static int load_market_order(json_t *params)
{
    if (json_array_size(params) != 6)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    // side
    if (!json_is_integer(json_array_get(params, 2)))
        return -__LINE__;
    uint32_t side = json_integer_value(json_array_get(params, 2));
    if (side != MARKET_ORDER_SIDE_ASK && side != MARKET_ORDER_SIDE_BID)
        return -__LINE__;

    mpd_t *amount = NULL;
    mpd_t *taker_fee = NULL;

    // amount
    if (!json_is_string(json_array_get(params, 3)))
        goto error;
    amount = decimal(json_string_value(json_array_get(params, 3)), market->stock_prec);
    if (amount == NULL)
        goto error;
    if (mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        goto error;

    // taker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto error;
    taker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (taker_fee == NULL)
        goto error;
    if (mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // source
    if (!json_is_string(json_array_get(params, 5)))
        goto error;
    const char *source = json_string_value(json_array_get(params, 5));
    if (strlen(source) > SOURCE_MAX_LEN)
        goto error;

    int ret = market_put_market_order(false, NULL, market, user_id, side, amount, taker_fee, source);

    mpd_del(amount);
    mpd_del(taker_fee);

    return ret;

error:
    if (amount)
        mpd_del(amount);
    if (taker_fee)
        mpd_del(taker_fee);

    return -__LINE__;
}

static int load_limit_order_batch(json_t *params)
{
    if (json_array_size(params) != 5)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    // amount price side list
    json_t *aps_list = json_array_get(params, 2);
    if (!json_is_array(aps_list))
        return -__LINE__;

    size_t a_size = json_array_size(aps_list);
    if (a_size > PUT_LIMIT_ORDER_BATCH_MAX_NUM)
        return -__LINE__;

    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;
    // taker fee
    if (!json_is_string(json_array_get(params, 3)))
        goto error;
    taker_fee = decimal(json_string_value(json_array_get(params, 3)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // maker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto error;
    maker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // // source
    // if (!json_is_string(json_array_get(params, 5)))
    //     goto error;
    // const char *source = json_string_value(json_array_get(params, 5));
    // if (strlen(source) >= SOURCE_MAX_LEN)
    //     goto error;

    for (uint32_t i = 0; i < a_size; ++i) {
        market_put_limit_order_extra(false, NULL, market, user_id, json_array_get(aps_list, i), taker_fee, maker_fee);
    }

    mpd_del(taker_fee);
    mpd_del(maker_fee);

    return 0;

error:
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return -__LINE__;
}

static int load_update_limit_order_batch(json_t *params)
{
    if (json_array_size(params) != 6)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    json_t *order_id_list = json_array_get(params, 2);
    if (!json_is_array(order_id_list))
        return -__LINE__;

    // amount price side list
    json_t *aps_list = json_array_get(params, 3);
    if (!json_is_array(aps_list))
        return -__LINE__;

    size_t a_size = json_array_size(aps_list);
    if (a_size > PUT_LIMIT_ORDER_BATCH_MAX_NUM)
        return -__LINE__;

    mpd_t *taker_fee = NULL;
    mpd_t *maker_fee = NULL;
    // taker fee
    if (!json_is_string(json_array_get(params, 4)))
        goto error;
    taker_fee = decimal(json_string_value(json_array_get(params, 4)), market->fee_prec);
    if (taker_fee == NULL || mpd_cmp(taker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(taker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // maker fee
    if (!json_is_string(json_array_get(params, 5)))
        goto error;
    maker_fee = decimal(json_string_value(json_array_get(params, 5)), market->fee_prec);
    if (maker_fee == NULL || mpd_cmp(maker_fee, mpd_zero, &mpd_ctx) < 0 || mpd_cmp(maker_fee, mpd_one, &mpd_ctx) >= 0)
        goto error;

    // // source
    // if (!json_is_string(json_array_get(params, 6)))
    //     goto error;
    // const char *source = json_string_value(json_array_get(params, 6));
    // if (strlen(source) >= SOURCE_MAX_LEN)
    //     goto error;

    // cancel orders list
    if (json_array_size(order_id_list) > 0) {
        for (uint32_t i = 0; i < json_array_size(order_id_list); ++i) {
            json_t *row = json_array_get(order_id_list, i);
            if (!json_is_integer(row))
                continue;

            uint64_t o_id = json_integer_value(row);
            order_t *order = market_get_order(market, o_id);
            if (order == NULL) {
                continue;
            }
            if (order->user_id != user_id) {
                continue;
            }

            market_cancel_order(false, NULL, market, order);
        }
    }

    for (uint32_t i = 0; i < a_size; ++i) {
        market_put_limit_order_extra(false, NULL, market, user_id, json_array_get(aps_list, i), taker_fee, maker_fee);
    }

    mpd_del(taker_fee);
    mpd_del(maker_fee);

    return 0;

error:
    if (taker_fee)
        mpd_del(taker_fee);
    if (maker_fee)
        mpd_del(maker_fee);

    return -__LINE__;
}

static int load_cancel_order(json_t *params)
{
    if (json_array_size(params) != 3)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    // order_id
    if (!json_is_integer(json_array_get(params, 2)))
        return -__LINE__;
    uint64_t order_id = json_integer_value(json_array_get(params, 2));

    order_t *order = market_get_order(market, order_id);
    if (order == NULL) {
        return -__LINE__;
    }

    int ret = market_cancel_order(false, NULL, market, order);
    if (ret < 0) {
        log_error("market_cancel_order id: %"PRIu64", user id: %u, market: %s", order_id, user_id, market_name);
        return -__LINE__;
    }

    return 0;
}

static int load_cancel_order_batch(json_t *params)
{
    int param_size = json_array_size(params);
    if (param_size != 3 && param_size != 2)
        return -__LINE__;

    // user_id
    if (!json_is_integer(json_array_get(params, 0)))
        return -__LINE__;
    uint32_t user_id = json_integer_value(json_array_get(params, 0));

    // market
    if (!json_is_string(json_array_get(params, 1)))
        return -__LINE__;
    const char *market_name = json_string_value(json_array_get(params, 1));
    market_t *market = get_market(market_name);
    if (market == NULL)
        return 0;

    if (param_size == 2) { // cancel all
        market_get_order_list(market, user_id);
        skiplist_t *order_list = market_get_order_list(market, user_id);
        if (order_list) {
            skiplist_iter *iter = skiplist_get_iterator(order_list);
            skiplist_node *node;
            while ((node = skiplist_next(iter)) != NULL) {
                market_cancel_order(false, NULL, market, node->value);
            }
            skiplist_release_iterator(iter);
        }
    } else { // cancel by order id list
        json_t *order_id_list = json_array_get(params, 2);
        if (!json_is_array(order_id_list))
            return 0;

        for (uint32_t i = 0; i < json_array_size(order_id_list); ++i) {
            json_t *row = json_array_get(order_id_list, i);
            if (!json_is_integer(row))
                continue;

            uint64_t o_id = json_integer_value(row);
            order_t *order = market_get_order(market, o_id);
            if (order == NULL) {
                continue;
            }
            if (order->user_id != user_id) {
                continue;
            }

            market_cancel_order(false, NULL, market, order);
        }
    }

    return 0;
}

static int load_oper(json_t *detail)
{
    const char *method = json_string_value(json_object_get(detail, "method"));
    if (method == NULL)
        return -__LINE__;
    json_t *params = json_object_get(detail, "params");
    if (params == NULL || !json_is_array(params))
        return -__LINE__;

    int ret = 0;
    if (strcmp(method, "update_balance") == 0) {
        ret = load_update_balance(params);
    } else if (strcmp(method, "limit_order") == 0) {
        ret = load_limit_order(params);
    } else if (strcmp(method, "market_order") == 0) {
        ret = load_market_order(params);
    } else if (strcmp(method, "cancel_order") == 0) {
        ret = load_cancel_order(params);
    } else if (strcmp(method, "cancel_order_batch") == 0){
        ret = load_cancel_order_batch(params);
    } else if (strcmp(method, "limit_order_batch") == 0){
        ret = load_limit_order_batch(params);
    } else if (strcmp(method, "update_limit_order") == 0){
        ret = load_update_limit_order(params);
    } else if (strcmp(method, "update_limit_order_batch") == 0){
        ret = load_update_limit_order_batch(params);
    } else {
        return -__LINE__;
    }

    return ret;
}

int load_operlog(MYSQL *conn, const char *table, uint64_t *start_id)
{
    size_t query_limit = 1000;
    uint64_t last_id = *start_id;
    while (true) {
        sds sql = sdsempty();
        sql = sdscatprintf(sql, "SELECT `id`, `detail` from `%s` WHERE `id` > %"PRIu64" ORDER BY `id` LIMIT %zu", table, last_id, query_limit);
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret != 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
        sdsfree(sql);

        MYSQL_RES *result = mysql_store_result(conn);
        size_t num_rows = mysql_num_rows(result);
        for (size_t i = 0; i < num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(result);
            uint64_t id = strtoull(row[0], NULL, 0);
            if (id != last_id + 1) {
                log_error("invalid id: %"PRIu64", last id: %"PRIu64"", id, last_id);
                return -__LINE__;
            }
            last_id = id;
            json_t *detail = json_loadb(row[1], strlen(row[1]), 0, NULL);
            if (detail == NULL) {
                log_error("invalid detail data: %s", row[1]);
                mysql_free_result(result);
                return -__LINE__;
            }
            ret = load_oper(detail);
            if (ret < 0) {
                json_decref(detail);
                log_error("load_oper: %"PRIu64":%s fail: %d", id, row[1], ret);
                mysql_free_result(result);
                return -__LINE__;
            }
            json_decref(detail);
        }
        mysql_free_result(result);

        if (num_rows < query_limit)
            break;
    }

    *start_id = last_id;
    return 0;
}


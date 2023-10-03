/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/04, create
 */

# include "ut_mysql.h"
# include "me_trade.h"
# include "me_market.h"
# include "me_balance.h"
# include "me_position.h"

static sds sql_append_mpd(sds sql, mpd_t *val, bool comma)
{
    char *str = mpd_to_sci(val, 0);
    sql = sdscatprintf(sql, "'%s'", str);
    if (comma) {
        sql = sdscatprintf(sql, ", ");
    }
    free(str);
    return sql;
}

static int dump_orders_list(MYSQL *conn, const char *table, skiplist_t *list)
{
    log_trace("%s", __FUNCTION__);
    sds sql = sdsempty();

    size_t insert_limit = 1000;
    size_t index = 0;
    skiplist_iter *iter = skiplist_get_iterator(list);
    skiplist_node *node;
    while ((node = skiplist_next(iter)) != NULL) {
        order_t *order = node->value;
        if (index == 0) {
            sql = sdscatprintf(sql, "INSERT INTO `%s` (`id`, `t`, `side`, `oper_type`, `create_time`, `update_time`, `user_id`, `market`, `relate_order`, `source`,"
                "`price`, `amount`, `leverage`, `trigger`, `taker_fee`, `maker_fee`, `left`, `freeze`, `deal_stock`, `deal_money`, `deal_fee`) VALUES ", table);
        } else {
            sql = sdscatprintf(sql, ", ");
        }

        sql = sdscatprintf(sql, "(%"PRIu64", %u, %u, %u, %f, %f, %u, '%s', '%u', '%s', ",
                order->id, order->type, order->side, order->oper_type, order->create_time, order->update_time, order->user_id, order->market, order->relate_order, order->source);
        sql = sql_append_mpd(sql, order->price, true);
        sql = sql_append_mpd(sql, order->amount, true);
        sql = sql_append_mpd(sql, order->leverage, true);
        sql = sql_append_mpd(sql, order->trigger, true);
        sql = sql_append_mpd(sql, order->taker_fee, true);
        sql = sql_append_mpd(sql, order->maker_fee, true);
        sql = sql_append_mpd(sql, order->left, true);
        sql = sql_append_mpd(sql, order->freeze, true);
        sql = sql_append_mpd(sql, order->deal_stock, true);
        sql = sql_append_mpd(sql, order->deal_money, true);
        sql = sql_append_mpd(sql, order->deal_fee, false);
        sql = sdscatprintf(sql, ")");

        index += 1;
        if (index == insert_limit) {
            log_trace("exec sql: %s", sql);
            int ret = mysql_real_query(conn, sql, sdslen(sql));
            if (ret < 0) {
                log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
                skiplist_release_iterator(iter);
                sdsfree(sql);
                return -__LINE__;
            }
            sdsclear(sql);
            index = 0;
        }
    }
    skiplist_release_iterator(iter);

    if (index > 0) {
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret < 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
    }

    sdsfree(sql);
    return 0;
}

int dump_orders(MYSQL *conn, const char *table)
{
    log_trace("%s", __FUNCTION__);
    sds sql = sdsempty();
    sql = sdscatprintf(sql, "DROP TABLE IF EXISTS `%s`", table);
    log_trace("exec sql: %s", sql);
    int ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsclear(sql);

    sql = sdscatprintf(sql, "CREATE TABLE IF NOT EXISTS `%s` LIKE `slice_order_example`", table);
    log_trace("exec sql: %s", sql);
    ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsfree(sql);
    log_trace("%s", __FUNCTION__);
    for (int i = 0; i < settings.market_num; ++i) {
        market_t *market = get_market(settings.markets[i].name);
        if (market == NULL) {
            return -__LINE__;
        }
        int ret;
        log_trace("%s", __FUNCTION__);
        ret = dump_orders_list(conn, table, market->asks);
        if (ret < 0) {
            log_error("dump market: %s asks orders list fail: %d", market->name, ret);
            return -__LINE__;
        }
        log_trace("%s", __FUNCTION__);
        ret = dump_orders_list(conn, table, market->bids);
        if (ret < 0) {
            log_error("dump market: %s bids orders list fail: %d", market->name, ret);
            return -__LINE__;
        }
    }

    return 0;
}

static int dump_balance_dict(MYSQL *conn, const char *table, dict_t *dict)
{
    sds sql = sdsempty();

    size_t insert_limit = 1000;
    size_t index = 0;
    dict_iterator *iter = dict_get_iterator(dict);
    dict_entry *entry;
    while ((entry = dict_next(iter)) != NULL) {
        struct balance_key *key = entry->key;
        mpd_t *balance = entry->val;
        if (index == 0) {
            sql = sdscatprintf(sql, "INSERT INTO `%s` (`id`, `user_id`, `asset`, `t`, `balance`) VALUES ", table);
        } else {
            sql = sdscatprintf(sql, ", ");
        }

        sql = sdscatprintf(sql, "(NULL, %u, '%s', %u, ", key->user_id, key->asset, key->type);
        sql = sql_append_mpd(sql, balance, false);
        sql = sdscatprintf(sql, ")");

        index += 1;
        if (index == insert_limit) {
            log_trace("exec sql: %s", sql);
            int ret = mysql_real_query(conn, sql, sdslen(sql));
            if (ret < 0) {
                log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
                dict_release_iterator(iter);
                sdsfree(sql);
                return -__LINE__;
            }
            sdsclear(sql);
            index = 0;
        }
    }
    dict_release_iterator(iter);

    if (index > 0) {
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret < 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
    }

    sdsfree(sql);
    return 0;
}

int dump_balance(MYSQL *conn, const char *table)
{
    sds sql = sdsempty();
    sql = sdscatprintf(sql, "DROP TABLE IF EXISTS `%s`", table);
    log_trace("exec sql: %s", sql);
    int ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsclear(sql);

    sql = sdscatprintf(sql, "CREATE TABLE IF NOT EXISTS `%s` LIKE `slice_balance_example`", table);
    log_trace("exec sql: %s", sql);
    ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsfree(sql);

    ret = dump_balance_dict(conn, table, dict_balance);
    if (ret < 0) {
        log_error("dump_balance_dict fail: %d", ret);
        return -__LINE__;
    }

    return 0;
}

static int dump_position_dict(MYSQL *conn, const char *table, dict_t *dict)
{
    sds sql = sdsempty();

    size_t insert_limit = 1000;
    size_t index = 0;
    dict_iterator *iter = dict_get_iterator(dict);
    dict_entry *entry;
    while ((entry = dict_next(iter)) != NULL) {
        struct position_key *key = entry->key;
        position_t *position = entry->val;
        if (index == 0) {
            sql = sdscatprintf(sql, "INSERT INTO `%s` (`id`, `user_id`, `market`, `side`, `pattern`, `leverage`, `position`, `frozen`, `price`, `principal`) VALUES ", table);
        } else {
            sql = sdscatprintf(sql, ", ");
        }

        sql = sdscatprintf(sql, "(NULL, %u, '%s', %u, %u, ", key->user_id, key->market, key->side, position->pattern);
        sql = sql_append_mpd(sql, position->leverage, true);
        sql = sql_append_mpd(sql, position->position, true);
        sql = sql_append_mpd(sql, position->frozen, true);
        sql = sql_append_mpd(sql, position->price, true);
        sql = sql_append_mpd(sql, position->principal, false);
        sql = sdscatprintf(sql, ")");

        index += 1;
        if (index == insert_limit) {
            log_trace("exec sql: %s", sql);
            int ret = mysql_real_query(conn, sql, sdslen(sql));
            if (ret < 0) {
                log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
                dict_release_iterator(iter);
                sdsfree(sql);
                return -__LINE__;
            }
            sdsclear(sql);
            index = 0;
        }
    }
    dict_release_iterator(iter);

    if (index > 0) {
        log_trace("exec sql: %s", sql);
        int ret = mysql_real_query(conn, sql, sdslen(sql));
        if (ret < 0) {
            log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
            sdsfree(sql);
            return -__LINE__;
        }
    }

    sdsfree(sql);
    return 0;
}

int dump_position(MYSQL *conn, const char *table)
{
    sds sql = sdsempty();
    sql = sdscatprintf(sql, "DROP TABLE IF EXISTS `%s`", table);
    log_trace("exec sql: %s", sql);
    int ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsclear(sql);

    sql = sdscatprintf(sql, "CREATE TABLE IF NOT EXISTS `%s` LIKE `slice_position_example`", table);
    log_trace("exec sql: %s", sql);
    ret = mysql_real_query(conn, sql, sdslen(sql));
    if (ret != 0) {
        log_error("exec sql: %s fail: %d %s", sql, mysql_errno(conn), mysql_error(conn));
        sdsfree(sql);
        return -__LINE__;
    }
    sdsfree(sql);

    ret = dump_position_dict(conn, table, dict_position);
    if (ret < 0) {
        log_error("dump_position_dict fail: %d", ret);
        return -__LINE__;
    }

    return 0;
}


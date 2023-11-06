/*
 * Description:
 *     History: yang@haipo.me, 2017/03/16, create
 */

#ifndef _ME_MARKET_H_
#define _ME_MARKET_H_

#include <stdbool.h>
#include "me_config.h"

extern uint64_t order_id_start;
extern uint64_t deals_id_start;
extern dict_t *user_orders;
#define MM_SOURCE_STR "mm" // mm order, do not save db
#define MM_SOURCE_STR_LEN 2
#define MM_SOURCE_STR_CANCEL "mmc" // mm order cancel auto, do not save db
#define MM_SOURCE_STR_CANCEL_LEN 3
#define PUT_LIMIT_ORDER_BATCH_MAX_NUM 300

#define MM_ORDER_TYPE_NORMAL 1
#define MM_ORDER_TYPE_CANCEL 2

#define MAX_ASSET_PREC_SHOW 13
#define MAX_ASSET_PREC_SAVE 16
#define MAX_MARKET_PREC_FEE 5

typedef struct order_t
{
    uint64_t id;
    uint32_t type;
    uint32_t isblast;
    uint32_t side;
    uint32_t pattern;
    uint32_t oper_type;
    double create_time;
    double update_time;
    uint32_t user_id;
    char *market;
    uint64_t relate_order;
    mpd_t *price;
    mpd_t *amount;
    mpd_t *leverage;
    mpd_t *trigger;
    mpd_t *current_price;
    mpd_t *taker_fee;
    mpd_t *maker_fee;
    mpd_t *left;
    mpd_t *freeze;
    mpd_t *deal_stock;
    mpd_t *deal_money;
    mpd_t *pnl; // 平仓 产生的利润
    mpd_t *deal_fee;
    char *source;
    int mm; // market maker type
} order_t;

typedef struct market_t
{
    char *name;
    char *stock;
    char *money;

    int stock_prec;
    int money_prec;
    int fee_prec;
    mpd_t *min_amount;
    mpd_t *latestPrice;
    dict_t *orders;
    dict_t *users;

    skiplist_t *asks;
    skiplist_t *bids;
    skiplist_t *plan_asks;
    skiplist_t *plan_bids;
} market_t;

market_t *market_create(struct market *conf);
int market_get_status(market_t *m, size_t *ask_count, mpd_t *ask_amount, size_t *bid_count, mpd_t *bid_amount);

int market_put_limit_order(bool real, json_t **result, market_t *m, uint32_t user_id, uint32_t side, mpd_t *amount, mpd_t *price, mpd_t *taker_fee, mpd_t *maker_fee, const char *source);
int market_put_market_order(bool real, json_t **result, market_t *m, uint32_t user_id, uint32_t side, mpd_t *amount, mpd_t *taker_fee, const char *source);
int market_cancel_order(bool real, json_t **result, market_t *m, order_t *order);
// int market_put_limit_order_extra(bool real, json_t *result, market_t *m, uint32_t user_id, json_t *row, mpd_t *taker_fee, mpd_t *maker_fee, const char *source);
int market_put_limit_order_extra(bool real, json_t *result, market_t *m, uint32_t user_id, json_t *row, mpd_t *taker_fee, mpd_t *maker_fee);
int market_cancel_all_order(market_t *m);
int market_put_order(market_t *m, order_t *order);

<<<<<<< HEAD
int check_position_order_mode(uint32_t user_id, market_t *market);
int market_put_order_common(void*);
int market_put_order_open(void*);
int market_put_order_close(void*);
=======
int market_put_order_common(void *);
int market_put_order_open(void *);
int market_put_order_close(void *);
>>>>>>> a99cb7e (fix create_trade_history.sql)
int checkPriAndFee(uint32_t pattern, uint32_t user_id, mpd_t *balance, mpd_t *priAndFee);

json_t *get_order_info(order_t *order);
json_t *get_order_info_brief(order_t *order);
order_t *market_get_order(market_t *m, uint64_t id);
skiplist_t *market_get_order_list(market_t *m, uint32_t user_id);
skiplist_t *market_get_user_order_list(uint32_t user_id);
int user_orders_list_create();
int get_mm_order_type_by_source(const char *source);

sds market_status(sds reply);

#endif

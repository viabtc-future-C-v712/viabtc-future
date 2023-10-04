# ifndef _ME_ARGS_H_
# define _ME_ARGS_H_

# include "me_market.h"

typedef struct args_t{
    uint32_t user_id;
    market_t * market;
    uint32_t direction;// 0：买（平空）  1：卖（平多）
    uint32_t Type;// 0：市价 1：限价 2：计划委托
    uint32_t pattern;// 1 逐仓, 2 全仓
    mpd_t *markPrice;
    mpd_t *triggerPrice;
    mpd_t *entrustPrice;
    mpd_t *leverage;
    mpd_t *volume;
    mpd_t *taker_fee_rate;
    mpd_t *maker_fee_rate;
    uint32_t real;
    uint32_t bOpen;
    mpd_t *priAmount;
    mpd_t *fee;
    mpd_t *priAndFee;
    order_t *taker;
    order_t *maker;
    char *msg;
}args_t;

args_t* initOpenArgs(json_t *params);

args_t* initCloseArgs(json_t *params);
void test();
# endif
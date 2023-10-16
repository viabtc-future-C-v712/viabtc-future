# ifndef _ME_ARGS_H_
# define _ME_ARGS_H_

# include "me_market.h"

#define BEAR    1
#define BULL    2

#define MARKET 0
#define LIMIT 1
#define ENTRUST 2

#define CROSS_MARGIN    2
#define ISOLATED_MARGIN  1

typedef struct args_t{
    uint32_t user_id;
    market_t * market;
    uint32_t direction;// 1：空(BEAR)  2：多(BULL)
    uint32_t Type;// 0：市价(MARKET) 1：限价(LIMIT) 2：计划委托(ENTRUST)
    uint32_t pattern;// 1 逐仓(ISOLATED_MARGIN), 2 全仓(CROSS_MARGIN)
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
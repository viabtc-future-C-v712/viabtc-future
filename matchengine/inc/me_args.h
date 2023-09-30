# ifndef _ME_ARGS_H_
# define _ME_D_ME_ARGS_H_EAL_H_

# include "me_config.h"
# include "me_market.h"
typedef struct args_t{
    int user_id;
    market_t * market;
    int direction;// 0：买（平空）  1：卖（平多）
    int type;// 0：市价 1：限价 2：计划委托
    mpd_t *markPrice;
    mpd_t *triggerPrice;
    mpd_t *entrustPrice;
    mpd_t *leverage;
    mpd_t *volume;
    mpd_t *taker_fee_rate;
    mpd_t *maker_fee_rate;
    int real;
    int bOpen;
    mpd_t *priAmount;
    mpd_t *fee;
    mpd_t *priAndFee;
    order_t *taker;
    order_t *maker;
}args_t;

# endif
# ifndef _ME_DEAL_H_
# define _ME_DEAL_H_

# include "me_config.h"
# include "me_args.h"

typedef struct deal_t{
    args_t *args;
    mpd_t *amount;
    mpd_t *price;
    mpd_t *taker_fee;
    mpd_t *maker_fee;
    mpd_t *deal; // 成交额
}deal_t;

args_t* initOpenArgs(json_t *params);

args_t* initCloseArgs(json_t *params);

# endif
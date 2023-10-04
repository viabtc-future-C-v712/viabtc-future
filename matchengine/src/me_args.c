#include "me_args.h"
#include "me_trade.h"

#define CODE_PIECES_CHECK_JSON_STRING(X,Y,Z) \
    if (!json_is_string(json_array_get(X, Y))) \
        return NULL; \
    const char *Z = json_string_value(json_array_get(params, Y));

#define CODE_PIECES_CHECK_JSON_INTEGER(X,Y,Z) \
    if (!json_is_integer(json_array_get(X, Y))) \
        return NULL; \
    int Z = json_integer_value(json_array_get(params, Y));

#define CODE_PIECES_CHECK_JSON_MDP(X,Y,Z) \
    mpd_t *Z = decimal(json_string_value(json_array_get(X, Y)), 0); \
    if (!Z) \
        return NULL;

#define CODE_PIECES_CHECK_MARKET(X,Y) \
    market_t *Y = get_market(X); \
    if (Y == NULL) \
        return NULL;

args_t* initOpenArgs(json_t *params){
    if (json_array_size(params) != 12) return NULL;
    CODE_PIECES_CHECK_JSON_INTEGER(params, 0, user_id)
    CODE_PIECES_CHECK_JSON_STRING(params, 1, market_name)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 2, direction)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 3, type)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 4, pattern)
    CODE_PIECES_CHECK_JSON_MDP(params, 5, markPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 6, triggerPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 7, entrustPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 8, leverage)
    CODE_PIECES_CHECK_JSON_MDP(params, 9, volume)
    CODE_PIECES_CHECK_JSON_MDP(params, 10, taker_fee)
    CODE_PIECES_CHECK_JSON_MDP(params, 11, maker_fee)

    // 检查币对
    CODE_PIECES_CHECK_MARKET(market_name, market)
    args_t *args = (args_t*)malloc(sizeof(args_t));
    args->user_id = user_id;
    args->market = market;
    args->direction = direction;
    args->Type = type;
    args->pattern = pattern;
    args->markPrice = markPrice;
    args->triggerPrice = triggerPrice;
    args->entrustPrice = entrustPrice;
    args->leverage = leverage;
    args->volume = volume;
    args->taker_fee_rate = taker_fee;
    args->maker_fee_rate = maker_fee;
    args->priAmount = mpd_new(&mpd_ctx);
    args->fee = mpd_new(&mpd_ctx);
    args->priAndFee = mpd_new(&mpd_ctx);
    args->msg = "";
    return args;
}

args_t* initCloseArgs(json_t *params){
    if (json_array_size(params) != 11) return NULL;
    CODE_PIECES_CHECK_JSON_INTEGER(params, 0, user_id)
    CODE_PIECES_CHECK_JSON_STRING(params, 1, market_name)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 2, direction)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 3, type)
    CODE_PIECES_CHECK_JSON_INTEGER(params, 4, pattern)
    CODE_PIECES_CHECK_JSON_MDP(params, 5, markPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 6, triggerPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 7, entrustPrice)
    CODE_PIECES_CHECK_JSON_MDP(params, 8, volume)
    CODE_PIECES_CHECK_JSON_MDP(params, 9, taker_fee)
    CODE_PIECES_CHECK_JSON_MDP(params, 10, maker_fee)

        // 检查币对
    CODE_PIECES_CHECK_MARKET(market_name, market)

    args_t *args = (args_t*)malloc(sizeof(args_t));
    args->user_id = user_id;
    args->market = market;
    args->direction = direction;
    args->Type = type;
    args->pattern = pattern;
    args->markPrice = markPrice;
    args->triggerPrice = triggerPrice;
    args->entrustPrice = entrustPrice;
    args->leverage = mpd_new(&mpd_ctx);;
    args->volume = volume;
    args->taker_fee_rate = taker_fee;
    args->maker_fee_rate = maker_fee;
    args->priAmount = mpd_new(&mpd_ctx);
    args->fee = mpd_new(&mpd_ctx);
    args->priAndFee = mpd_new(&mpd_ctx);
    args->msg = "";
    return args;
}
#include "me_config.h"
#include "me_market.h"
#include "me_position.h"
#include "me_args.h"

extern dict_t *dict_position;
extern market_t *get_market(const char *name);
extern mpd_t *getSumPNL(uint32_t user_id);
extern mpd_t *getPNL(position_t *position, mpd_t *latestPrice);

static order_t *initOrder(position_t *position){
   order_t *order = malloc(sizeof(order_t));
    order->id = ++order_id_start;
    order->type = 0;
    order->side = abs(2 - position->side);
    order->pattern = position->pattern;
    order->oper_type = 0;
    order->create_time = current_timestamp();
    order->update_time = order->create_time;
    order->user_id = position->user_id;
    order->market = strdup(position->market);
    order->relate_order = 0;
    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->leverage = mpd_new(&mpd_ctx);
    order->trigger = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);
    order->source = "";
    // order->source       = strdup(source);

    // mpd_copy(order->price, args->entrustPrice, &mpd_ctx);
    mpd_copy(order->amount, position->position, &mpd_ctx);
    mpd_add(order->amount, order->amount, position->frozen, &mpd_ctx);
    mpd_copy(order->leverage, position->leverage, &mpd_ctx);
    // mpd_copy(order->trigger, args->triggerPrice, &mpd_ctx);
    mpd_copy(order->taker_fee, decimal("0.001", 0), &mpd_ctx);
    mpd_copy(order->maker_fee, decimal("0.002", 0), &mpd_ctx);
    mpd_copy(order->left, order->amount, &mpd_ctx);
    mpd_copy(order->freeze, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_stock, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_money, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_fee, mpd_zero, &mpd_ctx);
    return order;
}

int force_liquidation(){
    // 通过遍历仓位，检查逐仓
    dict_iterator *iter = dict_get_iterator(dict_position);
    if (iter == NULL)
        return -1;
    dict_entry *entry;
    struct position_key* key = NULL;
    while ((entry = dict_next(iter)) != NULL) {
        key = entry->key;
        position_t *position = entry->val;
        if(position->pattern == 1){//逐仓
            market_t *market = get_market(position->market);
            mpd_t *pnl = getPNL(position, market->latestPrice);
            if (mpd_cmp(pnl, mpd_zero, &mpd_ctx) <= 0){//爆仓
                order_t *order = initOrder(position);
                execute_order(1, market, order->side, order);
            }
        }else{//全仓
            market_t *market = get_market(position->market);
            mpd_t *pnl = getSumPNL(position->user_id);
            if (mpd_cmp(pnl, mpd_zero, &mpd_ctx) <= 0){//爆仓
                order_t *order = initOrder(position);
                execute_order(1, market, order->side, order);
            }
        }
    }

    return 0;
}
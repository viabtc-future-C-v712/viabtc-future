#include "me_config.h"
#include "me_market.h"
#include "me_message.h"
#include "me_balance.h"
#include "me_position.h"
#include "me_args.h"
#include <pthread.h>

extern dict_t *dict_market;
extern int execute_order(uint32_t real, market_t* market, uint32_t direction, order_t* taker);

order_t * copyOrder(order_t *order_old){
    order_t *order = (order_t*)malloc(sizeof(order_t));
    memcpy(order, order_old, sizeof(order_t));

    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->leverage = mpd_new(&mpd_ctx);
    order->trigger = mpd_new(&mpd_ctx);
    order->current_price = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);

    mpd_copy(order->price, order_old->price, &mpd_ctx);
    mpd_copy(order->amount, order_old->amount, &mpd_ctx);
    mpd_copy(order->leverage, order_old->leverage, &mpd_ctx);
    mpd_copy(order->trigger, order_old->trigger, &mpd_ctx);
    mpd_copy(order->current_price, order_old->current_price, &mpd_ctx);
    mpd_copy(order->taker_fee, order_old->taker_fee, &mpd_ctx);
    mpd_copy(order->maker_fee, order_old->maker_fee, &mpd_ctx);
    mpd_copy(order->left, order_old->left, &mpd_ctx);
    mpd_copy(order->freeze, order_old->freeze, &mpd_ctx);
    mpd_copy(order->deal_stock, order_old->deal_stock, &mpd_ctx);
    mpd_copy(order->deal_money, order_old->deal_money, &mpd_ctx);
    mpd_copy(order->deal_fee, order_old->deal_fee, &mpd_ctx);
    return order;
}

void on_planner(uint32_t real)
{
    dict_iterator *iter_market = dict_get_iterator(dict_market);
    dict_entry *entry_market;
    while ((entry_market = dict_next(iter_market)) != NULL) {
        skiplist_node *node;
        market_t *market = entry_market->val;
        // 处理卖单
        skiplist_iter *iter = skiplist_get_iterator(market->plan_asks);
        while ((node = skiplist_next(iter)) != NULL){
            order_t *order_old = node->value;
            //判断是否要下单
            log_trace("order record %p", (void *)order_old);
            if( (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) >= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) <= 0) || // 市场价小于等于卖价
                (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) <= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) >= 0)){// 市场价大于等于卖价
                skiplist_delete(market->plan_asks, node);// 
                push_order_message(ORDER_EVENT_FINISH, order_old, market);
                order_t *order = copyOrder(order_old);
                if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0)
                    order->type = 0;//变为市价单
                else
                    order->type = 1;//变为限价单
                order->id = ++order_id_start;
                if(order->oper_type == 1){//开仓，卖 （开空）
                    // 在此处冻结资金 仅限价单需要
                    if (order->type == 1){
                        // 计算余额 是否大于保证金
                        mpd_t *priAndFee = mpd_new(&mpd_ctx);
                        mpd_div(priAndFee, order->left, order->leverage, &mpd_ctx);
                        if(checkPriAndFee(order->pattern, order->user_id, balance_get(order->user_id, BALANCE_TYPE_AVAILABLE, market->money), priAndFee)) return -1;
                        if (balance_freeze(order->user_id, market->money, priAndFee) == NULL) break;
                        mpd_copy(order->freeze, priAndFee, &mpd_ctx);
                    }
                    execute_order(real, market, BEAR, order);
                }
                else{//平仓，卖 （平多）
                    position_t *position = get_position(order->user_id, order->market, order->side);
                    mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
                    mpd_sub(position->position, position->position, order->left, &mpd_ctx);
                    execute_order(real, market, BULL, order);
                }
            }
        }
        // 处理买单
        iter = skiplist_get_iterator(market->plan_bids);
        while ((node = skiplist_next(iter)) != NULL){
            order_t *order_old = node->value;
            //判断是否要下单
            if( (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) >= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) <= 0) || // 市场价小于等于卖价
                (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) <= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) >= 0)){// 市场价大于等于卖价
                skiplist_delete(market->plan_bids, node);
                push_order_message(ORDER_EVENT_FINISH, order_old, market);
                order_t *order = copyOrder(order_old);
                if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0)
                    order->type = 0;//变为市价单
                else
                    order->type = 1;//变为限价单
                order->id = ++order_id_start;
                if(order->oper_type == 0){//开仓，买 （开多）
                    // 在此处冻结资金 仅限价单需要
                    if (order->type == 1){
                        // 计算余额 是否大于保证金
                        mpd_t *priAndFee = mpd_new(&mpd_ctx);
                        mpd_div(priAndFee, order->left, order->leverage, &mpd_ctx);
                        if(checkPriAndFee(order->pattern, order->user_id, balance_get(order->user_id, BALANCE_TYPE_AVAILABLE, market->money), priAndFee)) return -1;
                        if (balance_freeze(order->user_id, market->money, priAndFee) == NULL) break;
                        mpd_copy(order->freeze, priAndFee, &mpd_ctx);
                    }
                    execute_order(real, market, BULL, order);
                }
                else{//平仓，卖 （平空）
                    position_t *position = get_position(order->user_id, order->market, order->side);
                    mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
                    mpd_sub(position->position, position->position, order->left, &mpd_ctx);
                    execute_order(real, market, BEAR, order);
                }
            }
        }
    }
}
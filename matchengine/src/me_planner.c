#include "me_config.h"
#include "me_market.h"
#include "me_args.h"
#include <pthread.h>

extern dict_t *dict_market;
extern int execute_order(uint32_t real, market_t* market, uint32_t direction, order_t* taker);

void on_planner()
{
    dict_iterator *iter_market = dict_get_iterator(dict_market);
    dict_entry *entry_market;
    while ((entry_market = dict_next(iter_market)) != NULL) {
        skiplist_node *node;
        market_t *market = entry_market->val;
        // 处理卖单
        skiplist_iter *iter = skiplist_get_iterator(market->plan_asks);
        while ((node = skiplist_next(iter)) != NULL){
            order_t *order = node->value;
            if (mpd_cmp(order->left, mpd_zero, &mpd_ctx) == 0)
                break;
            //判断是否要下单
            if(mpd_cmp(market->latestPrice, order->trigger, &mpd_ctx) >= 0){// 市场价大于等于卖价
                if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0)
                    order->type = 0;//变为限价单
                else
                    order->type = 1;//变为市价单
                if(order->oper_type == 1)//开仓，卖 （开空）
                    execute_order(true, market, BEAR, order);
                else//平仓，卖 （平多）
                    execute_order(true, market, BULL, order);
            }
        }
        // 处理买单
        iter = skiplist_get_iterator(market->plan_bids);
        while ((node = skiplist_next(iter)) != NULL){
            order_t *order = node->value;
            if (mpd_cmp(order->left, mpd_zero, &mpd_ctx) == 0)
                break;
            //判断是否要下单
            if(mpd_cmp(market->latestPrice, order->trigger, &mpd_ctx) <= 0){// 市场价小于等于买价
                if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0)
                    order->type = 0;//变为限价单
                else
                    order->type = 1;//变为市价单
                if(order->oper_type == 0)//开仓，买 （开多）
                    execute_order(1, market, BULL, order);
                else//平仓，卖 （平空）
                    execute_order(1, market, BEAR, order);
            }
        }
    }
}
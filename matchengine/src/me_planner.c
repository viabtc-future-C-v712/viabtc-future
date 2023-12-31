#include "me_config.h"
#include "me_market.h"
#include "me_message.h"
#include "me_balance.h"
#include "me_position.h"
#include "me_args.h"
#include <pthread.h>

extern dict_t *dict_market;
extern int execute_order(uint32_t real, market_t *market, uint32_t direction, order_t *taker);

order_t *copyOrder(order_t *order_old)
{
    order_t *order = (order_t *)malloc(sizeof(order_t));
    memcpy(order, order_old, sizeof(order_t));

    order->price = mpd_new(&mpd_ctx);
    order->amount = mpd_new(&mpd_ctx);
    order->leverage = mpd_new(&mpd_ctx);
    order->trigger = mpd_new(&mpd_ctx);
    order->current_price = mpd_new(&mpd_ctx);
    order->taker_fee = mpd_new(&mpd_ctx);
    order->maker_fee = mpd_new(&mpd_ctx);
    order->tpPrice = mpd_new(&mpd_ctx);
    order->tpAmount = mpd_new(&mpd_ctx);
    order->slPrice = mpd_new(&mpd_ctx);
    order->slAmount = mpd_new(&mpd_ctx);
    order->left = mpd_new(&mpd_ctx);
    order->freeze = mpd_new(&mpd_ctx);
    order->deal_stock = mpd_new(&mpd_ctx);
    order->deal_money = mpd_new(&mpd_ctx);
    order->deal_fee = mpd_new(&mpd_ctx);
    order->pnl = mpd_new(&mpd_ctx);
    order->market = strdup(order_old->market);
    mpd_copy(order->price, order_old->price, &mpd_ctx);
    mpd_copy(order->amount, order_old->amount, &mpd_ctx);
    mpd_copy(order->leverage, order_old->leverage, &mpd_ctx);
    mpd_copy(order->trigger, order_old->trigger, &mpd_ctx);
    mpd_copy(order->current_price, order_old->current_price, &mpd_ctx);
    mpd_copy(order->taker_fee, order_old->taker_fee, &mpd_ctx);
    mpd_copy(order->maker_fee, order_old->maker_fee, &mpd_ctx);
    mpd_copy(order->tpPrice, order_old->tpPrice, &mpd_ctx);
    mpd_copy(order->tpAmount, order_old->tpAmount, &mpd_ctx);
    mpd_copy(order->slPrice, order_old->slPrice, &mpd_ctx);
    mpd_copy(order->slAmount, order_old->slAmount, &mpd_ctx);
    mpd_copy(order->left, order_old->left, &mpd_ctx);
    mpd_copy(order->freeze, order_old->freeze, &mpd_ctx);
    mpd_copy(order->deal_stock, order_old->deal_stock, &mpd_ctx);
    mpd_copy(order->deal_money, order_old->deal_money, &mpd_ctx);
    mpd_copy(order->deal_fee, order_old->deal_fee, &mpd_ctx);
    mpd_copy(order->pnl, mpd_zero, &mpd_ctx);
    return order;
}

int checkConditionAsk(order_t *order_old, market_t *market){
    if(order_old->tpPrice && mpd_cmp(market->latestPrice, order_old->tpPrice, &mpd_ctx) >= 0)
        return 1;
    if(order_old->slPrice && mpd_cmp(market->latestPrice, order_old->slPrice, &mpd_ctx) <= 0)
        return 2;
    return 0;
}

void on_tpslPlannerAsk(order_t *order_old, market_t *market, uint32_t real){
    // 判断是否要下单
    //  log_trace("order record %p", (void *)order_old);
    int bret = checkConditionAsk(order_old, market);
    if (bret)
    { // 市场价大于等于卖价

        order_t *order = copyOrder(order_old);
        if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0){
            order->type = 0;//变为市价单
        }
        else
            order->type = 1; // 变为限价单
        order->id = ++order_id_start;
        if(bret == 1){
            mpd_copy(order->amount, order->tpAmount, &mpd_ctx);
            mpd_copy(order->left, order->amount, &mpd_ctx);
        }
        else{
            mpd_copy(order->amount, order->slAmount, &mpd_ctx);
            mpd_copy(order->left, order->amount, &mpd_ctx);
        }
        // 平仓，卖 （平多）
        position_t *position = get_position(order->user_id, order->market, order->side);
        if (position)
        {
            mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
            mpd_sub(position->position, position->position, order->left, &mpd_ctx);
            if (!execute_order(real, market, BULL, order))
            {

                if (real)
                    push_order_message(ORDER_EVENT_FINISH, order_old, market);
                log_trace("on_planner success");
                order_finish_future(real, market, order_old);
            }
        }
        else
        {
            log_trace("on_planner error order id %d", order->id);
        }
    }
}

int checkConditionBid(order_t *order_old, market_t *market){
    if(order_old->tpPrice && mpd_cmp(market->latestPrice, order_old->tpPrice, &mpd_ctx) <= 0)
        return 1;
    if(order_old->slPrice && mpd_cmp(market->latestPrice, order_old->slPrice, &mpd_ctx) >= 0)
        return 2;
    return 0;
}

void on_tpslPlannerBid(order_t *order_old, market_t *market, uint32_t real){
    // 判断是否要下单
    int bret = checkConditionBid(order_old, market);
    if (bret)
    { // 市场价大于等于卖价

        order_t *order = copyOrder(order_old);
        if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0){
            order->type = 0;//变为市价单
        }
        else
            order->type = 1; // 变为限价单
        order->id = ++order_id_start;
        if(bret == 1){
            mpd_copy(order->amount, order->tpAmount, &mpd_ctx);
            mpd_copy(order->left, order->amount, &mpd_ctx);
        }
        else{
            mpd_copy(order->amount, order->slAmount, &mpd_ctx);
            mpd_copy(order->left, order->amount, &mpd_ctx);
        }
        // 平仓，卖 （平空）
        position_t *position = get_position(order->user_id, order->market, order->side);
        if (position)
        {
            mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
            mpd_sub(position->position, position->position, order->left, &mpd_ctx);
            if (!execute_order(real, market, BEAR, order))
            {
                if (real)
                    push_order_message(ORDER_EVENT_FINISH, order_old, market);
                log_trace("on_planner success");
                order_finish_future(real, market, order_old);
            }
        }
        else
        {
            log_trace("on_planner error order id %d", order->id);
        }
    }
}

void on_PannerAsk(order_t *order_old, market_t *market, uint32_t real){
    // 判断是否要下单
    //  log_trace("order record %p", (void *)order_old);
    if ((mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) >= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) <= 0) || // 市场价小于等于卖价
        (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) <= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) >= 0))
    { // 市场价大于等于卖价

        order_t *order = copyOrder(order_old);
        if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0){
            order->type = 0;//变为市价单
        }
        else
            order->type = 1; // 变为限价单
        order->id = ++order_id_start;
        if (order->oper_type == 1)
        { // 开仓，卖 （开空）
            // 在此处冻结资金 仅限价单需要

            // 计算余额 是否大于保证金
            mpd_t *priAndFee = mpd_new(&mpd_ctx);
            mpd_div(priAndFee, order->left, order->leverage, &mpd_ctx);
            mpd_t *balance = balance_get(order->user_id, BALANCE_TYPE_AVAILABLE, market->money);
            if (priAndFee)
                log_trace("on_planner %s", mpd_to_sci(priAndFee, 0));
            if (balance)
                log_trace("on_planner %s", mpd_to_sci(balance, 0));
            if (checkPriAndFee(order->pattern, order->user_id, balance, priAndFee))
            {
                log_trace("on_planner error checkPriAndFee order id %d", order->id);
                return -1;
            }
            if (order->type == 1)
            {
                if (balance_freeze(order->user_id, market->money, priAndFee) == NULL)
                    return 1;
                mpd_copy(order->freeze, priAndFee, &mpd_ctx);
            }
            if (!execute_order(real, market, BEAR, order))
            {
                if (real)
                    push_order_message(ORDER_EVENT_FINISH, order_old, market);
                log_trace("on_planner success");
                order_finish_future(real, market, order_old);
            }
        }
        else
        { // 平仓，卖 （平多）
            position_t *position = get_position(order->user_id, order->market, order->side);
            if (position)
            {
                mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
                mpd_sub(position->position, position->position, order->left, &mpd_ctx);
                if (!execute_order(real, market, BULL, order))
                {

                    if (real)
                        push_order_message(ORDER_EVENT_FINISH, order_old, market);
                    log_trace("on_planner success");
                    order_finish_future(real, market, order_old);
                }
            }
            else
            {
                log_trace("on_planner error order id %d", order->id);
            }
        }
    }
    return 0;
}

int on_PannerBid(order_t *order_old, market_t *market, uint32_t real){
    // 判断是否要下单
    if ((mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) >= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) <= 0) || // 市场价小于等于卖价
        (mpd_cmp(order_old->current_price, order_old->trigger, &mpd_ctx) <= 0 && mpd_cmp(market->latestPrice, order_old->trigger, &mpd_ctx) >= 0))
    { // 市场价大于等于卖价

        order_t *order = copyOrder(order_old);
        if(mpd_cmp(order->price, mpd_zero, &mpd_ctx) == 0){
            order->type = 0;//变为市价单
        }
        else
            order->type = 1; // 变为限价单
        order->id = ++order_id_start;
        if (order->oper_type == 1)
        { // 开仓，买 （开多）
            // 在此处冻结资金 仅限价单需要

            // 计算余额 是否大于保证金
            mpd_t *priAndFee = mpd_new(&mpd_ctx);
            mpd_div(priAndFee, order->left, order->leverage, &mpd_ctx);
            if (checkPriAndFee(order->pattern, order->user_id, balance_get(order->user_id, BALANCE_TYPE_AVAILABLE, market->money), priAndFee))
            {
                log_trace("on_planner error checkPriAndFee order id %d", order->id);
                return -1;
            }
            if (order->type == 1)
            {
                if (balance_freeze(order->user_id, market->money, priAndFee) == NULL)
                    return 1;
                mpd_copy(order->freeze, priAndFee, &mpd_ctx);
            }
            if (!execute_order(real, market, BULL, order))
            {
                if (real)
                    push_order_message(ORDER_EVENT_FINISH, order_old, market);
                log_trace("on_planner success");
                order_finish_future(real, market, order_old);
            }
        }
        else
        { // 平仓，卖 （平空）
            position_t *position = get_position(order->user_id, order->market, order->side);
            if (position)
            {
                mpd_add(position->frozen, position->frozen, order->left, &mpd_ctx);
                mpd_sub(position->position, position->position, order->left, &mpd_ctx);
                if (!execute_order(real, market, BEAR, order))
                {
                    if (real)
                        push_order_message(ORDER_EVENT_FINISH, order_old, market);
                    log_trace("on_planner success");
                    order_finish_future(real, market, order_old);
                }
            }
            else
            {
                log_trace("on_planner error order id %d", order->id);
            }
        }
    }
    return 0;
}

void on_planner(uint32_t real)
{
    dict_iterator *iter_market = dict_get_iterator(dict_market);
    dict_entry *entry_market;
    while ((entry_market = dict_next(iter_market)) != NULL)
    {
        skiplist_node *node;
        market_t *market = entry_market->val;
        // 处理卖单
        skiplist_iter *iter = skiplist_get_iterator(market->plan_asks);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order_t *order_old = node->value;
            if(order_old->type == 2){
                on_PannerAsk(order_old, market, real);
            }else{
                on_tpslPlannerAsk(order_old, market, real);
            }
        }
        // 处理买单
        iter = skiplist_get_iterator(market->plan_bids);
        while ((node = skiplist_next(iter)) != NULL)
        {
            order_t *order_old = node->value;
            if(order_old->type == 2){
                on_PannerBid(order_old, market, real);
            }else{
                on_tpslPlannerBid(order_old, market, real);
            }
        }
    }
}
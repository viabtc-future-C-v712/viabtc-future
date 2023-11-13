#include "me_config.h"
#include "me_market.h"
#include "me_position.h"
#include "me_args.h"

extern dict_t *dict_position;
extern market_t *get_market(const char *name);
extern mpd_t *getSumPNL(uint32_t user_id);
extern mpd_t *getPNL(position_t *position, mpd_t *latestPrice);
extern mpd_t *getSumCloseFee(uint32_t user_id);
extern mpd_t *getSumMaintenance(uint32_t user_id);

static order_t *initOrder(position_t *position)
{
    order_t *order = malloc(sizeof(order_t));
    order->id = ++order_id_start;
    order->type = 0;
    order->isblast = 1;
    order->side = position->side;
    order->pattern = position->pattern;
    order->oper_type = 2;
    order->create_time = current_timestamp();
    order->update_time = order->create_time;
    order->user_id = position->user_id;
    order->market = strdup(position->market);
    order->relate_order = 0;
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
    order->pnl = mpd_new(&mpd_ctx);
    order->source = "";
    order->mm = 0;
    // order->source       = strdup(source);

    // mpd_copy(order->price, args->entrustPrice, &mpd_ctx);
    mpd_copy(order->price, mpd_zero, &mpd_ctx);
    mpd_copy(order->amount, position->position, &mpd_ctx);
    mpd_add(order->amount, order->amount, position->frozen, &mpd_ctx);
    mpd_copy(order->leverage, position->leverage, &mpd_ctx);
    mpd_copy(order->trigger, mpd_zero, &mpd_ctx);
    mpd_copy(order->current_price, mpd_zero, &mpd_ctx);
    // mpd_copy(order->trigger, args->triggerPrice, &mpd_ctx);
    mpd_copy(order->taker_fee, decimal("0.0001", 0), &mpd_ctx);
    mpd_copy(order->maker_fee, decimal("0.0001", 0), &mpd_ctx);
    mpd_copy(order->left, order->amount, &mpd_ctx);
    mpd_copy(order->freeze, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_stock, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_money, mpd_zero, &mpd_ctx);
    mpd_copy(order->deal_fee, mpd_zero, &mpd_ctx);
    mpd_copy(order->pnl, mpd_zero, &mpd_ctx);
    return order;
}

int force_liquidation(uint32_t real)
{
    // 通过遍历仓位，检查逐仓
    dict_iterator *iter = dict_get_iterator(dict_position);
    if (iter == NULL)
        return -1;
    dict_entry *entry;
    struct position_key *key = NULL;
    while ((entry = dict_next(iter)) != NULL)
    {
        key = entry->key;
        position_t *position = entry->val;
        if (position->pattern == 1)
        { // 逐仓
            market_t *market = get_market(position->market);
            if (mpd_cmp(market->latestPrice, mpd_zero, &mpd_ctx) <= 0)
                continue;
            mpd_t *pnl = getPNL(position, market->latestPrice);
            log_trace("force_liquidation pnl %s", mpd_to_sci(pnl, 0));

            // maintenance = 持仓总量 * maintenance_fee
            mpd_t *maintenance = mpd_new(&mpd_ctx);
            mpd_add(maintenance, position->position, position->frozen, &mpd_ctx);
            mpd_mul(maintenance, maintenance, decimal("0.0003", 0), &mpd_ctx);
            log_trace("force_liquidation maintenance %s", mpd_to_sci(maintenance, 0));
            
            // closeFee = 持仓总量 * taker_fee
            mpd_t *closeFee = mpd_new(&mpd_ctx);
            mpd_add(closeFee, position->position, position->frozen, &mpd_ctx);
            mpd_mul(closeFee, closeFee, decimal("0.0001", 0), &mpd_ctx);
            log_trace("force_liquidation closeFee %s", mpd_to_sci(closeFee, 0));

            // pnl - closeFee - naintenance < 0  ==> 爆仓
            mpd_t *condition = mpd_new(&mpd_ctx);
            mpd_sub(condition, pnl, closeFee, &mpd_ctx);
            mpd_sub(condition, condition, maintenance, &mpd_ctx);
            log_trace("force_liquidation condition %s", mpd_to_sci(condition, 0));

            if (mpd_cmp(condition, mpd_zero, &mpd_ctx) <= 0)
            { // 爆仓
                // 检查order
                int flag = 1;
                while(flag){
                    flag = 0;
                    skiplist_t *order_list = market_get_order_list(market, position->user_id);
                    if (order_list)
                    {
                        skiplist_iter *iter = skiplist_get_iterator(order_list);
                        skiplist_node *node;
                        while (iter && (node = skiplist_next(iter)) != NULL)
                        { // 如果有order存在，需要撤销
                            order_t *tmp = node->value;
                            if(tmp->side == position->side && tmp->oper_type == 2){
                                int ret = market_cancel_order(true, NULL, market, tmp);
                                flag = 1;
                                break;
                            }
                        }
                    }
                }
                order_t *order = initOrder(position);
                order->isblast = 1;
                if (mpd_cmp(position->frozen, mpd_zero, &mpd_ctx) > 0){
                    log_trace("force_liquidation frozen not empty error %d", position->id);
                }
                log_trace("force_liquidation %d", order->id);
                mpd_add(position->frozen, position->frozen, position->position, &mpd_ctx);
                mpd_copy(position->position, mpd_zero, &mpd_ctx);
                execute_order(real, market, order->side, order);
            }
            mpd_del(maintenance);
            mpd_del(closeFee);
            mpd_del(condition);
            mpd_del(pnl);
        }
        else
        { // 全仓
            market_t *market = get_market(position->market);
            if (mpd_cmp(market->latestPrice, mpd_zero, &mpd_ctx) <= 0)
                continue;
            mpd_t *pnl = getSumPNL(position->user_id);
            mpd_t *closeFee = getSumCloseFee(position->user_id);
            mpd_t *maintenance = getSumMaintenance(position->user_id);

            mpd_t *condition = mpd_new(&mpd_ctx);
            mpd_sub(condition, pnl, closeFee, &mpd_ctx);
            mpd_sub(condition, condition, maintenance, &mpd_ctx);

            if (mpd_cmp(condition, mpd_zero, &mpd_ctx) <= 0)
            { // 爆仓
                order_t *order = initOrder(position);
                order->isblast = 1;
                mpd_add(position->frozen, position->frozen, position->position, &mpd_ctx);
                mpd_copy(position->position, mpd_zero, &mpd_ctx);
                log_trace("force_liquidation %d", order->id);
                execute_order(real, market, order->side, order);
            }
            mpd_del(pnl);
            mpd_del(closeFee);
            mpd_del(maintenance);
            mpd_del(condition);
        }
    }

    return 0;
}

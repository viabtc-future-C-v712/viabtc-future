#include "me_config.h"
#include "me_market.h"
#include "me_position.h"
#include "me_args.h"

extern dict_t *dict_position;
extern market_t *get_market(const char *name);
extern mpd_t *getSumPNL(uint32_t user_id);
extern mpd_t *getPNL(position_t *position, mpd_t *latestPrice);

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
            /*
                这个地方还需要减去 维持保证金率  和 市价 平仓手续费

                maintenance = (position -> principal) * leve * maintenance_fee

                closeFee = (position + frozen) * taker_fee

                pnl - closeFee - naintenance < 0  ==> 爆仓

            */

            if (mpd_cmp(pnl, mpd_zero, &mpd_ctx) <= 0)
            { // 爆仓
                order_t *order = initOrder(position);
                mpd_add(position->frozen, position->frozen, position->position, &mpd_ctx);
                mpd_sub(position->position, position->position, position->position, &mpd_ctx);
                log_trace("force_liquidation %d", order->id);
                execute_order(real, market, order->side, order);
            }
        }
        else
        { // 全仓
            market_t *market = get_market(position->market);
            if (mpd_cmp(market->latestPrice, mpd_zero, &mpd_ctx) <= 0)
                continue;
            mpd_t *pnl = getSumPNL(position->user_id);
            if (mpd_cmp(pnl, mpd_zero, &mpd_ctx) <= 0)
            { // 爆仓
                order_t *order = initOrder(position);
                mpd_add(position->frozen, position->frozen, position->position, &mpd_ctx);
                mpd_sub(position->position, position->position, position->position, &mpd_ctx);
                log_trace("force_liquidation %d", order->id);
                execute_order(real, market, order->side, order);
            }
        }
    }

    return 0;
}

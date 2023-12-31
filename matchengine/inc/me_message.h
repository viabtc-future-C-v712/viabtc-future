/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/08, create
 */

# ifndef _ME_MESSAGE_H_
# define _ME_MESSAGE_H_

# include "me_config.h"
# include "me_market.h"
# include "me_position.h"

int init_message(void);
int fini_message(void);

enum {
    ORDER_EVENT_PUT     = 1,
    ORDER_EVENT_UPDATE  = 2,
    ORDER_EVENT_FINISH  = 3,
};

int push_balance_message(double t, uint32_t user_id, const char *asset, const char *business, mpd_t *change);
int push_order_message(uint32_t event, order_t *order, market_t *market);
int push_deal_message(double t, const char *market, order_t *ask, order_t *bid, mpd_t *price, mpd_t *amount,
        mpd_t *ask_fee, mpd_t *bid_fee, int side, uint64_t id, const char *stock, const char *money);
int push_deal_message_extra(double t, const char *market, uint64_t aid, uint64_t bid, uint32_t a_u_id, uint32_t b_u_id, mpd_t *price, mpd_t *amount,
        mpd_t *ask_fee, mpd_t *bid_fee, int side, uint64_t id, const char *stock, const char *money);
int push_position_message_(position_t *position);
bool is_message_block(void);
sds message_status(sds reply);

# endif


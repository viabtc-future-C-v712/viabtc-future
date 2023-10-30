/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/27, create
 */

# ifndef _AW_POSITION_H_
# define _AW_POSITION_H_

int init_position(void);

int position_subscribe(uint32_t user_id, nw_ses *ses, const char *market, uint32_t side);
int position_unsubscribe(uint32_t user_id, nw_ses *ses);

int position_on_update(uint32_t user_id, const char *market, uint32_t side);
# endif

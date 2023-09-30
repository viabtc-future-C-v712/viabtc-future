/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/29, create
 */

# ifndef _ME_TRADE_H_
# define _ME_TRADE_H_

# include "me_market.h"

int init_trade(void);
market_t *get_market(const char *name);
int add_market(struct market *m);
int del_market(market_t *m);
int del_market_by_asset(const char* name);
int check_market_prec_by_asset(const char* name, int prec_show);

# endif


/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/04, create
 */

# ifndef _ME_LOAD_H_
# define _ME_LOAD_H_

# include <stdint.h>
# include "ut_mysql.h"

int load_orders(MYSQL *conn, const char *table);
int load_balance(MYSQL *conn, const char *table);
int load_position(MYSQL *conn, const char *table);
int load_market_db(MYSQL *conn, const char *table);
int load_operlog(MYSQL *conn, const char *table, uint64_t *start_id);

# endif


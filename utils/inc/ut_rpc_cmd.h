/*
 * Description: 
 *     History: yang@haipo.me, 2016/09/30, create
 */

# ifndef _UT_RPC_CMD_H_
# define _UT_RPC_CMD_H_

// balance
# define CMD_BALANCE_QUERY          101
# define CMD_BALANCE_UPDATE         102
# define CMD_BALANCE_HISTORY        103
# define CMD_ASSET_LIST             104
# define CMD_ASSET_SUMMARY          105
# define CMD_BALANCE_UNFREEZE       106
# define CMD_MATCHENGINE_MAKE_SLICE 107

# define CMD_BALANCE_QUERY_ALL      108

// trade
# define CMD_ORDER_PUT_LIMIT        201
# define CMD_ORDER_PUT_MARKET       202
# define CMD_ORDER_QUERY            203
# define CMD_ORDER_CANCEL           204
# define CMD_ORDER_BOOK             205
# define CMD_ORDER_BOOK_DEPTH       206
# define CMD_ORDER_DETAIL           207
# define CMD_ORDER_HISTORY          208
# define CMD_ORDER_DEALS            209
# define CMD_ORDER_DETAIL_FINISHED  210
# define CMD_ORDER_CANCEL_BATCH     211
# define CMD_ORDER_QUERY_BRIEF      212
# define CMD_ORDER_QUERY_ALL        213
# define CMD_ORDER_PUT_LIMIT_BATCH  214
# define CMD_ORDER_UPDATE_LIMIT     215
# define CMD_ORDER_UPDATE_LIMIT_BATCH     216
# define CMD_ORDER_PUSH_DEALS       217
# define CMD_ORDER_BOOK_EXT         218
# define CMD_ORDER_HISTORY_ALL      219

// 合约
# define CMD_ORDER_OPEN             250
# define CMD_ORDER_CLOSE            251
# define CMD_ORDER_QUERY_ALLUSER    252

# define CMD_POSITION_QUERY         260
# define CMD_POSITION_QUERY_ALL     261
# define CMD_POSITION_HISTORY       262
# define CMD_POSITION_ADJUST_PRINCIPAL         263

# define CMD_POSITION_MODE_QUERY    264
# define CMD_POSITION_MODE_ADJUST   265

# define CMD_MATCHENGINE_SUICIDE    270

// market
# define CMD_MARKET_STATUS          301
# define CMD_MARKET_KLINE           302
# define CMD_MARKET_DEALS           303
# define CMD_MARKET_LAST            304
# define CMD_MARKET_STATUS_TODAY    305
# define CMD_MARKET_USER_DEALS      306
# define CMD_MARKET_LIST            307
# define CMD_MARKET_SUMMARY         308
# define CMD_MARKET_ADD_ASSET       309
# define CMD_MARKET_DEL_ASSET       310
# define CMD_MARKET_ADD_MARKET      311
# define CMD_MARKET_DEL_MARKET      312
# define CMD_MARKET_USER_ALL_DEALS  313
# define CMD_MARKET_KLINE_RELOAD    314
# define CMD_MARKET_KLINE_UPDATE    315

# endif


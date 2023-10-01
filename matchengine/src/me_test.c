# include "me_trade.h"
void test(){
        // market_t *X = get_market("BTCBCH");
        char * p = (void*)get_market("BTCBCH");
    log_trace("%s %p", __FUNCTION__, (void*)p);
}
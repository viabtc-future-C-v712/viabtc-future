# include "me_deal.h"


deal_t* initDeal(){
    deal_t *deal = malloc(sizeof(deal_t));
    deal->amount = mpd_new(&mpd_ctx);
    deal->price = mpd_new(&mpd_ctx);
    deal->taker_fee = mpd_new(&mpd_ctx);
    deal->maker_fee = mpd_new(&mpd_ctx);
    deal->deal = mpd_new(&mpd_ctx);
    return deal;
}
/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/29, create
 */

# include "me_config.h"
# include "me_trade.h"

dict_t *dict_market;

static uint32_t market_dict_hash_function(const void *key)
{
    return dict_generic_hash_function(key, strlen(key));
}

static int market_dict_key_compare(const void *key1, const void *key2)
{
    return strcmp(key1, key2);
}

static void *market_dict_key_dup(const void *key)
{
    return strdup(key);
}

static void market_dict_key_free(void *key)
{
    free(key);
}

int init_trade(void)
{
    dict_types type;
    memset(&type, 0, sizeof(type));
    type.hash_function = market_dict_hash_function;
    type.key_compare = market_dict_key_compare;
    type.key_dup = market_dict_key_dup;
    type.key_destructor = market_dict_key_free;

    dict_market = dict_create(&type, 64);
    if (dict_market == NULL)
        return -__LINE__;

    for (size_t i = 0; i < settings.market_num; ++i) {
        market_t *m = market_create(&settings.markets[i]);
        log_trace("%s %p %s", __FUNCTION__, (void*)m, m->name);
        if (m == NULL) {
            return -__LINE__;
        }

        dict_add(dict_market, settings.markets[i].name, m);
    }

    return 0;
}

market_t *get_market(const char *name)
{
    dict_entry *entry = dict_find(dict_market, name);
    if (entry){
        return (market_t *)entry->val;
    }
    return NULL;
}

int add_market(struct market *m_conf)
{
    market_t *m = market_create(m_conf);
    if (m == NULL) {
        return -__LINE__;
    }

    dict_add(dict_market, m->name, m);

    return 0;
}

int del_market(market_t *m)
{
    dict_delete(dict_market, m->name);

    dict_clear(m->users);
    dict_release(m->users);

    dict_clear(m->orders);
    dict_release(m->orders);

    skiplist_release(m->asks);
    skiplist_release(m->bids);

    if (m->name)
        free(m->name);
    if (m->stock)
        free(m->stock);
    if (m->money)
        free(m->money);
    if (m->min_amount)
        mpd_del(m->min_amount);
    free(m);

    return 0;
}

int check_market_prec_by_asset(const char* name, int prec_show)
{
    for (size_t i = 0; i < settings.market_num; ++i) {
        if ((strcmp(name, settings.markets[i].stock) == 0)) {
            if (settings.markets[i].stock_prec > prec_show)
                return -1;
        }
        if ((strcmp(name, settings.markets[i].money) == 0)) {
            if (settings.markets[i].money_prec > prec_show)
                return -1;
        }
    }

    return 0;
}

int del_market_by_asset(const char* name)
{
    for (size_t i = 0; i < settings.market_num; ++i) {
        if ((strcmp(name, settings.markets[i].stock) == 0) || (strcmp(name, settings.markets[i].money) == 0)) {
            market_t *m = get_market(settings.markets[i].name);
            if (m) {
                market_cancel_all_order(m);
                del_market(m);
            }
        }
    }

    return 0;
}

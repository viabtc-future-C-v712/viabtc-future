/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/16, create
 */

# include "me_config.h"

struct settings settings;
const char* conf_path = NULL;

json_t* reload_config()
{
    json_error_t error;
    json_t *root = json_load_file(conf_path, 0, &error);
    if (root == NULL) {
        printf("reload_config: json_load_file from: %s fail: %s in line: %d\n", conf_path, error.text, error.line);
        return NULL;
    }
    if (!json_is_object(root)) {
        json_decref(root);
        return NULL;
    }

    return root;
}

int save_config(json_t* root, bool backup)
{
    if (!conf_path || !root)
        return -1;

    if (backup) {
        char buf[2048] = {0};
        snprintf(buf, 2048, "%s_", conf_path);
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        strftime(buf+strlen(conf_path)+1, 128, "%y-%m-%d_%H:%M:%S", t);
        rename(conf_path, buf);
    }

    json_dump_file(root, conf_path, JSON_INDENT(4));

    return 0;
}

int add_asset_into_config(const char* name, int prec_save, int prec_show, int add)
{
    bool save = false;
    json_t* root = reload_config();
    if (!root) {
        return -1;
    }

    json_t *node = json_object_get(root, "assets");
    if (!node || !json_is_array(node)) {
        json_decref(root);
        return -2;
    }

    json_t *asset = NULL;
    json_t *name_obj = NULL;
    if (add) {
        // add settings
        if (settings.asset_num >= settings.asset_array_num) {
            settings.asset_array_num += MAX_ASSET_ARRAY_NUM;
            settings.assets = realloc(settings.assets, sizeof(struct asset) * (settings.asset_array_num));
            if (!settings.assets) {
                log_error("realloc assets fail");
                json_decref(root);
                return -3;
            }
        }

        settings.assets[settings.asset_num].name = strdup(name);
        settings.assets[settings.asset_num].prec_save = prec_save;
        settings.assets[settings.asset_num].prec_show = prec_show;
        settings.asset_num++;

        // add json data
        asset = json_object();
        json_object_set_new(asset, "name", json_string(name));
        json_object_set_new(asset, "prec_save", json_integer(prec_save));
        json_object_set_new(asset, "prec_show", json_integer(prec_show));
        json_array_append_new(node, asset);
        save = true;
    } else {
        for (size_t i = 0; i < settings.asset_num; ++i) {
            asset = json_array_get(node, i);
            if (!json_is_object(asset))
                continue;

            name_obj = json_object_get(asset, "name");
            if (!json_is_string(name_obj))
                continue;

            if (strcmp(name, json_string_value(name_obj)) == 0) {
                json_integer_set(json_object_get(asset, "prec_save"), prec_save);
                json_integer_set(json_object_get(asset, "prec_show"), prec_show);
                settings.assets[i].prec_save = prec_save;
                settings.assets[i].prec_show = prec_show;
                save = true;
                break;
            }
        }
    }

    if (save)
        save_config(root, true);

    json_decref(root);
    return 0;
}

int del_asset_from_config(const char* name)
{
    bool save = false;
    bool save_market = false;
    json_t* root = reload_config();
    if (!root) {
        return -1;
    }

    json_t *assets = json_object_get(root, "assets");
    if (!assets || !json_is_array(assets)) {
        json_decref(root);
        return -2;
    }

    json_t *markets = json_object_get(root, "markets");
    if (!markets || !json_is_array(markets)) {
        json_decref(root);
        return -2;
    }

    json_t *market = NULL;
    json_t *stock_obj = NULL;
    json_t *money_obj = NULL;
    for (size_t i = 0; i < settings.market_num; ) {
        market = json_array_get(markets, i);
        if (!json_is_object(market)) {
            ++i;
            continue;
        }

        stock_obj = json_object_get(json_object_get(market, "stock"), "name");
        if (!json_is_string(stock_obj)) {
            ++i;
            continue;
        }
        money_obj = json_object_get(json_object_get(market, "money"), "name");
        if (!json_is_string(money_obj)) {
            ++i;
            continue;
        }

        if ((strcmp(name, json_string_value(stock_obj)) == 0) || (strcmp(name, json_string_value(money_obj)) == 0)) {
            json_array_remove(markets, i);
            if (settings.markets[i].name)
                free(settings.markets[i].name);
            if (settings.markets[i].min_amount)
                mpd_del(settings.markets[i].min_amount);
            if (settings.markets[i].money)
                free(settings.markets[i].money);
            if (settings.markets[i].stock)
                free(settings.markets[i].stock);

            if (i+1<settings.market_num) { // move i+1 element to i, so i should not ++
                memcpy(&settings.markets[i], &settings.markets[i+1], sizeof(struct market));
            }

            settings.market_num--;
            save_market = true;
        } else {
            ++i;
        }
    }

    json_t *asset = NULL;
    json_t *name_obj = NULL;
    for (size_t i = 0; i < settings.asset_num; ++i) {
        asset = json_array_get(assets, i);
        if (!json_is_object(asset))
            continue;

        name_obj = json_object_get(asset, "name");
        if (!json_is_string(name_obj))
            continue;

        if (strcmp(name, json_string_value(name_obj)) == 0) {
            json_array_remove(assets, i);
            if (settings.assets[i].name)
                free(settings.assets[i].name);
            save = true;
        }

        if (save && (i+1<settings.asset_num)) {
            settings.assets[i].name = settings.assets[i+1].name;
            settings.assets[i].prec_save = settings.assets[i+1].prec_save;
            settings.assets[i].prec_show = settings.assets[i+1].prec_show;
        }
    }

    if (save) {
        settings.asset_num--;
    }

    if (save || save_market)
        save_config(root, true);

    json_decref(root);
    return 0;
}

static int load_assets(json_t *root, const char *key)
{
    json_t *node = json_object_get(root, key);
    if (!node || !json_is_array(node)) {
        return -__LINE__;
    }

    settings.asset_array_num = (json_array_size(node)/MAX_ASSET_ARRAY_NUM+1)*MAX_ASSET_ARRAY_NUM;
    settings.asset_num = json_array_size(node);
    settings.assets = malloc(sizeof(struct asset) * settings.asset_array_num);
    for (size_t i = 0; i < settings.asset_num; ++i) {
        json_t *row = json_array_get(node, i);
        if (!json_is_object(row))
            return -__LINE__;
        ERR_RET_LN(read_cfg_str(row, "name", &settings.assets[i].name, NULL));
        ERR_RET_LN(read_cfg_int(row, "prec_save", &settings.assets[i].prec_save, true, 0));
        ERR_RET_LN(read_cfg_int(row, "prec_show", &settings.assets[i].prec_show, false, settings.assets[i].prec_save));
        if (strlen(settings.assets[i].name) > ASSET_NAME_MAX_LEN)
            return -__LINE__;
    }

    return 0;
}

int add_market_into_config(const char* name, const char* stock, const char* money, int stock_prec, int money_prec, int fee_prec, mpd_t* min_amount, int add)
{
    bool save = false;
    json_t* root = reload_config();
    if (!root) {
        return -1;
    }

    json_t *node = json_object_get(root, "markets");
    if (!node || !json_is_array(node)) {
        json_decref(root);
        return -2;
    }

    if (add) {
        // add settings
        if (settings.market_num >= settings.market_array_num) {
            settings.market_array_num += MAX_MARKET_ARRAY_NUM;
            settings.markets = realloc(settings.markets, sizeof(struct market) * (settings.market_array_num));
            if (!settings.markets) {
                log_error("realloc markets fail");
                json_decref(root);
                return -3;
            }
        }

        settings.markets[settings.market_num].name = strdup(name);
        settings.markets[settings.market_num].fee_prec = fee_prec;
        settings.markets[settings.market_num].min_amount = mpd_qncopy(min_amount);
        settings.markets[settings.market_num].money = strdup(money);
        settings.markets[settings.market_num].stock = strdup(stock);
        settings.markets[settings.market_num].stock_prec = stock_prec;
        settings.markets[settings.market_num].money_prec = money_prec;
        settings.market_num++;

        // add json data
        json_t *market = json_object();
        json_object_set_new(market, "name", json_string(name));

        json_t *stock_obj = json_object();
        json_object_set_new(stock_obj, "name", json_string(stock));
        json_object_set_new(stock_obj, "prec", json_integer(stock_prec));
        json_object_set_new(market, "stock", stock_obj);

        json_t *money_obj = json_object();
        json_object_set_new(money_obj, "name", json_string(money));
        json_object_set_new(money_obj, "prec", json_integer(money_prec));
        json_object_set_new(market, "money", money_obj);

        json_object_set_new(market, "fee_prec", json_integer(fee_prec));
        json_object_set_new_mpd(market, "min_amount", min_amount);
        json_array_append_new(node, market);
        save = true;
    } else {
        json_t *market = NULL;
        json_t *name_obj = NULL;
        for (size_t i = 0; i < settings.market_num; ++i) {
            market = json_array_get(node, i);
            if (!json_is_object(market))
                continue;

            name_obj = json_object_get(market, "name");
            if (!json_is_string(name_obj))
                continue;

            if (strcmp(name, json_string_value(name_obj)) == 0) {
                settings.markets[i].fee_prec = fee_prec;
                if (settings.markets[i].min_amount)
                    mpd_del(settings.markets[i].min_amount);
                settings.markets[i].min_amount = mpd_qncopy(min_amount);
                if (settings.markets[i].money)
                    free(settings.markets[i].money);
                settings.markets[i].money = strdup(money);
                if (settings.markets[i].stock)
                    free(settings.markets[i].stock);
                settings.markets[i].stock = strdup(stock);
                settings.markets[i].stock_prec = stock_prec;
                settings.markets[i].money_prec = money_prec;

                json_t *stock_obj = json_object();
                json_object_set_new(stock_obj, "name", json_string(stock));
                json_object_set_new(stock_obj, "prec", json_integer(stock_prec));
                json_object_set_new(market, "stock", stock_obj);
                json_t *money_obj = json_object();
                json_object_set_new(money_obj, "name", json_string(money));
                json_object_set_new(money_obj, "prec", json_integer(money_prec));
                json_object_set_new(market, "money", money_obj);
                json_object_set_new(market, "fee_prec", json_integer(fee_prec));
                json_object_set_new_mpd(market, "min_amount", min_amount);
                save = true;
                break;
            }
        }
    }

    if (save)
        save_config(root, true);

    json_decref(root);
    return 0;
}

int del_market_from_config(const char* name)
{
    bool save = false;
    json_t* root = reload_config();
    if (!root) {
        return -1;
    }

    json_t *node = json_object_get(root, "markets");
    if (!node || !json_is_array(node)) {
        json_decref(root);
        return -2;
    }

    json_t *market = NULL;
    json_t *name_obj = NULL;
    for (size_t i = 0; i < settings.market_num; ++i) {
        market = json_array_get(node, i);
        if (!json_is_object(market))
            continue;

        name_obj = json_object_get(market, "name");
        if (!json_is_string(name_obj))
            continue;

        if (strcmp(name, json_string_value(name_obj)) == 0) {
            json_array_remove(node, i);
            if (settings.markets[i].name)
                free(settings.markets[i].name);
            if (settings.markets[i].min_amount)
                mpd_del(settings.markets[i].min_amount);
            if (settings.markets[i].money)
                free(settings.markets[i].money);
            if (settings.markets[i].stock)
                free(settings.markets[i].stock);
            save = true;
            // break;
        }

        if (save && (i+1<settings.market_num)) {
            memcpy(&settings.markets[i], &settings.markets[i+1], sizeof(struct market));
            // settings.markets[i].name = settings.markets[i+1].name;
            // settings.markets[i].fee_prec = settings.markets[i+1].fee_prec;
            // settings.markets[i].min_amount = settings.markets[i+1].min_amount;
            // settings.markets[i].money = settings.markets[i+1].money;
            // settings.markets[i].stock = settings.markets[i+1].stock;
            // settings.markets[i].stock_prec = settings.markets[i+1].stock_prec;
            // settings.markets[i].money_prec = settings.markets[i+1].money_prec;
        }
    }

    if (save) {
        settings.market_num--;
        save_config(root, true);
    }

    json_decref(root);
    return 0;
}
#if 0
int del_market_from_config_by_asset(const char* name)
{
    bool save = false;
    json_t* root = reload_config()
    if (!root) {
        return -1;
    }

    json_t *node = json_object_get(root, "markets");
    if (!node || !json_is_array(node)) {
        json_decref(root);
        return -2;
    }

    json_t *market = NULL;
    json_t *stock_obj = NULL;
    json_t *money_obj = NULL;
    for (size_t i = 0; i < settings.market_num; ;) {
        market = json_array_get(node, i);
        if (!json_is_object(market)) {
            ++i;
            continue;
        }

        stock_obj = json_object_get(json_object_get(market, "stock"), "name");
        if (!json_is_string(stock_obj)) {
            ++i;
            continue;
        }
        money_obj = json_object_get(json_object_get(market, "money"), "name");
        if (!json_is_string(money_obj)) {
            ++i;
            continue;
        }

        if ((strcmp(name, json_string_value(stock_obj)) == 0) || (strcmp(name, json_string_value(money_obj)) == 0)) {
            json_array_remove(node, i);
            if (settings.markets[i].name)
                free(settings.markets[i].name);
            if (settings.markets[i].min_amount)
                mpd_del(settings.markets[i].min_amount);
            if (settings.markets[i].money)
                free(settings.markets[i].money);
            if (settings.markets[i].stock)
                free(settings.markets[i].stock);

            if (i+1<settings.market_num) { // move i+1 element to i, so i should not ++
                memcpy(&settings.markets[i], &settings.markets[i+1], sizeof(struct market));
            }

            settings.market_num--;
            save = true;
        } else {
            ++i;
        }
    }

    if (save) {
        save_config(root, true);
    }

    json_decref(root);
    return 0;
}
#endif
static int load_markets(json_t *root, const char *key)
{
    json_t *node = json_object_get(root, key);
    if (!node || !json_is_array(node)) {
        return -__LINE__;
    }

    settings.market_array_num = (json_array_size(node)/MAX_MARKET_ARRAY_NUM+1)*MAX_MARKET_ARRAY_NUM;
    settings.market_num = json_array_size(node);
    settings.markets = malloc(sizeof(struct market) * settings.market_array_num);
    for (size_t i = 0; i < settings.market_num; ++i) {
        json_t *row = json_array_get(node, i);
        if (!json_is_object(row))
            return -__LINE__;
        ERR_RET_LN(read_cfg_str(row, "name", &settings.markets[i].name, NULL));
        ERR_RET_LN(read_cfg_int(row, "fee_prec", &settings.markets[i].fee_prec, false, 4));
        ERR_RET_LN(read_cfg_mpd(row, "min_amount", &settings.markets[i].min_amount, "0.01"));

        json_t *stock = json_object_get(row, "stock");
        if (!stock || !json_is_object(stock))
            return -__LINE__;
        ERR_RET_LN(read_cfg_str(stock, "name", &settings.markets[i].stock, NULL));
        ERR_RET_LN(read_cfg_int(stock, "prec", &settings.markets[i].stock_prec, true, 0));

        json_t *money = json_object_get(row, "money");
        if (!money || !json_is_object(money))
            return -__LINE__;
        ERR_RET_LN(read_cfg_str(money, "name", &settings.markets[i].money, NULL));
        ERR_RET_LN(read_cfg_int(money, "prec", &settings.markets[i].money_prec, true, 0));
    }

    return 0;
}

static int read_config_from_json(json_t *root)
{
    int ret;
    ret = read_cfg_bool(root, "debug", &settings.debug, false, false);
    if (ret < 0) {
        printf("read debug config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_process(root, "process", &settings.process);
    if (ret < 0) {
        printf("load process config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_log(root, "log", &settings.log);
    if (ret < 0) {
        printf("load log config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_alert(root, "alert", &settings.alert);
    if (ret < 0) {
        printf("load alert config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_rpc_svr(root, "svr", &settings.svr);
    if (ret < 0) {
        printf("load svr config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_cli_svr(root, "cli", &settings.cli);
    if (ret < 0) {
        printf("load cli config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_mysql(root, "db_log", &settings.db_log);
    if (ret < 0) {
        printf("load log db config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_mysql(root, "db_history", &settings.db_history);
    if (ret < 0) {
        printf("load history db config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_assets(root, "assets");
    if (ret < 0) {
        printf("load assets config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_markets(root, "markets");
    if (ret < 0) {
        printf("load markets config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_str(root, "brokers", &settings.brokers, NULL);
    if (ret < 0) {
        printf("load brokers fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_int(root, "slice_interval", &settings.slice_interval, false, 86400);
    if (ret < 0) {
        printf("load slice_interval fail: %d", ret);
        return -__LINE__;
    }
    ret = read_cfg_int(root, "slice_keeptime", &settings.slice_keeptime, false, 86400 * 3);
    if (ret < 0) {
        printf("load slice_keeptime fail: %d", ret);
        return -__LINE__;
    }
    ret = read_cfg_int(root, "history_thread", &settings.history_thread, false, 10);
    if (ret < 0) {
        printf("load history_thread fail: %d", ret);
        return -__LINE__;
    }

    ERR_RET_LN(read_cfg_real(root, "cache_timeout", &settings.cache_timeout, false, 0.45));

    return 0;
}

int init_config(const char *path)
{
    json_error_t error;
    json_t *root = json_load_file(path, 0, &error);
    if (root == NULL) {
        printf("json_load_file from: %s fail: %s in line: %d\n", path, error.text, error.line);
        return -__LINE__;
    }
    if (!json_is_object(root)) {
        json_decref(root);
        return -__LINE__;
    }

    int ret = read_config_from_json(root);
    if (ret < 0) {
        json_decref(root);
        return ret;
    }
    json_decref(root);

    conf_path = strdup(path);

    return 0;
}

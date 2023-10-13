/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/15, create
 */

# include "me_config.h"
# include "me_position.h"
# include "ut_dict.h"

dict_t *dict_position;

static uint32_t position_dict_hash_function(const void *key){
    return dict_generic_hash_function(key, sizeof(struct position_key));
}

static void *position_dict_key_dup(const void *key){
    struct position_key *obj = malloc(sizeof(struct position_key));
    if (obj == NULL) return NULL;
    memcpy(obj, key, sizeof(struct position_key));
    return obj;
}

static void *position_dict_val_dup(const void *val){
    return mpd_qncopy(val);
}

static int position_dict_key_compare(const void *key1, const void *key2){
    return memcmp(key1, key2, sizeof(struct position_key));
}

static void position_dict_key_free(void *key){
    free(key);
}

static void position_dict_val_free(void *val){
    mpd_del(val);
}

static int init_dict(void){
    dict_types type;
    memset(&type, 0, sizeof(type));
    type.hash_function  = position_dict_hash_function;
    type.key_compare    = position_dict_key_compare;
    type.key_dup        = position_dict_key_dup;
    type.key_destructor = position_dict_key_free;
    // type.val_dup        = position_dict_val_dup;
    // type.val_destructor = position_dict_val_free;

    dict_position = dict_create(&type, 64);
    if (dict_position == NULL)
        return -__LINE__;

    return 0;
}

int init_position(){
    ERR_RET(init_dict());
    return 0;
}

int add_position(uint32_t user_id, char* market, uint32_t side, position_t *p){
    if (!p) return -1;
    struct position_key key;
    key.user_id = user_id;
    key.side = side;
    strncpy(key.market, market, sizeof(key.market));
    dict_entry *entry = dict_add(dict_position, &key, p);
    position_t *tmp = entry->val;
    return 0;
}

int del_position(uint32_t user_id, char* market, uint32_t side){
    dict_iterator *iter = dict_get_iterator(dict_position);
    if (iter == NULL)
        return -1;
    dict_entry *entry;
    struct position_key* key = NULL;
    while ((entry = dict_next(iter)) != NULL) {
        key = entry->key;
        if (user_id == key->user_id && strcmp(market, key->market) == 0 && side == key->side)
            dict_delete(dict_position, entry->key);
    }
    dict_release_iterator(iter);
    return 0;
}

position_t* get_position(uint32_t user_id, char* market, uint32_t side){
    struct position_key key;
    key.side = side;
    key.user_id = user_id;
    strncpy(key.market, market, sizeof(key.market));

    dict_entry *entry = dict_find(dict_position, &key);
    if (entry) {
        return entry->val;
    }

    return NULL;
}

position_t* initPosition(uint32_t user_id, const char* market, uint32_t pattern){
    position_t *position = malloc(sizeof(position_t));
    memset(position, 0, sizeof(position_t));
    position->id = 0;
    position->user_id = user_id;
    position->market = strdup(market);
    position->side = 0;
    position->pattern = pattern;
    position->leverage = mpd_new(&mpd_ctx);;
    position->position = mpd_new(&mpd_ctx);;
    position->frozen = mpd_new(&mpd_ctx);;
    position->price = mpd_new(&mpd_ctx);;
    position->principal = mpd_new(&mpd_ctx);;
    return position;
}
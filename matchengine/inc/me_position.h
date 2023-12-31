/*
 * Description: user balance manage
 *     History: yang@haipo.me, 2017/03/15, create
 */

# ifndef _ME_POSITION_H_
# define _ME_POSITION_H_

# include "me_config.h"

extern dict_t *dict_position;
extern dict_t *dict_position_mode;
extern uint64_t position_id_start;

typedef struct position_mode_t{
    uint32_t        id;
    uint32_t        user_id;
    char            *market;
    uint32_t        pattern;
    mpd_t           *leverage;
} position_mode_t;

typedef struct position_t{
    uint32_t        id;
    uint32_t        user_id;
    char            *market;
    uint32_t        side;
    uint32_t        pattern;
    mpd_t           *leverage;
    mpd_t           *position;
    mpd_t           *frozen;
    mpd_t           *price;
    mpd_t           *principal;
} position_t;

struct position_mode_key {
    uint32_t    user_id;
    char        market[MARKET_NAME_MAX_LEN + 1];
};

struct position_key {
    uint32_t    user_id;
    char        market[MARKET_NAME_MAX_LEN + 1];
    uint32_t    side;
};

int init_position(void);
int add_position(uint32_t user_id, char* market, uint32_t side, position_t *p);
position_t* get_position(uint32_t user_id, char* market, uint32_t side);
position_mode_t* get_position_mode(uint32_t user_id, char* market);
int add_position_mode_2(uint32_t user_id, char* market, position_mode_t *position_mode);
int del_position(uint32_t user_id, char* market, uint32_t side);
position_t *initPosition(uint32_t user_id, const char* market, uint32_t pattern);
# endif


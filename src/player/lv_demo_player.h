/**
 * @file lv_demo_player.h
 *
 */

#ifndef LV_DEMO_PLAYER_H
#define LV_DEMO_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

#if LV_USE_DEMO_PLAYER

/*********************
 *      DEFINES
 *********************/

#if LV_DEMO_PLAYER_LARGE
#  define LV_DEMO_PLAYER_HANDLE_SIZE  40
#else
#  define LV_DEMO_PLAYER_HANDLE_SIZE  20
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_demo_player(void);
const char * lv_demo_player_get_title(uint32_t track_id);
const char * lv_demo_player_get_artist(uint32_t track_id);
const char * lv_demo_player_get_genre(uint32_t track_id);
uint32_t lv_demo_player_get_track_length(uint32_t track_id);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_DEMO_PLAYER*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_PLAYER_H*/

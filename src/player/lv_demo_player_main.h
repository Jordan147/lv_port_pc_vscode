/**
 * @file lv_demo_player_main.h
 *
 */

#ifndef LV_DEMO_PLAYER_MAIN_H
#define LV_DEMO_PLAYER_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_player.h"

#if LV_USE_DEMO_PLAYER

#if LV_USE_GRID == 0
#error "LV_USE_GRID needs to be enabled"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_obj_t * lv_demo_player_main_create(lv_obj_t * parent);
void lv_demo_player_play(uint32_t id);
void lv_demo_player_resume(void);
void lv_demo_player_pause(void);
void lv_demo_player_album_next(bool next);

/**********************
 *      MACROS
 **********************/
#endif /*LV_USE_DEMO_PLAYER*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_PLAYER_MAIN_H*/

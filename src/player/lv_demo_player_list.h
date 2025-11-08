/**
 * @file lv_demo_player_list.h
 *
 */

#ifndef LV_DEMO_PLAYER_LIST_H
#define LV_DEMO_PLAYER_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_player.h"
#if LV_USE_DEMO_PLAYER

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_obj_t * lv_demo_player_list_create(lv_obj_t * parent);
void lv_demo_player_list_button_check(uint32_t track_id, bool state);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_DEMO_PLAYER*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_PLAYER_LIST_H*/

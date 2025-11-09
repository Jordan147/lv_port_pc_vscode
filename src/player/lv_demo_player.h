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

// Extended API for media system integration
uint32_t lv_demo_player_get_track_count(void);
bool lv_demo_player_is_audio_track(uint32_t track_id);

// Media type definitions (from main.c)
typedef enum
{
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_IMAGE
} media_type_t;

// External media system integration (declared in main.c)
extern const char *player_get_title(uint32_t track_id);
extern const char *player_get_artist(uint32_t track_id);
extern const char *player_get_genre(uint32_t track_id);
extern uint32_t player_get_track_length(uint32_t track_id);
extern uint32_t player_get_track_count(void);
extern bool player_is_audio_track(uint32_t track_id);
extern media_type_t get_media_type_by_id(uint32_t track_id);
extern const char *get_media_filepath_by_id(uint32_t track_id);
extern void init_player_media_system(void);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_DEMO_PLAYER*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_PLAYER_H*/

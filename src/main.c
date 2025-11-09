/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include <SDL.h>
#include "lv_demo_player.h"

#include "hal/hal.h"

/*********************
 *      DEFINES
 *********************/
#define MEDIA_PATH "/opt/media"
#define MAX_MEDIA_FILES 1000
#define MAX_FILENAME_LEN 256

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
  char filepath[MAX_FILENAME_LEN];
  char title[MAX_FILENAME_LEN];
  char artist[MAX_FILENAME_LEN];
  char genre[MAX_FILENAME_LEN];
  uint32_t duration; // in seconds
  media_type_t type;
} media_info_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void version_timer_cb(lv_timer_t *timer);
static media_type_t get_media_type(const char *filename);
static void scan_media_files(void);
static bool is_media_file(const char *filename);
static void populate_media_info(media_info_t *info, const char *filepath);
static int get_media_count(void);
static const char *get_media_title(uint32_t track_id);
static const char *get_media_artist(uint32_t track_id);
static const char *get_media_genre(uint32_t track_id);
static uint32_t get_media_duration(uint32_t track_id);

/**********************
 *  STATIC VARIABLES
 **********************/
static media_info_t media_list[MAX_MEDIA_FILES];
static int media_count = 0;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/

  /*Initialize LVGL*/
  lv_init();

  /*Initialize the HAL (display, input devices, tick) for LVGL*/
  sdl_hal_init(320, 480);

  /* Scan media files */
  scan_media_files();

  /* Run the default demo */
  /* To try a different demo or example, replace this with one of: */
  /* - lv_demo_benchmark(); */
  /* - lv_demo_stress(); */
  /* - lv_example_label_1(); */
  /* - etc. */
  // lv_demo_widgets();
  lv_demo_player();

  /* Show LVGL version in top-right corner for 5 seconds */
  static lv_obj_t *version_label = NULL;
  version_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(version_label, "LVGL v%d.%d.%d",
                        LVGL_VERSION_MAJOR,
                        LVGL_VERSION_MINOR,
                        LVGL_VERSION_PATCH);
  lv_obj_set_style_text_color(version_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(version_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(version_label, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(version_label, LV_OPA_70, 0);
  lv_obj_set_style_pad_all(version_label, 4, 0);
  lv_obj_set_style_radius(version_label, 3, 0);
  lv_obj_align(version_label, LV_ALIGN_TOP_RIGHT, -10, 10);

  /* Create timer to hide version label after 5 seconds */
  lv_timer_t *version_timer = lv_timer_create(version_timer_cb, 5000, version_label);
  lv_timer_set_repeat_count(version_timer, 1);

  while (1)
  {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }

  return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Timer callback to hide the version label
 */
static void version_timer_cb(lv_timer_t *timer)
{
  lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(timer);
  if (label)
  {
    lv_obj_del(label);
  }
}

/**
 * Get media type based on file extension
 */
static media_type_t get_media_type(const char *filename)
{
  const char *ext = strrchr(filename, '.');
  if (!ext)
    return MEDIA_TYPE_UNKNOWN;

  ext++; // Skip the dot

  // Audio files
  if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 ||
      strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "aac") == 0 ||
      strcasecmp(ext, "ogg") == 0 || strcasecmp(ext, "m4a") == 0)
  {
    return MEDIA_TYPE_AUDIO;
  }

  // Video files
  if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
      strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0 ||
      strcasecmp(ext, "wmv") == 0 || strcasecmp(ext, "webm") == 0)
  {
    return MEDIA_TYPE_VIDEO;
  }

  // Image files
  if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
      strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
      strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "webp") == 0)
  {
    return MEDIA_TYPE_IMAGE;
  }

  return MEDIA_TYPE_UNKNOWN;
}

/**
 * Check if file is a media file
 */
static bool is_media_file(const char *filename)
{
  return get_media_type(filename) != MEDIA_TYPE_UNKNOWN;
}

/**
 * Populate media info structure
 */
static void populate_media_info(media_info_t *info, const char *filepath)
{
  const char *filename = strrchr(filepath, '/');
  filename = filename ? filename + 1 : filepath;

  // Copy filepath
  strncpy(info->filepath, filepath, MAX_FILENAME_LEN - 1);
  info->filepath[MAX_FILENAME_LEN - 1] = '\0';

  // Extract title from filename (without extension)
  const char *ext = strrchr(filename, '.');
  int title_len = ext ? (ext - filename) : strlen(filename);
  title_len = title_len > MAX_FILENAME_LEN - 1 ? MAX_FILENAME_LEN - 1 : title_len;
  strncpy(info->title, filename, title_len);
  info->title[title_len] = '\0';

  // Set default values
  strcpy(info->artist, "Unknown Artist");
  strcpy(info->genre, "Unknown Genre");
  info->duration = 180; // Default 3 minutes
  info->type = get_media_type(filename);

  // For demonstration, vary some values based on file type
  if (info->type == MEDIA_TYPE_AUDIO)
  {
    strcpy(info->genre, "Music");
    info->duration = 180 + (rand() % 120); // 3-5 minutes
  }
  else if (info->type == MEDIA_TYPE_VIDEO)
  {
    strcpy(info->artist, "Director");
    strcpy(info->genre, "Video");
    info->duration = 1800 + (rand() % 3600); // 30-90 minutes
  }
  else if (info->type == MEDIA_TYPE_IMAGE)
  {
    strcpy(info->artist, "Photographer");
    strcpy(info->genre, "Image");
    info->duration = 0; // Images don't have duration
  }
}

/**
 * Scan media files in MEDIA_PATH
 */
static void scan_media_files(void)
{
  DIR *dir;
  struct dirent *entry;
  struct stat file_stat;
  char filepath[MAX_FILENAME_LEN];

  media_count = 0;

  printf("Scanning media files in: %s\n", MEDIA_PATH);

  dir = opendir(MEDIA_PATH);
  if (dir == NULL)
  {
    printf("Warning: Could not open media directory %s, using default media list\n", MEDIA_PATH);
    // Fill with default media entries
    strcpy(media_list[0].title, "Waiting for true love");
    strcpy(media_list[0].artist, "The John Smith Band");
    strcpy(media_list[0].genre, "Rock - 1997");
    strcpy(media_list[0].filepath, "/default/track1.mp3");
    media_list[0].duration = 74;
    media_list[0].type = MEDIA_TYPE_AUDIO;

    strcpy(media_list[1].title, "Need a Better Future");
    strcpy(media_list[1].artist, "My True Name");
    strcpy(media_list[1].genre, "Drum'n bass - 2016");
    strcpy(media_list[1].filepath, "/default/track2.mp3");
    media_list[1].duration = 146;
    media_list[1].type = MEDIA_TYPE_AUDIO;

    media_count = 2;
    return;
  }

  while ((entry = readdir(dir)) != NULL && media_count < MAX_MEDIA_FILES)
  {
    // Skip hidden files and directories
    if (entry->d_name[0] == '.')
      continue;

    snprintf(filepath, sizeof(filepath), "%s/%s", MEDIA_PATH, entry->d_name);

    if (stat(filepath, &file_stat) == 0)
    {
      // Only process regular files
      if (S_ISREG(file_stat.st_mode) && is_media_file(entry->d_name))
      {
        populate_media_info(&media_list[media_count], filepath);
        printf("Found media: %s (%s)\n", media_list[media_count].title,
               media_list[media_count].type == MEDIA_TYPE_AUDIO ? "Audio" : media_list[media_count].type == MEDIA_TYPE_VIDEO ? "Video"
                                                                        : media_list[media_count].type == MEDIA_TYPE_IMAGE   ? "Image"
                                                                                                                             : "Unknown");
        media_count++;
      }
    }
  }

  closedir(dir);
  printf("Scanned %d media files\n", media_count);
}

/**
 * Get total media count
 */
static int get_media_count(void)
{
  return media_count;
}

/**
 * Get media title by ID
 */
static const char *get_media_title(uint32_t track_id)
{
  if (track_id >= media_count)
    return "Unknown";
  return media_list[track_id].title;
}

/**
 * Get media artist by ID
 */
static const char *get_media_artist(uint32_t track_id)
{
  if (track_id >= media_count)
    return "Unknown Artist";
  return media_list[track_id].artist;
}

/**
 * Get media genre by ID
 */
static const char *get_media_genre(uint32_t track_id)
{
  static char genre_with_ext[128];
  
  if (track_id >= media_count)
    return "Unknown Genre";
  
  // Get file extension
  const char *filepath = media_list[track_id].filepath;
  const char *ext = strrchr(filepath, '.');
  if (ext != NULL) {
    ext++; // Skip the dot
    // Format: "Genre - EXT"
    snprintf(genre_with_ext, sizeof(genre_with_ext), "%s - %s", 
             media_list[track_id].genre, ext);
    return genre_with_ext;
  }
  
  return media_list[track_id].genre;
}

/**
 * Get media duration by ID
 */
static uint32_t get_media_duration(uint32_t track_id)
{
  if (track_id >= media_count)
    return 180;
  return media_list[track_id].duration;
}

/**
 * Get media type by ID
 */
media_type_t get_media_type_by_id(uint32_t track_id)
{
  if (track_id >= media_count)
    return MEDIA_TYPE_UNKNOWN;
  return media_list[track_id].type;
}

/**
 * Get media file path by track ID
 */
const char *get_media_filepath_by_id(uint32_t track_id)
{
  if (track_id >= media_count)
    return NULL;
  return media_list[track_id].filepath;
}

/**
 * Initialize player media system - sets up function pointers for player demo
 */
void init_player_media_system(void)
{
  // This function can be used by the player demo to initialize
  // any required systems. Currently just ensures media is scanned.
  if (media_count == 0)
  {
    scan_media_files();
  }
}

/**
 * Media system integration functions for player demo
 */
const char *player_get_title(uint32_t track_id)
{
  return get_media_title(track_id);
}

const char *player_get_artist(uint32_t track_id)
{
  return get_media_artist(track_id);
}

const char *player_get_genre(uint32_t track_id)
{
  return get_media_genre(track_id);
}

uint32_t player_get_track_length(uint32_t track_id)
{
  return get_media_duration(track_id);
}

uint32_t player_get_track_count(void)
{
  return media_count;
}

bool player_is_audio_track(uint32_t track_id)
{
  return get_media_type_by_id(track_id) == MEDIA_TYPE_AUDIO;
}

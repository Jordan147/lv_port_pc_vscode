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

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void version_timer_cb(lv_timer_t * timer);

/**********************
 *  STATIC VARIABLES
 **********************/

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

  /* Run the default demo */
  /* To try a different demo or example, replace this with one of: */
  /* - lv_demo_benchmark(); */
  /* - lv_demo_stress(); */
  /* - lv_example_label_1(); */
  /* - etc. */
  // lv_demo_widgets();
  lv_demo_player();

  /* Show LVGL version in top-right corner for 5 seconds */
  static lv_obj_t * version_label = NULL;
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
  lv_timer_t * version_timer = lv_timer_create(version_timer_cb, 5000, version_label);
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
static void version_timer_cb(lv_timer_t * timer)
{
    lv_obj_t * label = (lv_obj_t*)lv_timer_get_user_data(timer);
    if(label) {
        lv_obj_del(label);
    }
}

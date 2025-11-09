/**
 * @file lv_demo_player_main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_player_main.h"
#if LV_USE_DEMO_PLAYER

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "lv_demo_player_list.h"
#include "assets/spectrum_1.h"
#include "assets/spectrum_2.h"
#include "assets/spectrum_3.h"
#include "lvgl_private.h"
#if LV_USE_GIF
#include "../../lvgl/src/libs/gif/lv_gif.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define INTRO_TIME 2000
#define BAR_COLOR1 lv_color_hex(0xe9dbfc)
#define BAR_COLOR2 lv_color_hex(0x6f8af6)
#define BAR_COLOR3 lv_color_hex(0xffffff)
#if LV_DEMO_PLAYER_LARGE
#define BAR_COLOR1_STOP 160
#define BAR_COLOR2_STOP 200
#define BAR_REST_RADIUS 165
#else
#define BAR_COLOR1_STOP 80
#define BAR_COLOR2_STOP 100
#define BAR_REST_RADIUS 82
#endif
#define BAR_COLOR3_STOP (LV_MAX(LV_HOR_RES, LV_VER_RES) / 3)
#define BAR_CNT 20
#define DEG_STEP (180 / BAR_CNT)
#define BAND_CNT 4
#define BAR_PER_BAND_CNT (BAR_CNT / BAND_CNT)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t *create_cont(lv_obj_t *parent);
static void create_wave_images(lv_obj_t *parent);
static lv_obj_t *create_title_box(lv_obj_t *parent);
static lv_obj_t *create_icon_box(lv_obj_t *parent);
static lv_obj_t *create_spectrum_obj(lv_obj_t *parent);
static lv_obj_t *create_ctrl_box(lv_obj_t *parent);
static lv_obj_t *create_handle(lv_obj_t *parent);

static void spectrum_anim_cb(void *a, int32_t v);
static void start_anim_cb(void *var, int32_t v);
static void del_counter_timer_cb(lv_event_t *e);
static void spectrum_draw_event_cb(lv_event_t *e);
static lv_obj_t *album_image_create(lv_obj_t *parent);
static void album_gesture_event_cb(lv_event_t *e);
static void play_event_click_cb(lv_event_t *e);
static void prev_click_event_cb(lv_event_t *e);
static void next_click_event_cb(lv_event_t *e);
static void timer_cb(lv_timer_t *t);
static void track_load(uint32_t id);
static void stop_start_anim(lv_timer_t *t);
static void spectrum_end_cb(lv_anim_t *a);
static void album_fade_anim_cb(void *var, int32_t v);
static int32_t get_cos(int32_t deg, int32_t a);
static int32_t get_sin(int32_t deg, int32_t a);
static void cover_spectrum_timer_cb(lv_timer_t *timer);
static void switch_cover_spectrum_display(void);

static void setup_fullsize_image_display(void);
static void cover_cycle_timer_cb(lv_timer_t *timer);
static void update_album_cover_for_audio(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *main_cont;
static lv_obj_t *spectrum_obj;
static lv_obj_t *title_label;
static lv_obj_t *artist_label;
static lv_obj_t *genre_label;
static lv_obj_t *time_obj;
static lv_obj_t *album_image_obj;
static lv_obj_t *media_image_obj;  // For displaying actual image files
static lv_obj_t *slider_obj;
static uint32_t spectrum_i = 0;
static uint32_t spectrum_i_pause = 0;
static uint32_t bar_ofs = 0;
static uint32_t bar_rot = 0;
static uint32_t time_act;
static lv_timer_t *stop_start_anim_timer;
static lv_timer_t *sec_counter_timer;
static const lv_font_t *font_small;
static const lv_font_t *font_large;
static uint32_t track_id;
static bool playing;
static bool start_anim;
static int32_t start_anim_values[40];
static lv_obj_t *play_obj;
static const uint16_t (*spectrum)[4];
static uint32_t spectrum_len;
static const uint16_t rnd_array[30] = {994, 285, 553, 11, 792, 707, 966, 641, 852, 827, 44, 352, 146, 581, 490, 80, 729, 58, 695, 940, 724, 561, 124, 653, 27, 292, 557, 506, 382, 199};

static void delayed_track_load_cb(lv_timer_t *timer);

// Audio-specific animation variables
static lv_timer_t *cover_spectrum_timer = NULL;
static bool show_cover = true;                         // true = show cover, false = show spectrum
static uint32_t cover_spectrum_switch_interval = 5000; // 5 seconds

// Cover cycling variables
static uint32_t current_cover_index = 0; // Index of current cover (0-2)
static lv_timer_t *cover_cycle_timer = NULL;
static uint32_t cover_cycle_interval = 3000; // 3 seconds for cover cycling

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*
 * Callback adapter function to convert parameter types to avoid compile-time
 * warning.
 */
static void _image_set_scale_anim_cb(void *obj, int32_t scale)
{
    lv_image_set_scale((lv_obj_t *)obj, (uint16_t)scale);
}

/*
 * Callback adapter function to convert parameter types to avoid compile-time
 * warning.
 */
static void _obj_set_x_anim_cb(void *obj, int32_t x)
{
    lv_obj_set_x((lv_obj_t *)obj, (int32_t)x);
}

lv_obj_t *lv_demo_player_main_create(lv_obj_t *parent)
{
    font_small = LV_FONT_DEFAULT;
    font_large = LV_FONT_DEFAULT;

#if LV_DEMO_PLAYER_LARGE
#if LV_FONT_MONTSERRAT_22
    font_small = &lv_font_montserrat_22;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_22 is not enabled for the music demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_32
    font_large = &lv_font_montserrat_32;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_32 is not enabled for the music demo. Using LV_FONT_DEFAULT instead.");
#endif
#else
#if LV_FONT_MONTSERRAT_12
    font_small = &lv_font_montserrat_12;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the music demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_16
    font_large = &lv_font_montserrat_16;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the music demo. Using LV_FONT_DEFAULT instead.");
#endif
#endif

    /*Create the content of the music player*/
    lv_obj_t *cont = create_cont(parent);

    create_wave_images(cont);
    lv_obj_t *title_box = create_title_box(cont);
    lv_obj_t *icon_box = create_icon_box(cont);
    lv_obj_t *ctrl_box = create_ctrl_box(cont);
    spectrum_obj = create_spectrum_obj(cont);
    lv_obj_t *handle_box = create_handle(cont);

#if LV_DEMO_PLAYER_ROUND
    lv_obj_set_style_pad_hor(cont, LV_HOR_RES / 6, 0);
#endif

    /*Arrange the content into a grid*/
#if LV_DEMO_PLAYER_SQUARE || LV_DEMO_PLAYER_ROUND
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t grid_rows[] = {LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                  0,                          /*Spectrum obj, set later*/
                                  LV_GRID_CONTENT,            /*Title box*/
                                  LV_GRID_FR(3),              /*Spacer*/
                                  LV_GRID_CONTENT,            /*Icon box*/
                                  LV_GRID_FR(3),              /*Spacer*/
                                  LV_GRID_CONTENT,            /*Control box*/
                                  LV_GRID_FR(3),              /*Spacer*/
                                  LV_GRID_CONTENT,            /*Handle box*/
                                  LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                  LV_GRID_TEMPLATE_LAST};

    grid_rows[1] = LV_VER_RES;

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 8, 1);
#elif LV_DEMO_PLAYER_LANDSCAPE == 0
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Title box*/
                                        LV_GRID_FR(3),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Icon box*/
                                        LV_GRID_FR(3),              /*Spacer*/
#if LV_DEMO_PLAYER_LARGE == 0
                                        250, /*Spectrum obj*/
#else
                                        480, /*Spectrum obj*/
#endif
                                        LV_GRID_FR(3),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Control box*/
                                        LV_GRID_FR(3),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Handle box*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                        LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 8, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 10, 1);
#else
    /*Arrange the content into a grid*/
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Title box*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Icon box*/
                                        LV_GRID_FR(3),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Control box*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_GRID_CONTENT,            /*Handle box*/
                                        LV_GRID_FR(1),              /*Spacer*/
                                        LV_DEMO_PLAYER_HANDLE_SIZE, /*Spacing*/
                                        LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(cont, grid_cols, grid_rows);
    lv_obj_set_style_grid_row_align(cont, LV_GRID_ALIGN_SPACE_BETWEEN, 0);
    lv_obj_set_grid_cell(title_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(icon_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(ctrl_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    lv_obj_set_grid_cell(handle_box, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 8, 1);
    lv_obj_set_grid_cell(spectrum_obj, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 1, 9);
#endif

    sec_counter_timer = lv_timer_create(timer_cb, 1000, NULL);
    lv_timer_pause(sec_counter_timer);

    /*Animate in the content after the intro time*/
    lv_anim_t a;

    start_anim = true;

    stop_start_anim_timer = lv_timer_create(stop_start_anim, INTRO_TIME + 3000, NULL);
    lv_timer_set_repeat_count(stop_start_anim_timer, 1);

    lv_anim_init(&a);

    uint32_t i;
    lv_anim_set_exec_cb(&a, start_anim_cb);
    lv_anim_set_values(&a, LV_MAX(LV_HOR_RES, LV_VER_RES) / 2, 0);
    lv_anim_set_path_cb(&a, lv_anim_path_bounce);
    for (i = 0; i < BAR_CNT; i++)
    {
        lv_anim_set_delay(&a, INTRO_TIME - 200 + (rnd_array[i] % 200));
        lv_anim_set_duration(&a, 2500 + (rnd_array[i] % 500));
        lv_anim_set_var(&a, &start_anim_values[i]);
        lv_anim_start(&a);
    }

    lv_obj_fade_in(title_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(icon_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(ctrl_box, 1000, INTRO_TIME + 1000);
    lv_obj_fade_in(handle_box, 1000, INTRO_TIME + 1000);

    // Don't fade in album and spectrum objects initially -
    // track_load() will handle their visibility based on media type

    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (album_image_obj)
    {
        lv_anim_set_var(&a, album_image_obj);
        lv_anim_set_duration(&a, 1000);
        lv_anim_set_delay(&a, INTRO_TIME + 1000);
        lv_anim_set_values(&a, 1, LV_SCALE_NONE);
        lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
        lv_anim_set_completed_cb(&a, NULL);
        lv_anim_start(&a);
    }

    /* Create an intro from a logo + label */
    LV_IMAGE_DECLARE(img_lv_demo_player_logo);
    lv_obj_t *logo = lv_image_create(lv_screen_active());
    lv_image_set_src(logo, &img_lv_demo_player_logo);
    lv_obj_move_to_index(logo, -1);
    lv_obj_align_to(logo, spectrum_obj, LV_ALIGN_CENTER, 0, 0);

#if LV_DEMO_PLAYER_SQUARE == 0 && LV_DEMO_PLAYER_ROUND == 0
    lv_obj_t *title = lv_label_create(lv_screen_active());
    lv_label_set_text(title, "LVGL Demo\nMusic player");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, font_large, 0);
    lv_obj_set_style_text_line_space(title, 8, 0);
    lv_obj_fade_out(title, 500, INTRO_TIME);
    lv_obj_align_to(title, logo, LV_ALIGN_OUT_LEFT_MID, -20, 0);
#endif

    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_var(&a, logo);
    lv_anim_set_duration(&a, 400);
    lv_anim_set_delay(&a, INTRO_TIME + 800);
    lv_anim_set_values(&a, LV_SCALE_NONE, 10);
    lv_anim_set_completed_cb(&a, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&a);

    lv_obj_update_layout(main_cont);

    // Delay the track loading to avoid issues with initialization
    lv_timer_create(delayed_track_load_cb, 100, NULL);

    return main_cont;
}

void lv_demo_player_album_next(bool next)
{
    uint32_t total_tracks = lv_demo_player_get_track_count();
    if (total_tracks == 0)
        return; // No tracks available

    uint32_t id = track_id;
    if (next)
    {
        id++;
        if (id >= total_tracks)
            id = 0;
    }
    else
    {
        if (id == 0)
        {
            id = total_tracks - 1;
        }
        else
        {
            id--;
        }
    }

    if (playing)
    {
        lv_demo_player_play(id);
    }
    else
    {
        track_load(id);
    }
}

void lv_demo_player_play(uint32_t id)
{
    track_load(id);

    lv_demo_player_resume();
}

void lv_demo_player_resume(void)
{
    playing = true;
    spectrum_i = spectrum_i_pause;

    // Check if current track is audio
    if (lv_demo_player_is_audio_track(track_id))
    {
        // For audio files, start cover/spectrum switching animation
        show_cover = true; // Start with cover
        if (album_image_obj)
        {
            lv_obj_set_style_opa(album_image_obj, LV_OPA_COVER, 0);
        }
        lv_obj_set_style_opa(spectrum_obj, LV_OPA_TRANSP, 0);

        // Start timer for switching between cover and spectrum
        if (cover_spectrum_timer)
        {
            lv_timer_delete(cover_spectrum_timer);
        }
        cover_spectrum_timer = lv_timer_create(cover_spectrum_timer_cb, cover_spectrum_switch_interval, NULL);

        // Start timer for cycling through covers during cover display
        if (cover_cycle_timer)
        {
            lv_timer_delete(cover_cycle_timer);
        }
        cover_cycle_timer = lv_timer_create(cover_cycle_timer_cb, cover_cycle_interval, NULL);

        // Start spectrum animation (but hidden initially)
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_values(&a, spectrum_i, spectrum_len - 1);
        lv_anim_set_exec_cb(&a, spectrum_anim_cb);
        lv_anim_set_var(&a, spectrum_obj);
        lv_anim_set_duration(&a, ((spectrum_len - spectrum_i) * 1000) / 30);
        lv_anim_set_reverse_duration(&a, 0);
        lv_anim_set_completed_cb(&a, spectrum_end_cb);
        lv_anim_start(&a);
    }
    else
    {
        // For non-audio files (images/videos), hide both cover and spectrum
        if (album_image_obj)
        {
            lv_obj_set_style_opa(album_image_obj, LV_OPA_TRANSP, 0);
        }
        lv_obj_set_style_opa(spectrum_obj, LV_OPA_TRANSP, 0);

        // No spectrum animation or cover switching for non-audio files
        if (cover_spectrum_timer)
        {
            lv_timer_delete(cover_spectrum_timer);
            cover_spectrum_timer = NULL;
        }

        // No cover cycling for non-audio files
        if (cover_cycle_timer)
        {
            lv_timer_delete(cover_cycle_timer);
            cover_cycle_timer = NULL;
        }
    }

    if (sec_counter_timer)
        lv_timer_resume(sec_counter_timer);
    lv_slider_set_range(slider_obj, 0, lv_demo_player_get_track_length(track_id));

    lv_obj_add_state(play_obj, LV_STATE_CHECKED);
}

void lv_demo_player_pause(void)
{
    playing = false;
    spectrum_i_pause = spectrum_i;
    spectrum_i = 0;

    // Stop cover/spectrum switching timer
    if (cover_spectrum_timer)
    {
        lv_timer_delete(cover_spectrum_timer);
        cover_spectrum_timer = NULL;
    }

    // Stop cover cycling timer
    if (cover_cycle_timer)
    {
        lv_timer_delete(cover_cycle_timer);
        cover_cycle_timer = NULL;
    }

    lv_anim_delete(spectrum_obj, spectrum_anim_cb);
    lv_obj_invalidate(spectrum_obj);
    if (album_image_obj)
    {
        lv_image_set_scale(album_image_obj, LV_SCALE_NONE);
    }

    // When paused, always show cover
    if (album_image_obj)
    {
        lv_obj_set_style_opa(album_image_obj, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_opa(spectrum_obj, LV_OPA_TRANSP, 0);

    if (sec_counter_timer)
        lv_timer_pause(sec_counter_timer);
    lv_obj_remove_state(play_obj, LV_STATE_CHECKED);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t *create_cont(lv_obj_t *parent)
{
    /*A transparent container in which the player section will be scrolled*/
    main_cont = lv_obj_create(parent);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_style_all(main_cont); /*Make it transparent*/
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_snap_y(main_cont, LV_SCROLL_SNAP_CENTER); /*Snap the children to the center*/

    /*Create a container for the player*/
    lv_obj_t *player = lv_obj_create(main_cont);
    lv_obj_set_y(player, -LV_DEMO_PLAYER_HANDLE_SIZE);
#if LV_DEMO_PLAYER_SQUARE || LV_DEMO_PLAYER_ROUND
    lv_obj_set_size(player, LV_HOR_RES, 2 * LV_VER_RES + LV_DEMO_PLAYER_HANDLE_SIZE * 2);
#else
    lv_obj_set_size(player, LV_HOR_RES, LV_VER_RES + LV_DEMO_PLAYER_HANDLE_SIZE * 2);
#endif
    lv_obj_remove_flag(player, LV_OBJ_FLAG_SNAPPABLE);

    lv_obj_set_style_bg_color(player, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(player, 0, 0);
    lv_obj_set_style_pad_all(player, 0, 0);
    lv_obj_set_scroll_dir(player, LV_DIR_VER);

    /* Transparent placeholders below the player container
     * It is used only to snap it to center.*/
    lv_obj_t *placeholder1 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder1);
    lv_obj_remove_flag(placeholder1, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *placeholder2 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder2);
    lv_obj_remove_flag(placeholder2, LV_OBJ_FLAG_CLICKABLE);

#if LV_DEMO_PLAYER_SQUARE || LV_DEMO_PLAYER_ROUND
    lv_obj_t *placeholder3 = lv_obj_create(main_cont);
    lv_obj_remove_style_all(placeholder3);
    lv_obj_remove_flag(placeholder3, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_size(placeholder1, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder1, 0);

    lv_obj_set_size(placeholder2, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder2, LV_VER_RES);

    lv_obj_set_size(placeholder3, lv_pct(100), LV_VER_RES - 2 * LV_DEMO_PLAYER_HANDLE_SIZE);
    lv_obj_set_y(placeholder3, 2 * LV_VER_RES + LV_DEMO_PLAYER_HANDLE_SIZE);
#else
    lv_obj_set_size(placeholder1, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder1, 0);

    lv_obj_set_size(placeholder2, lv_pct(100), LV_VER_RES - 2 * LV_DEMO_PLAYER_HANDLE_SIZE);
    lv_obj_set_y(placeholder2, LV_VER_RES + LV_DEMO_PLAYER_HANDLE_SIZE);
#endif

    lv_obj_update_layout(main_cont);

    return player;
}

static void create_wave_images(lv_obj_t *parent)
{
    LV_IMAGE_DECLARE(img_lv_demo_player_wave_top);
    LV_IMAGE_DECLARE(img_lv_demo_player_wave_bottom);
    lv_obj_t *wave_top = lv_image_create(parent);
    lv_image_set_src(wave_top, &img_lv_demo_player_wave_top);
    lv_image_set_inner_align(wave_top, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_width(wave_top, LV_HOR_RES);
    lv_obj_align(wave_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(wave_top, LV_OBJ_FLAG_IGNORE_LAYOUT);

    lv_obj_t *wave_bottom = lv_image_create(parent);
    lv_image_set_src(wave_bottom, &img_lv_demo_player_wave_bottom);
    lv_image_set_inner_align(wave_bottom, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_width(wave_bottom, LV_HOR_RES);
    lv_obj_align(wave_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(wave_bottom, LV_OBJ_FLAG_IGNORE_LAYOUT);

    LV_IMAGE_DECLARE(img_lv_demo_player_corner_left);
    LV_IMAGE_DECLARE(img_lv_demo_player_corner_right);
    lv_obj_t *wave_corner = lv_image_create(parent);
    lv_image_set_src(wave_corner, &img_lv_demo_player_corner_left);
#if LV_DEMO_PLAYER_ROUND == 0
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_LEFT, 0, 0);
#else
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_LEFT, -LV_HOR_RES / 6, 0);
#endif
    lv_obj_add_flag(wave_corner, LV_OBJ_FLAG_IGNORE_LAYOUT);

    wave_corner = lv_image_create(parent);
    lv_image_set_src(wave_corner, &img_lv_demo_player_corner_right);
#if LV_DEMO_PLAYER_ROUND == 0
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#else
    lv_obj_align(wave_corner, LV_ALIGN_BOTTOM_RIGHT, LV_HOR_RES / 6, 0);
#endif
    lv_obj_add_flag(wave_corner, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

static lv_obj_t *create_title_box(lv_obj_t *parent)
{

    /*Create the titles*/
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    title_label = lv_label_create(cont);
    lv_obj_set_style_text_font(title_label, font_large, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x504d6d), 0);
    lv_label_set_text(title_label, lv_demo_player_get_title(track_id));
    lv_obj_set_height(title_label, lv_font_get_line_height(font_large) * 3 / 2);

    artist_label = lv_label_create(cont);
    lv_obj_set_style_text_font(artist_label, font_small, 0);
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0x504d6d), 0);
    lv_label_set_text(artist_label, lv_demo_player_get_artist(track_id));

    genre_label = lv_label_create(cont);
    lv_obj_set_style_text_font(genre_label, font_small, 0);
    lv_obj_set_style_text_color(genre_label, lv_color_hex(0x8a86b8), 0);
    lv_label_set_text(genre_label, lv_demo_player_get_genre(track_id));

    return cont;
}

static lv_obj_t *create_icon_box(lv_obj_t *parent)
{

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon;
    LV_IMAGE_DECLARE(img_lv_demo_player_icon_1);
    LV_IMAGE_DECLARE(img_lv_demo_player_icon_2);
    LV_IMAGE_DECLARE(img_lv_demo_player_icon_3);
    LV_IMAGE_DECLARE(img_lv_demo_player_icon_4);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_icon_1);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_icon_2);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_icon_3);
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_icon_4);

    return cont;
}

static lv_obj_t *create_spectrum_obj(lv_obj_t *parent)
{
    /*Create the spectrum visualizer*/
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
#if LV_DEMO_PLAYER_LARGE
    lv_obj_set_height(obj, 500);
#else
    lv_obj_set_height(obj, 250);
#endif
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, spectrum_draw_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_refresh_ext_draw_size(obj);

    // Initially hide spectrum object - will be shown when audio files are loaded
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);

    // Create album image normally - we'll handle visibility later
    album_image_obj = album_image_create(obj);

    // Initially hide album image as well
    if (album_image_obj)
    {
        lv_obj_set_style_opa(album_image_obj, LV_OPA_TRANSP, 0);
    }

    // Create media image object for displaying actual image files
    media_image_obj = lv_image_create(obj);
    lv_obj_set_width(media_image_obj, lv_pct(100));
    lv_obj_set_height(media_image_obj, lv_pct(100));
    lv_obj_align(media_image_obj, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_inner_align(media_image_obj, LV_IMAGE_ALIGN_CONTAIN);  // Best fit: scale to fit container while maintaining aspect ratio
    lv_obj_add_flag(media_image_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);  // Initially hidden

    return obj;
}

static lv_obj_t *create_ctrl_box(lv_obj_t *parent)
{
    /*Create the control box*/
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
#if LV_DEMO_PLAYER_LARGE
    lv_obj_set_style_pad_bottom(cont, 17, 0);
#else
    lv_obj_set_style_pad_bottom(cont, 8, 0);
#endif
    static const int32_t grid_col[] = {LV_GRID_FR(2), LV_GRID_FR(3), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_row[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, grid_col, grid_row);

    LV_IMAGE_DECLARE(img_lv_demo_player_btn_loop);
    LV_IMAGE_DECLARE(img_lv_demo_player_btn_rnd);
    LV_IMAGE_DECLARE(img_lv_demo_player_btn_next);
    LV_IMAGE_DECLARE(img_lv_demo_player_btn_prev);
    LV_IMAGE_DECLARE(img_lv_demo_player_btn_play);
    LV_IMAGE_DECLARE(img_lv_demo_player_btn_pause);

    lv_obj_t *icon;
    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_btn_rnd);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_btn_loop);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_btn_prev);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(icon, prev_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    play_obj = lv_imagebutton_create(cont);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_player_btn_play, NULL);
    lv_imagebutton_set_src(play_obj, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_player_btn_pause, NULL);
    lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_grid_cell(play_obj, LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_add_event_cb(play_obj, play_event_click_cb, LV_EVENT_CLICKED, NULL);

    // Initially disable play button - will be enabled based on media type when track loads
    lv_obj_clear_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(play_obj, LV_OPA_30, 0);

    lv_obj_set_width(play_obj, img_lv_demo_player_btn_play.header.w);

    icon = lv_image_create(cont);
    lv_image_set_src(icon, &img_lv_demo_player_btn_next);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 4, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(icon, next_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    LV_IMAGE_DECLARE(img_lv_demo_player_slider_knob);
    slider_obj = lv_slider_create(cont);
    lv_obj_set_style_anim_duration(slider_obj, 100, 0);
    lv_obj_add_flag(slider_obj, LV_OBJ_FLAG_CLICKABLE); /*No input from the slider*/
    lv_obj_remove_flag(slider_obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

#if LV_DEMO_PLAYER_LARGE == 0
    lv_obj_set_height(slider_obj, 3);
#else
    lv_obj_set_height(slider_obj, 6);
#endif
    lv_obj_set_grid_cell(slider_obj, LV_GRID_ALIGN_STRETCH, 1, 4, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_set_style_bg_image_src(slider_obj, &img_lv_demo_player_slider_knob, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider_obj, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_obj, 20, LV_PART_KNOB);
    lv_obj_set_style_bg_grad_dir(slider_obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_obj, lv_color_hex(0x569af8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(slider_obj, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
    lv_obj_set_style_outline_width(slider_obj, 0, 0);
    lv_obj_add_event_cb(slider_obj, del_counter_timer_cb, LV_EVENT_DELETE, NULL);

    time_obj = lv_label_create(cont);
    lv_obj_set_style_text_font(time_obj, font_small, 0);
    lv_obj_set_style_text_color(time_obj, lv_color_hex(0x8a86b8), 0);
    lv_label_set_text(time_obj, "0:00");
    lv_obj_set_grid_cell(time_obj, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_add_event_cb(time_obj, del_counter_timer_cb, LV_EVENT_DELETE, NULL);

    return cont;
}

static lv_obj_t *create_handle(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);

    lv_obj_set_size(cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 8, 0);

    /*A handle to scroll to the track list*/
    lv_obj_t *handle_label = lv_label_create(cont);
    lv_label_set_text(handle_label, "ALL TRACKS");
    lv_obj_set_style_text_font(handle_label, font_small, 0);
    lv_obj_set_style_text_color(handle_label, lv_color_hex(0x8a86b8), 0);

    lv_obj_t *handle_rect = lv_obj_create(cont);
#if LV_DEMO_PLAYER_LARGE
    lv_obj_set_size(handle_rect, 40, 3);
#else
    lv_obj_set_size(handle_rect, 20, 2);
#endif

    lv_obj_set_style_bg_color(handle_rect, lv_color_hex(0x8a86b8), 0);
    lv_obj_set_style_border_width(handle_rect, 0, 0);

    return cont;
}

static void track_load(uint32_t id)
{
    spectrum_i = 0;
    time_act = 0;
    spectrum_i_pause = 0;
    lv_slider_set_value(slider_obj, 0, LV_ANIM_OFF);
    lv_label_set_text(time_obj, "0:00");

    if (id == track_id)
        return;

    // Reset cover index when loading new track
    current_cover_index = 0;

    bool next = false;
    uint32_t total_tracks = lv_demo_player_get_track_count();
    if (total_tracks > 0 && (track_id + 1) % total_tracks == id)
        next = true;

    lv_demo_player_list_button_check(track_id, false);

    track_id = id;

    lv_demo_player_list_button_check(id, true);

    lv_label_set_text(title_label, lv_demo_player_get_title(track_id));
    lv_label_set_text(artist_label, lv_demo_player_get_artist(track_id));
    lv_label_set_text(genre_label, lv_demo_player_get_genre(track_id));

    // Print filename when loading track
    const char *filepath = get_media_filepath_by_id(track_id);
    if (filepath)
    {
        printf("Loading track %u: %s\n", track_id, filepath);
    }

    media_type_t current_media_type = get_media_type_by_id(track_id);

    // Handle different media types - basic visibility control
    if (current_media_type == MEDIA_TYPE_AUDIO)
    {
        // Audio files: Enable play button, show spectrum and cover
        lv_obj_clear_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(play_obj, LV_OPA_COVER, 0);

        // Show spectrum and album for audio
        lv_obj_set_style_opa(spectrum_obj, LV_OPA_COVER, 0);
        if (album_image_obj)
        {
            lv_obj_set_style_opa(album_image_obj, LV_OPA_COVER, 0);
        }

        // Hide media image for audio files
        if (media_image_obj)
        {
            lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);
        }
    }
    else if (current_media_type == MEDIA_TYPE_IMAGE)
    {
        // Image files: Disable play button, hide spectrum and album covers
        lv_obj_clear_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(play_obj, LV_OPA_30, 0);

        // Show spectrum container for images (it holds media_image_obj)
        lv_obj_set_style_opa(spectrum_obj, LV_OPA_COVER, 0);

        // Hide album cover for images
        if (album_image_obj)
        {
            lv_obj_set_style_opa(album_image_obj, LV_OPA_TRANSP, 0);
        }

        // Load and display the actual image file
        if (media_image_obj)
        {
            const char *filepath = get_media_filepath_by_id(track_id);
            if (filepath)
            {
                // Convert path to LVGL file system format (A:/path/to/file)
                static char lv_path[512];
                snprintf(lv_path, sizeof(lv_path), "A:%s", filepath);

                // Check if file is a GIF
                size_t len = strlen(filepath);
                bool is_gif = (len > 4 &&
                               (strcasecmp(filepath + len - 4, ".gif") == 0));

                // Get parent before potentially deleting the object
                lv_obj_t *parent = lv_obj_get_parent(media_image_obj);

                // Delete old object and create appropriate type
                lv_obj_del(media_image_obj);

                if (is_gif)
                {
#if LV_USE_GIF
                    // For GIF files, use lv_gif widget
                    media_image_obj = lv_gif_create(parent);
                    lv_obj_set_width(media_image_obj, lv_pct(100));
                    lv_obj_set_height(media_image_obj, lv_pct(100));
                    lv_obj_align(media_image_obj, LV_ALIGN_CENTER, 0, 0);
                    lv_gif_set_src(media_image_obj, lv_path);
                    lv_image_set_inner_align(media_image_obj, LV_IMAGE_ALIGN_CONTAIN);  // Scale to fit while maintaining aspect ratio
                    lv_obj_set_style_opa(media_image_obj, LV_OPA_COVER, 0);
                    printf("✓ GIF loaded and displayed: %s\n", filepath);
#else
                    // GIF not supported, create empty image
                    media_image_obj = lv_image_create(parent);
                    lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);
                    printf("✗ GIF support not enabled: %s\n", filepath);
#endif
                }
                else
                {
                    // For regular images (JPG, PNG, BMP), use normal image widget
                    media_image_obj = lv_image_create(parent);
                    lv_obj_set_width(media_image_obj, lv_pct(100));
                    lv_obj_set_height(media_image_obj, lv_pct(100));
                    lv_obj_align(media_image_obj, LV_ALIGN_CENTER, 0, 0);
                    lv_image_set_inner_align(media_image_obj, LV_IMAGE_ALIGN_CONTAIN);  // Scale to fit while maintaining aspect ratio
                    lv_image_set_src(media_image_obj, lv_path);

                    // Check if image loaded successfully
                    const void *src = lv_image_get_src(media_image_obj);
                    if (src != NULL)
                    {
                        // Image loaded successfully, show it
                        lv_obj_set_style_opa(media_image_obj, LV_OPA_COVER, 0);
                        printf("✓ Image loaded and displayed: %s\n", filepath);
                    }
                    else
                    {
                        // Image failed to load
                        lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);
                        printf("✗ Image failed to load: %s\n", filepath);
                    }
                }
            }
            else
            {
                lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);
            }
        }
    }
    else
    {
        // Video or other files: Disable play button, hide spectrum and covers
        lv_obj_clear_flag(play_obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(play_obj, LV_OPA_40, 0);

        // Hide spectrum and album cover for non-audio
        lv_obj_set_style_opa(spectrum_obj, LV_OPA_TRANSP, 0);
        if (album_image_obj)
        {
            lv_obj_set_style_opa(album_image_obj, LV_OPA_TRANSP, 0);
        }

        // Hide media image for non-image files
        if (media_image_obj)
        {
            lv_obj_set_style_opa(media_image_obj, LV_OPA_TRANSP, 0);
        }
    }

    // Return early for non-audio files to skip complex animations
    if (current_media_type != MEDIA_TYPE_AUDIO)
    {
        return;
    }

    // Album transition animations for audio files only
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_values(&a, lv_obj_get_style_image_opa(album_image_obj, LV_PART_MAIN), LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, album_fade_anim_cb);
    lv_anim_set_duration(&a, 500);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
#if LV_DEMO_PLAYER_LANDSCAPE
    if (next)
    {
        lv_anim_set_values(&a, 0, -LV_HOR_RES / 7);
    }
    else
    {
        lv_anim_set_values(&a, 0, LV_HOR_RES / 7);
    }
#else
    if (next)
    {
        lv_anim_set_values(&a, 0, -LV_HOR_RES / 2);
    }
    else
    {
        lv_anim_set_values(&a, 0, LV_HOR_RES / 2);
    }
#endif
    lv_anim_set_exec_cb(&a, _obj_set_x_anim_cb);
    lv_anim_set_completed_cb(&a, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&a);

    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_values(&a, LV_SCALE_NONE, LV_SCALE_NONE / 2);
    lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);

    album_image_obj = album_image_create(spectrum_obj);

    // Only continue with animations if we have an album image (audio files only)
    if (!album_image_obj)
    {
        return;
    }

    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_values(&a, LV_SCALE_NONE / 4, LV_SCALE_NONE);
    lv_anim_set_exec_cb(&a, _image_set_scale_anim_cb);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, album_fade_anim_cb);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_delay(&a, 100);
    lv_anim_start(&a);
}

int32_t get_cos(int32_t deg, int32_t a)
{
    int32_t r = (lv_trigo_cos(deg) * a);

    r += LV_TRIGO_SIN_MAX / 2;
    return r >> LV_TRIGO_SHIFT;
}

int32_t get_sin(int32_t deg, int32_t a)
{
    int32_t r = lv_trigo_sin(deg) * a;

    return (r + LV_TRIGO_SIN_MAX / 2) >> LV_TRIGO_SHIFT;
}

static void cover_spectrum_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    // Only switch if we're playing an audio track
    if (playing && lv_demo_player_is_audio_track(track_id))
    {
        switch_cover_spectrum_display();
    }
}

static void switch_cover_spectrum_display(void)
{
    lv_anim_t fade_out, fade_in;

    if (show_cover)
    {
        // Currently showing cover, switch to spectrum

        // Fade out cover
        lv_anim_init(&fade_out);
        lv_anim_set_var(&fade_out, album_image_obj);
        lv_anim_set_values(&fade_out, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&fade_out, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_duration(&fade_out, 500);
        lv_anim_start(&fade_out);

        // Fade in spectrum
        lv_anim_init(&fade_in);
        lv_anim_set_var(&fade_in, spectrum_obj);
        lv_anim_set_values(&fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&fade_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_duration(&fade_in, 500);
        lv_anim_set_delay(&fade_in, 250);
        lv_anim_start(&fade_in);

        show_cover = false;
    }
    else
    {
        // Currently showing spectrum, switch to cover

        // Fade out spectrum
        lv_anim_init(&fade_out);
        lv_anim_set_var(&fade_out, spectrum_obj);
        lv_anim_set_values(&fade_out, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&fade_out, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_duration(&fade_out, 500);
        lv_anim_start(&fade_out);

        // Fade in cover
        lv_anim_init(&fade_in);
        lv_anim_set_var(&fade_in, album_image_obj);
        lv_anim_set_values(&fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&fade_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_duration(&fade_in, 500);
        lv_anim_set_delay(&fade_in, 250);
        lv_anim_start(&fade_in);

        show_cover = true;
    }
}

static void setup_fullsize_image_display(void)
{
    // For image files, we want to maximize the display area
    // Set album image to fill the container optimally
    if (album_image_obj)
    {
        // Make the image fill more of the available space
        lv_obj_set_width(album_image_obj, lv_pct(90));  // 90% of container width
        lv_obj_set_height(album_image_obj, lv_pct(90)); // 90% of container height

        // Center the image in the container
        lv_obj_center(album_image_obj);

        // Ensure the image scales properly to fit
        lv_obj_set_style_img_opa(album_image_obj, LV_OPA_COVER, 0);

        // Set image to cover mode for optimal display
        lv_image_set_scale(album_image_obj, LV_SCALE_NONE);
    }
}

static void cover_cycle_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    // Only cycle covers for audio tracks when cover is being shown
    if (playing && lv_demo_player_is_audio_track(track_id) && show_cover)
    {
        current_cover_index = (current_cover_index + 1) % 3;
        update_album_cover_for_audio();
    }
}

static void update_album_cover_for_audio(void)
{
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_1);
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_2);
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_3);

    if (!album_image_obj)
        return;

    // Update the cover image based on current_cover_index
    switch (current_cover_index % 3)
    {
    case 0:
        lv_image_set_src(album_image_obj, &img_lv_demo_player_cover_1);
        spectrum = spectrum_1;
        spectrum_len = sizeof(spectrum_1) / sizeof(spectrum_1[0]);
        break;
    case 1:
        lv_image_set_src(album_image_obj, &img_lv_demo_player_cover_2);
        spectrum = spectrum_2;
        spectrum_len = sizeof(spectrum_2) / sizeof(spectrum_2[0]);
        break;
    case 2:
        lv_image_set_src(album_image_obj, &img_lv_demo_player_cover_3);
        spectrum = spectrum_3;
        spectrum_len = sizeof(spectrum_3) / sizeof(spectrum_3[0]);
        break;
    }

    // Add a subtle fade effect when changing covers
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, album_image_obj);
    lv_anim_set_values(&a, LV_OPA_60, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, album_fade_anim_cb);
    lv_anim_set_duration(&a, 300);
    lv_anim_start(&a);
}

static void delayed_track_load_cb(lv_timer_t *timer)
{
    // Load the first track after UI is fully initialized
    track_load(0);

    // Delete the timer as it's no longer needed
    lv_timer_delete(timer);
}

static void del_counter_timer_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE && sec_counter_timer)
    {
        lv_timer_delete(sec_counter_timer);
        sec_counter_timer = NULL;
    }
}

static void spectrum_draw_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE)
    {
#if LV_DEMO_PLAYER_LANDSCAPE
        lv_event_set_ext_draw_size(e, LV_HOR_RES);
#else
        lv_event_set_ext_draw_size(e, LV_VER_RES);
#endif
    }
    else if (code == LV_EVENT_COVER_CHECK)
    {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
    }
    else if (code == LV_EVENT_DRAW_MAIN_BEGIN)
    {
        // Don't draw spectrum for non-audio files
        media_type_t current_media_type = get_media_type_by_id(track_id);
        if (current_media_type != MEDIA_TYPE_AUDIO)
        {
            return;
        }

        lv_obj_t *obj = lv_event_get_target(e);
        lv_layer_t *layer = lv_event_get_layer(e);

        lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);
        if (opa <= LV_OPA_MIN)
            return;

        lv_point_t center;
        lv_area_t obj_coords;
        lv_obj_get_coords(obj, &obj_coords);
        center.x = obj_coords.x1 + lv_obj_get_width(obj) / 2;
        center.y = obj_coords.y1 + lv_obj_get_height(obj) / 2;

        lv_draw_triangle_dsc_t draw_dsc;
        lv_draw_triangle_dsc_init(&draw_dsc);
        draw_dsc.opa = LV_OPA_COVER;

        uint16_t r[64];
        uint32_t i;

        for (i = 0; i < BAR_CNT; i++)
        {
            r[i] = BAR_REST_RADIUS;
        }

        uint32_t s;
        for (s = 0; s < 4; s++)
        {
            uint32_t f;
            uint32_t band_w = 0; /*Real number of bars in this band.*/
            switch (s)
            {
            case 0:
                band_w = 20;
                break;
            case 1:
                band_w = 8;
                break;
            case 2:
                band_w = 4;
                break;
            case 3:
                band_w = 2;
                break;
            }

            /* Add "side bars" with cosine characteristic.*/
            for (f = 0; f < band_w; f++)
            {
                uint32_t ampl_main = spectrum[spectrum_i][s];
                int32_t ampl_mod = get_cos(f * 360 / band_w + 180, 180) + 180;
                int32_t t = BAR_PER_BAND_CNT * s - band_w / 2 + f;
                if (t < 0)
                    t = BAR_CNT + t;
                if (t >= BAR_CNT)
                    t = t - BAR_CNT;
                r[t] += (ampl_main * ampl_mod) >> 9;
            }
        }

        const int32_t amax = 20;
        int32_t animv = spectrum_i - 0;
        if (animv > amax)
            animv = amax;
        for (i = 0; i < BAR_CNT; i++)
        {
            uint32_t deg_space = 1;
            uint32_t deg = i * DEG_STEP + 90;
            uint32_t j = (i + bar_rot + rnd_array[bar_ofs % 10]) % BAR_CNT;
            uint32_t k = (i + bar_rot + rnd_array[(bar_ofs + 1) % 10]) % BAR_CNT;

            uint32_t v;
            if (start_anim)
            {
                v = BAR_REST_RADIUS + start_anim_values[i];
            }
            else
            {
                v = (r[k] * animv + r[j] * (amax - animv)) / amax;
            }

            if (v < BAR_COLOR1_STOP)
                draw_dsc.color = BAR_COLOR1;
            else if (v > (uint32_t)BAR_COLOR3_STOP)
                draw_dsc.color = BAR_COLOR3;
            else if (v > BAR_COLOR2_STOP)
                draw_dsc.color = lv_color_mix(BAR_COLOR3, BAR_COLOR2,
                                              ((v - BAR_COLOR2_STOP) * 255) / (BAR_COLOR3_STOP - BAR_COLOR2_STOP));
            else
                draw_dsc.color = lv_color_mix(BAR_COLOR2, BAR_COLOR1,
                                              ((v - BAR_COLOR1_STOP) * 255) / (BAR_COLOR2_STOP - BAR_COLOR1_STOP));

            uint32_t di = deg + deg_space;

            int32_t x1_out = get_cos(di, v);
            draw_dsc.p[0].x = center.x + x1_out;
            draw_dsc.p[0].y = center.y + get_sin(di, v);

            di += DEG_STEP - deg_space * 2;

            int32_t x2_out = get_cos(di, v);
            draw_dsc.p[1].x = center.x + x2_out;
            draw_dsc.p[1].y = center.y + get_sin(di, v);

            int32_t x2_in = get_cos(di, 0);
            draw_dsc.p[2].x = center.x + x2_in;
            draw_dsc.p[2].y = center.y + get_sin(di, 0);
            lv_draw_triangle(layer, &draw_dsc);

            draw_dsc.p[0].x = center.x - x1_out;
            draw_dsc.p[1].x = center.x - x2_out;
            draw_dsc.p[2].x = center.x - x2_in;
            lv_draw_triangle(layer, &draw_dsc);
        }
    }
    else if (code == LV_EVENT_DELETE)
    {
        lv_anim_delete(NULL, start_anim_cb);
        lv_anim_delete(NULL, spectrum_anim_cb);
        if (start_anim && stop_start_anim_timer)
            lv_timer_delete(stop_start_anim_timer);
    }
}

static void spectrum_anim_cb(void *a, int32_t v)
{
    lv_obj_t *obj = a;
    if (start_anim)
    {
        lv_obj_invalidate(obj);
        return;
    }

    spectrum_i = v;
    lv_obj_invalidate(obj);

    static uint32_t bass_cnt = 0;
    static int32_t last_bass = -1000;
    static int32_t dir = 1;
    if (spectrum[spectrum_i][0] > 12)
    {
        if (spectrum_i - last_bass > 5)
        {
            bass_cnt++;
            last_bass = spectrum_i;
            if (bass_cnt >= 2)
            {
                bass_cnt = 0;
                bar_ofs++;
            }
        }
    }
    if (spectrum[spectrum_i][0] < 4)
        bar_rot += dir;

    if (album_image_obj)
    {
        lv_image_set_scale(album_image_obj, LV_SCALE_NONE + spectrum[spectrum_i][0]);
    }
}

static void start_anim_cb(void *var, int32_t v)
{
    int32_t *av = var;
    *av = v;
    lv_obj_invalidate(spectrum_obj);
}

static lv_obj_t *album_image_create(lv_obj_t *parent)
{
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_1);
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_2);
    LV_IMAGE_DECLARE(img_lv_demo_player_cover_3);

    lv_obj_t *img;
    img = lv_image_create(parent);

    // Use simple logic based on track_id for now
    switch (track_id % 3)
    {
    case 0:
        lv_image_set_src(img, &img_lv_demo_player_cover_1);
        spectrum = spectrum_1;
        spectrum_len = sizeof(spectrum_1) / sizeof(spectrum_1[0]);
        break;
    case 1:
        lv_image_set_src(img, &img_lv_demo_player_cover_2);
        spectrum = spectrum_2;
        spectrum_len = sizeof(spectrum_2) / sizeof(spectrum_2[0]);
        break;
    case 2:
        lv_image_set_src(img, &img_lv_demo_player_cover_3);
        spectrum = spectrum_3;
        spectrum_len = sizeof(spectrum_3) / sizeof(spectrum_3[0]);
        break;
    }

    lv_image_set_antialias(img, false);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(img, album_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);

    return img;
}

static void album_gesture_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT)
        lv_demo_player_album_next(true);
    if (dir == LV_DIR_RIGHT)
        lv_demo_player_album_next(false);
}

static void play_event_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);

    // Only allow play/pause for audio tracks
    if (!lv_demo_player_is_audio_track(track_id))
    {
        return;
    }

    if (lv_obj_has_state(obj, LV_STATE_CHECKED))
    {
        lv_demo_player_resume();
    }
    else
    {
        lv_demo_player_pause();
    }
}

static void prev_click_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_demo_player_album_next(false);
}

static void next_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_demo_player_album_next(true);
    }
}

static void timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    time_act++;
    lv_label_set_text_fmt(time_obj, "%" LV_PRIu32 ":%02" LV_PRIu32, time_act / 60, time_act % 60);
    lv_slider_set_value(slider_obj, time_act, LV_ANIM_ON);
}

static void spectrum_end_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_demo_player_album_next(true);
}

static void stop_start_anim(lv_timer_t *t)
{
    LV_UNUSED(t);
    start_anim = false;
    lv_obj_refresh_ext_draw_size(spectrum_obj);
}

static void album_fade_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_image_opa(var, v, 0);
}
#endif /*LV_USE_DEMO_PLAYER*/

/**
 * @file lv_demo_player_main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_player_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h> // For PATH_MAX
#include "lvgl.h"

#if LV_USE_FFMPEG
#include "../../lvgl/src/libs/ffmpeg/lv_ffmpeg.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define MEDIA_PATH "/opt/media"
#define THUMBNAIL_SIZE 120
#define GRID_COLS 4
#define GRID_ROWS 3
#define MAX_MEDIA_FILES 100

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
    char *filename;
    char *filepath;
    bool is_video;
    bool is_gif;
} media_file_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t *create_media_grid(lv_obj_t *parent);
static void scan_media_files(void);
static void create_media_thumbnail(lv_obj_t *parent, media_file_t *media, int index);
static void media_click_event_cb(lv_event_t *e);
static void fullscreen_click_event_cb(lv_event_t *e);
static void show_fullscreen_media(const char *filepath, bool is_video);
static bool is_gif_file(const char *filename);
static void hide_fullscreen_media(void);
static bool is_video_file(const char *filename);
static bool is_image_file(const char *filename);

/**********************
 *  STATIC VARIABLES
 **********************/
static media_file_t media_files[MAX_MEDIA_FILES];
static int media_count = 0;
static lv_obj_t *fullscreen_container = NULL;
static lv_obj_t *fullscreen_image = NULL;
static lv_obj_t *fullscreen_video = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t *lv_demo_player_main_create(lv_obj_t *parent)
{
    LV_LOG_USER("Starting media player demo");

    // Test filesystem
    lv_fs_res_t res = lv_fs_is_ready('P');
    LV_LOG_USER("POSIX filesystem ready: %d", res);

    // Test opening a file
    lv_fs_file_t f;
    res = lv_fs_open(&f, "P:/opt/media/test_200x200.jpg", LV_FS_MODE_RD);
    if (res == LV_FS_RES_OK)
    {
        LV_LOG_USER("File opened successfully");
        lv_fs_close(&f);
    }
    else
    {
        LV_LOG_USER("Failed to open file: %d", res);
    }

    // Scan media files
    scan_media_files();

    // Create main container
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(cont, 20, 0);

    // Create title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "媒体播放器");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create media grid
    lv_obj_t *grid = create_media_grid(cont);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);

    return cont;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t *create_media_grid(lv_obj_t *parent)
{
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, lv_pct(90), lv_pct(80));
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_pad_all(grid, 10, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    // Set up grid layout
    static lv_coord_t col_dsc[GRID_COLS + 1];
    static lv_coord_t row_dsc[GRID_ROWS + 1];

    for (int i = 0; i < GRID_COLS; i++)
    {
        col_dsc[i] = THUMBNAIL_SIZE + 20;
    }
    col_dsc[GRID_COLS] = LV_GRID_TEMPLATE_LAST;

    for (int i = 0; i < GRID_ROWS; i++)
    {
        row_dsc[i] = THUMBNAIL_SIZE + 40;
    }
    row_dsc[GRID_ROWS] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    // Create thumbnails for media files
    for (int i = 0; i < media_count && i < GRID_COLS * GRID_ROWS; i++)
    {
        create_media_thumbnail(grid, &media_files[i], i);
    }

    return grid;
}

static void scan_media_files(void)
{
    DIR *dir;
    struct dirent *ent;
    char filepath[512];

    media_count = 0;

    if ((dir = opendir(MEDIA_PATH)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL && media_count < MAX_MEDIA_FILES)
        {
            if (ent->d_name[0] == '.')
                continue; // Skip hidden files

            if (is_video_file(ent->d_name) || is_image_file(ent->d_name))
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", MEDIA_PATH, ent->d_name);

                media_files[media_count].filename = strdup(ent->d_name);
                media_files[media_count].filepath = strdup(filepath);
                media_files[media_count].is_video = is_video_file(ent->d_name);
                media_files[media_count].is_gif = is_gif_file(ent->d_name);
                media_count++;
            }
        }
        closedir(dir);
    }
}

static void create_media_thumbnail(lv_obj_t *parent, media_file_t *media, int index)
{
    lv_obj_t *thumb_cont = lv_obj_create(parent);
    lv_obj_set_size(thumb_cont, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
    lv_obj_set_style_bg_color(thumb_cont, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(thumb_cont, 1, 0);
    lv_obj_set_style_border_color(thumb_cont, lv_color_hex(0x555555), 0);
    lv_obj_set_style_pad_all(thumb_cont, 5, 0);

    int col = index % GRID_COLS;
    int row = index / GRID_COLS;
    lv_obj_set_grid_cell(thumb_cont, LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    // Create image or video thumbnail
    lv_obj_t *img = lv_image_create(thumb_cont);
    lv_obj_center(img);

    if (media->is_video)
    {
        // For video files, try to load first frame or use a video icon
        lv_image_set_src(img, LV_SYMBOL_VIDEO);
        lv_obj_set_style_text_color(img, lv_color_white(), 0);
        lv_obj_set_style_text_font(img, &lv_font_source_han_sans_sc_16_cjk, 0);
    }
    else
    {
        // For all image formats including GIF, load the actual image
        char prefixed_path[PATH_MAX + 3];                                      // +3 for "P:" and null terminator
        snprintf(prefixed_path, sizeof(prefixed_path), "%s", media->filepath); // Try without drive letter

        if (media->is_gif)
        {
            // Use lv_gif for GIF files
            LV_LOG_USER("Loading GIF: %s", prefixed_path);
            lv_gif_set_src(img, prefixed_path);
        }
        else
        {
            // Use lv_image for other formats
            LV_LOG_USER("Loading image: %s", prefixed_path);
            lv_image_set_src(img, prefixed_path);
            // Set to maintain aspect ratio and fully fill container
            lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
        }
        lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
    }

    // Create filename label
    lv_obj_t *label = lv_label_create(thumb_cont);
    char short_name[32];
    strncpy(short_name, media->filename, 31);
    short_name[31] = '\0';

    // Truncate long filenames
    if (strlen(short_name) > 15)
    {
        short_name[12] = '.';
        short_name[13] = '.';
        short_name[14] = '.';
        short_name[15] = '\0';
    }

    lv_label_set_text(label, short_name);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Store media info in user data
    lv_obj_set_user_data(thumb_cont, media);

    // Add click event
    lv_obj_add_event_cb(thumb_cont, media_click_event_cb, LV_EVENT_CLICKED, NULL);
}

static void media_click_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    media_file_t *media = (media_file_t *)lv_obj_get_user_data(obj);

    if (media)
    {
        show_fullscreen_media(media->filepath, media->is_video);
    }
}

static void show_fullscreen_media(const char *filepath, bool is_video)
{
    // Create fullscreen container
    fullscreen_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(fullscreen_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(fullscreen_container, lv_color_black(), 0);
    lv_obj_set_style_pad_all(fullscreen_container, 0, 0);
    lv_obj_set_style_border_width(fullscreen_container, 0, 0);
    lv_obj_add_event_cb(fullscreen_container, fullscreen_click_event_cb, LV_EVENT_CLICKED, NULL);

    // Check if it's a GIF file
    const char *ext = strrchr(filepath, '.');
    bool is_gif = (ext && strcasecmp(ext, ".gif") == 0);

    if (is_video)
    {
#if LV_USE_FFMPEG
        // Create FFmpeg video player
        fullscreen_video = lv_ffmpeg_player_create(fullscreen_container);
        lv_ffmpeg_player_set_src(fullscreen_video, filepath);
        lv_obj_center(fullscreen_video);

        // Auto-resize to fit screen while maintaining aspect ratio
        lv_ffmpeg_player_set_auto_restart(fullscreen_video, true);
#endif
    }
    else
    {
        if (is_gif)
        {
            // For GIF files, show a message since GIF is not supported
            lv_obj_t *gif_message = lv_label_create(fullscreen_container);
            lv_label_set_text(gif_message, "GIF 文件不支持\n\n不支持的格式");
            lv_obj_set_style_text_color(gif_message, lv_color_white(), 0);
            lv_obj_set_style_text_font(gif_message, &lv_font_source_han_sans_sc_16_cjk, 0);
            lv_obj_set_style_text_align(gif_message, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(gif_message);
        }
        else
        {
            // Create image viewer
            fullscreen_image = lv_image_create(fullscreen_container);
            char prefixed_path[PATH_MAX + 3]; // +3 for "P:" and null terminator
            snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", filepath);
            lv_image_set_src(fullscreen_image, prefixed_path);
            lv_obj_center(fullscreen_image);

            // Set size mode to maintain aspect ratio
            lv_image_set_inner_align(fullscreen_image, LV_IMAGE_ALIGN_CONTAIN);

            // Scale to fit screen
            lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
            lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
            lv_coord_t img_w = lv_image_get_src_width(fullscreen_image);
            lv_coord_t img_h = lv_image_get_src_height(fullscreen_image);

            if (img_w > 0 && img_h > 0)
            {
                float scale_w = (float)screen_w / img_w;
                float scale_h = (float)screen_h / img_h;
                float scale = LV_MIN(scale_w, scale_h);

                lv_image_set_scale(fullscreen_image, (int)(scale * 256));
            }
        }
    }

    // Bring to front
    lv_obj_move_foreground(fullscreen_container);
}

static void fullscreen_click_event_cb(lv_event_t *e)
{
    hide_fullscreen_media();
}

static void hide_fullscreen_media(void)
{
    if (fullscreen_video)
    {
        lv_obj_delete(fullscreen_video);
        fullscreen_video = NULL;
    }

    if (fullscreen_image)
    {
        lv_obj_delete(fullscreen_image);
        fullscreen_image = NULL;
    }

    if (fullscreen_container)
    {
        lv_obj_delete(fullscreen_container);
        fullscreen_container = NULL;
    }
}

static bool is_video_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;

    return (strcasecmp(ext, ".mp4") == 0 ||
            strcasecmp(ext, ".avi") == 0 ||
            strcasecmp(ext, ".mkv") == 0 ||
            strcasecmp(ext, ".mov") == 0 ||
            strcasecmp(ext, ".wmv") == 0);
}

static bool is_image_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;

    return (strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 ||
            strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".bmp") == 0 ||
            strcasecmp(ext, ".gif") == 0);
}

static bool is_gif_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;

    return strcasecmp(ext, ".gif") == 0;
}

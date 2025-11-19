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
#define MAX_GIF_SIZE (256 * 1024) // 256KB limit for GIF files

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
static void cleanup_media_files(void);
static bool is_video_file(const char *filename);
static bool is_image_file(const char *filename);
static void load_custom_font(void);
static void scale_image_to_fit(lv_obj_t * img, int32_t target_w, int32_t target_h);

/**********************
 *  STATIC VARIABLES
 **********************/
static media_file_t media_files[MAX_MEDIA_FILES];
static int media_count = 0;
static lv_obj_t *fullscreen_container = NULL;
static lv_obj_t *fullscreen_image = NULL;
static lv_obj_t *fullscreen_video = NULL;
static lv_font_t *font_cjk = NULL;

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

    // Load custom font
    load_custom_font();

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

            bool is_video = is_video_file(ent->d_name);
            bool is_image = is_image_file(ent->d_name);
            
            if (is_video) {
                LV_LOG_USER("Found video file: %s", ent->d_name);
            }
            
            if (is_video || is_image)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", MEDIA_PATH, ent->d_name);

                // Check file size for GIF files
                if (is_gif_file(ent->d_name))
                {
                    struct stat st;
                    if (stat(filepath, &st) == 0 && st.st_size > MAX_GIF_SIZE)
                    {
                        LV_LOG_WARN("Skipping large GIF file: %s (%lld bytes)", ent->d_name, (long long)st.st_size);
                        continue; // Skip large GIF files
                    }
                }

                media_files[media_count].filename = strdup(ent->d_name);
                media_files[media_count].filepath = strdup(filepath);
                
                if (media_files[media_count].filename == NULL || media_files[media_count].filepath == NULL) {
                    LV_LOG_ERROR("Failed to allocate memory for media file: %s", ent->d_name);
                    // Free any allocated memory
                    if (media_files[media_count].filename) free(media_files[media_count].filename);
                    if (media_files[media_count].filepath) free(media_files[media_count].filepath);
                    continue; // Skip this file
                }
                
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
    lv_obj_t *img;
    
    if (media->is_gif)
    {
        // Use lv_gif for GIF files
        img = lv_gif_create(thumb_cont);
    }
    else
    {
        // Use lv_image for other formats
        img = lv_image_create(thumb_cont);
    }
    
    if (img == NULL) {
        LV_LOG_ERROR("Failed to create image widget for %s", media->filename);
        return;
    }
    
    lv_obj_center(img);

    if (media->is_video)
    {
        LV_LOG_USER("Processing video thumbnail: %s", media->filename);
#if LV_USE_FFMPEG
        // Use FFmpeg player to display video thumbnail
        lv_obj_delete(img); // Remove the placeholder image
        lv_obj_t *player = lv_ffmpeg_player_create(thumb_cont);
        if (player == NULL) {
            LV_LOG_ERROR("Failed to create ffmpeg player for video %s", media->filename);
            // fallback to icon
            img = lv_image_create(thumb_cont);
            lv_image_set_src(img, LV_SYMBOL_VIDEO);
            lv_obj_set_style_text_color(img, lv_color_white(), 0);
            lv_obj_set_style_text_font(img, &lv_font_source_han_sans_sc_16_cjk, 0);
            lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            lv_obj_center(img);
        } else {
            LV_LOG_USER("Setting video source: %s", media->filepath);
            lv_result_t rr = lv_ffmpeg_player_set_src(player, media->filepath);
            if (rr != LV_RESULT_OK) {
                LV_LOG_ERROR("ffmpeg failed to open video %s (result: %d)", media->filepath, rr);
                lv_obj_delete(player);
                // fallback to icon
                img = lv_image_create(thumb_cont);
                lv_image_set_src(img, LV_SYMBOL_VIDEO);
                lv_obj_set_style_text_color(img, lv_color_white(), 0);
                lv_obj_set_style_text_font(img, &lv_font_source_han_sans_sc_16_cjk, 0);
                lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                lv_obj_center(img);
            } else {
                LV_LOG_USER("Video source set successfully, starting playback for thumbnail");
                // Size player to thumbnail
                lv_obj_set_size(player, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                lv_obj_center(player);
                // Try to use CONTAIN alignment for video player
                lv_image_set_inner_align(player, LV_IMAGE_ALIGN_CONTAIN);
                
                // Start playing to show video content (will show frames)
                lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_START);
                // Mute audio for thumbnail
                // lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_VOLUME_MUTE);
                img = player; // Store for later reference
            }
        }
#else
        // No FFmpeg: use static icon
        lv_image_set_src(img, LV_SYMBOL_VIDEO);
        lv_obj_set_style_text_color(img, lv_color_white(), 0);
        lv_obj_set_style_text_font(img, font_cjk, 0);
        lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
#endif
    }
    else
    {
        // For all image formats including GIF, load the actual image
        char prefixed_path[PATH_MAX + 3]; // +3 for "P:" and null terminator
        snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", media->filepath);
        
        if (media->is_gif)
        {
            // Try to use lv_gif for GIF files first
            LV_LOG_USER("Loading GIF (lv_gif): %s", prefixed_path);
            lv_gif_set_color_format(img, LV_COLOR_FORMAT_ARGB8888);
            lv_gif_set_src(img, prefixed_path);

            // If GIF decoder failed or not loaded, fall back to ffmpeg player (if available)
            if (!lv_gif_is_loaded(img)) {
                LV_LOG_WARN("lv_gif couldn't load %s, falling back to FFmpeg if available", prefixed_path);
                lv_obj_delete(img);
                img = NULL;

#if LV_USE_FFMPEG
                // Create a small ffmpeg player to act as thumbnail for GIF
                lv_obj_t *player = lv_ffmpeg_player_create(thumb_cont);
                if (player == NULL) {
                    LV_LOG_ERROR("Failed to create ffmpeg player for GIF %s", media->filename);
                    // fallback to placeholder
                    img = lv_image_create(thumb_cont);
                    lv_image_set_src(img, LV_SYMBOL_IMAGE);
                    lv_obj_set_style_text_color(img, lv_color_white(), 0);
                    lv_obj_set_style_text_font(img, font_cjk, 0);
                    lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                    lv_obj_center(img);
                } else {
                    // Pass the filesystem path (ffmpeg uses POSIX file paths)
                    lv_result_t rr = lv_ffmpeg_player_set_src(player, media->filepath);
                    if (rr != LV_RESULT_OK) {
                        LV_LOG_ERROR("ffmpeg failed to open GIF %s", media->filepath);
                        lv_obj_delete(player);
                        // fallback to placeholder
                        img = lv_image_create(thumb_cont);
                        lv_image_set_src(img, LV_SYMBOL_IMAGE);
                        lv_obj_set_style_text_color(img, lv_color_white(), 0);
                        lv_obj_set_style_text_font(img, font_cjk, 0);
                        lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                        lv_obj_center(img);
                    } else {
                        // Size the player to thumbnail and start playback
                        lv_obj_set_size(player, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                        lv_obj_center(player);
                        lv_image_set_inner_align(player, LV_IMAGE_ALIGN_CONTAIN);
                        lv_ffmpeg_player_set_auto_restart(player, true);
                        lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_START);
                        // store the player as the img variable so later code that expects 'img' can continue
                        img = player;
                    }
                }
#else
                // No ffmpeg available: fallback to static placeholder
                img = lv_image_create(thumb_cont);
                lv_image_set_src(img, LV_SYMBOL_IMAGE);
                lv_obj_set_style_text_color(img, lv_color_white(), 0);
                lv_obj_set_style_text_font(img, font_cjk, 0);
                lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                lv_obj_center(img);
#endif
            } else {
                // GIF loaded successfully
                scale_image_to_fit(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            }
        }
        else
        {
            // Use lv_image for other formats
            LV_LOG_USER("Loading image: %s", prefixed_path);
            lv_image_set_src(img, prefixed_path);
            
            // Check if image loaded successfully by checking source dimensions
            int32_t src_width = lv_image_get_src_width(img);
            int32_t src_height = lv_image_get_src_height(img);
            if (src_width <= 0 || src_height <= 0) {
                LV_LOG_ERROR("Failed to load image %s (dimensions: %dx%d), using placeholder", prefixed_path, src_width, src_height);
                // Replace with placeholder
                lv_image_set_src(img, LV_SYMBOL_IMAGE);
                lv_obj_set_style_text_color(img, lv_color_white(), 0);
                lv_obj_set_style_text_font(img, font_cjk, 0);
            } else {
                // Use manual scaling to ensure it fits
                scale_image_to_fit(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            }
        }
    }

    // Ensure the image/player does not steal click events from the container
    if (img) {
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }

    // Create filename label
    lv_obj_t *label = lv_label_create(thumb_cont);
    if (label == NULL) {
        LV_LOG_ERROR("Failed to create label for %s", media->filename);
        // Continue without label rather than crashing
        return;
    }

    // Use CJK font for label
    lv_obj_set_style_text_font(label, font_cjk, 0);

    // Truncate filename if too long for display
    char short_name[64]; // Increase buffer size
    size_t filename_len = strlen(media->filename);
    if (filename_len >= sizeof(short_name)) {
        // Truncate very long filenames
        strncpy(short_name, media->filename, sizeof(short_name) - 4);
        short_name[sizeof(short_name) - 4] = '\0';
        strcat(short_name, "...");
    } else {
        strcpy(short_name, media->filename);
    }

    // Truncate long filenames for display
    if (strlen(short_name) > 15)
    {
        short_name[12] = '.';
        short_name[13] = '.';
        short_name[14] = '.';
        short_name[15] = '\0';
    }

    lv_label_set_text(label, media->filename);
    lv_obj_set_width(label, THUMBNAIL_SIZE - 10);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);

    // Store media info in user data for click event
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
        if (fullscreen_video == NULL) {
            LV_LOG_ERROR("Failed to create ffmpeg player for %s", filepath);
        } else {
            lv_result_t r = lv_ffmpeg_player_set_src(fullscreen_video, filepath);
            if (r != LV_RESULT_OK) {
                LV_LOG_ERROR("lv_ffmpeg_player_set_src failed for %s (result: %d)", filepath, r);
                lv_obj_delete(fullscreen_video);
                fullscreen_video = NULL;
            } else {
                LV_LOG_USER("Fullscreen video source set, starting playback");
                // Set player size to container to let the player render into this area.
                lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
                lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
                lv_obj_set_size(fullscreen_video, screen_w, screen_h);
                lv_obj_center(fullscreen_video);
                // Try CONTAIN for video
                lv_image_set_inner_align(fullscreen_video, LV_IMAGE_ALIGN_CONTAIN);
                // Auto-restart so looped playback continues
                lv_ffmpeg_player_set_auto_restart(fullscreen_video, true);
                // Start playback
                lv_ffmpeg_player_set_cmd(fullscreen_video, LV_FFMPEG_PLAYER_CMD_START);
            }
        }
#endif
    }
    else
    {
        // Create image/GIF viewer for all formats
        char prefixed_path[PATH_MAX + 3]; // +3 for "P:" and null terminator
        snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", filepath);
        
        if (is_gif)
        {
            // Try lv_gif first for fullscreen
            fullscreen_image = lv_gif_create(fullscreen_container);
            if (fullscreen_image == NULL) {
                LV_LOG_ERROR("Failed to create fullscreen GIF widget for %s", filepath);
            } else {
                lv_gif_set_color_format(fullscreen_image, LV_COLOR_FORMAT_ARGB8888);
                lv_gif_set_src(fullscreen_image, prefixed_path);
                if (!lv_gif_is_loaded(fullscreen_image)) {
                    LV_LOG_WARN("lv_gif couldn't load fullscreen GIF %s, falling back to FFmpeg", prefixed_path);
                    lv_obj_delete(fullscreen_image);
                    fullscreen_image = NULL;
                }
            }

            if (fullscreen_image == NULL) {
#if LV_USE_FFMPEG
                // Use ffmpeg player to play the GIF as a video for fullscreen
                fullscreen_video = lv_ffmpeg_player_create(fullscreen_container);
                if (fullscreen_video == NULL) {
                    LV_LOG_ERROR("Failed to create ffmpeg player for fullscreen GIF %s", filepath);
                } else {
                    lv_result_t r = lv_ffmpeg_player_set_src(fullscreen_video, filepath);
                    if (r != LV_RESULT_OK) {
                        LV_LOG_ERROR("lv_ffmpeg_player_set_src failed for GIF %s (result: %d)", filepath, r);
                        lv_obj_delete(fullscreen_video);
                        fullscreen_video = NULL;
                    } else {
                        lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
                        lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
                        lv_obj_set_size(fullscreen_video, screen_w, screen_h);
                        lv_obj_center(fullscreen_video);
                        lv_image_set_inner_align(fullscreen_video, LV_IMAGE_ALIGN_CONTAIN);
                        lv_ffmpeg_player_set_auto_restart(fullscreen_video, true);
                        lv_ffmpeg_player_set_cmd(fullscreen_video, LV_FFMPEG_PLAYER_CMD_START);
                    }
                }
#endif
            } else {
                // lv_gif succeeded: size it to screen
                lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
                lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
                scale_image_to_fit(fullscreen_image, screen_w, screen_h);
            }
        }
        else
        {
            // Create regular image viewer
            fullscreen_image = lv_image_create(fullscreen_container);
            if (fullscreen_image == NULL) {
                LV_LOG_ERROR("Failed to create fullscreen image widget for %s", filepath);
                return;
            }
            lv_image_set_src(fullscreen_image, prefixed_path);
            
            // Check if image loaded successfully
            int32_t src_width = lv_image_get_src_width(fullscreen_image);
            int32_t src_height = lv_image_get_src_height(fullscreen_image);
            LV_LOG_USER("Fullscreen image %s dimensions: %dx%d", filepath, src_width, src_height);
            if (src_width <= 0 || src_height <= 0) {
                LV_LOG_ERROR("Failed to load fullscreen image %s (dimensions: %dx%d)", prefixed_path, src_width, src_height);
                lv_obj_delete(fullscreen_image);
                fullscreen_image = NULL;
                return;
            }
            
            // Get screen dimensions
            lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
            lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
            
            // Use manual scaling to ensure it fits
            scale_image_to_fit(fullscreen_image, screen_w, screen_h);
        }
    }

    // Bring to front
    lv_obj_move_foreground(fullscreen_container);
}

static void fullscreen_click_event_cb(lv_event_t *e)
{
    hide_fullscreen_media();
}

static void cleanup_media_files(void)
{
    for (int i = 0; i < media_count; i++) {
        if (media_files[i].filename) {
            free(media_files[i].filename);
            media_files[i].filename = NULL;
        }
        if (media_files[i].filepath) {
            free(media_files[i].filepath);
            media_files[i].filepath = NULL;
        }
    }
    media_count = 0;
}

static void load_custom_font(void)
{
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
    // Try to load a system font that supports Traditional Chinese
    // macOS common font path
    const char * font_path = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
    
    // Check if file exists using POSIX fopen (since we use P: drive for LVGL but this is direct check)
    // Actually, lv_tiny_ttf_create_file uses LVGL FS.
    // We need to map it. "P:/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
    
    font_cjk = lv_tiny_ttf_create_file("P:/System/Library/Fonts/Supplemental/Arial Unicode.ttf", 16);
    
    if (font_cjk == NULL) {
        LV_LOG_WARN("Failed to load Arial Unicode.ttf, trying PingFang.ttc");
        // PingFang might not work as it is a collection, but worth a try if supported
        font_cjk = lv_tiny_ttf_create_file("P:/System/Library/Fonts/PingFang.ttc", 16);
    }
    
    if (font_cjk == NULL) {
        LV_LOG_WARN("Failed to load system fonts, falling back to built-in font");
    } else {
        LV_LOG_USER("Successfully loaded custom CJK font");
    }
#else
    LV_LOG_WARN("TinyTTF file support not enabled");
#endif

    if (font_cjk == NULL) {
        // Fallback to built-in SC font
        font_cjk = &lv_font_source_han_sans_sc_16_cjk;
    }
}

static void scale_image_to_fit(lv_obj_t * img, int32_t target_w, int32_t target_h) {
    int32_t src_w = lv_image_get_src_width(img);
    int32_t src_h = lv_image_get_src_height(img);
    
    if (src_w <= 0 || src_h <= 0) return;
    
    // Calculate scale factor (256 = 100%)
    // We want to fit within target_w x target_h while maintaining aspect ratio
    
    int32_t scale_w = (target_w * 256) / src_w;
    int32_t scale_h = (target_h * 256) / src_h;
    
    // Use the smaller scale to ensure it fits both dimensions
    int32_t scale = (scale_w < scale_h) ? scale_w : scale_h;
    
    lv_image_set_scale(img, scale);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
}

static void hide_fullscreen_media(void)
{
    if (fullscreen_container) {
        lv_obj_delete(fullscreen_container);
        fullscreen_container = NULL;
        fullscreen_image = NULL;
        fullscreen_video = NULL;
    }
}

static bool is_video_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".avi") == 0 || 
            strcasecmp(ext, ".mkv") == 0 || strcasecmp(ext, ".mov") == 0);
}

static bool is_image_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || 
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".bmp") == 0 ||
            strcasecmp(ext, ".gif") == 0);
}

static bool is_gif_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".gif") == 0);
}

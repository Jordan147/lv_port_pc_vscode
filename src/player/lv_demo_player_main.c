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
#include <signal.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/wait.h>
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
#define GRID_ROWS 100
#define MAX_MEDIA_FILES 100
#define MAX_GIF_SIZE (20 * 1024 * 1024) // 20MB limit for GIF files

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
    char *filename;
    char *filepath;
    bool is_video;
    bool is_gif;
    bool is_audio;
} media_file_t;

typedef struct
{
    lv_image_dsc_t dsc;
    void *data;
} bitmap_wrapper_t;

static void bitmap_free_cb(lv_event_t *e)
{
    bitmap_wrapper_t *bmp = lv_event_get_user_data(e);
    if (bmp)
    {
        if (bmp->data)
            free(bmp->data);
        free(bmp);
    }
}

static bitmap_wrapper_t *decode_jpg_to_bitmap(const char *path)
{
    lv_image_header_t header;
    if (lv_image_decoder_get_info(path, &header) != LV_RESULT_OK)
    {
        LV_LOG_ERROR("Failed to get info for %s", path);
        return NULL;
    }

    int32_t w = header.w;
    int32_t h = header.h;

    size_t buf_size = w * h * 4;
    void *buf = malloc(buf_size);
    if (!buf)
    {
        LV_LOG_ERROR("OOM for JPG decode");
        return NULL;
    }

    lv_obj_t *canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_buffer(canvas, buf, w, h, LV_COLOR_FORMAT_ARGB8888);

    lv_draw_image_dsc_t draw_dsc;
    lv_draw_image_dsc_init(&draw_dsc);
    draw_dsc.src = path;

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_area_t coords = {0, 0, w - 1, h - 1};
    lv_draw_image(&layer, &draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
    lv_obj_delete(canvas);

    bitmap_wrapper_t *bmp = malloc(sizeof(bitmap_wrapper_t));
    bmp->data = buf;
    memset(&bmp->dsc, 0, sizeof(lv_image_dsc_t));
    bmp->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    bmp->dsc.header.w = w;
    bmp->dsc.header.h = h;
    bmp->dsc.header.stride = w * 4;
    bmp->dsc.data = buf;
    bmp->dsc.data_size = buf_size;

    return bmp;
}

static bitmap_wrapper_t *create_thumbnail_bitmap(bitmap_wrapper_t *src, int32_t target_w, int32_t target_h)
{
    if (!src || !src->data)
        return NULL;

    int32_t src_w = src->dsc.header.w;
    int32_t src_h = src->dsc.header.h;
    uint8_t *src_buf = (uint8_t *)src->data;

    size_t dst_size = target_w * target_h * 4;
    void *dst_buf = malloc(dst_size);
    if (!dst_buf)
        return NULL;

    // Fill with opaque black (ARGB8888: A=FF, R=0, G=0, B=0)
    // In Little Endian 32-bit: 0xFF000000
    uint32_t *dst_pixels = (uint32_t *)dst_buf;
    for (int i = 0; i < target_w * target_h; i++)
    {
        dst_pixels[i] = 0xFF000000;
    }

    // Calculate scale for CONTAIN (Fit inside)
    float scale_x = (float)target_w / src_w;
    float scale_y = (float)target_h / src_h;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    int32_t final_w = (int32_t)(src_w * scale);
    int32_t final_h = (int32_t)(src_h * scale);

    if (final_w < 1)
        final_w = 1;
    if (final_h < 1)
        final_h = 1;

    int32_t off_x = (target_w - final_w) / 2;
    int32_t off_y = (target_h - final_h) / 2;

    // Nearest Neighbor Resizing
    for (int y = 0; y < final_h; y++)
    {
        for (int x = 0; x < final_w; x++)
        {
            // Map to source coordinates
            int32_t src_x = (int32_t)(x / scale);
            int32_t src_y = (int32_t)(y / scale);

            if (src_x >= src_w)
                src_x = src_w - 1;
            if (src_y >= src_h)
                src_y = src_h - 1;

            // Copy pixel
            uint32_t *src_p = (uint32_t *)src_buf + (src_y * src_w + src_x);
            uint32_t *dst_p = dst_pixels + ((off_y + y) * target_w + (off_x + x));

            *dst_p = *src_p;
        }
    }

    bitmap_wrapper_t *bmp = malloc(sizeof(bitmap_wrapper_t));
    bmp->data = dst_buf;
    memset(&bmp->dsc, 0, sizeof(lv_image_dsc_t));
    bmp->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    bmp->dsc.header.w = target_w;
    bmp->dsc.header.h = target_h;
    bmp->dsc.header.stride = target_w * 4;
    bmp->dsc.data = dst_buf;
    bmp->dsc.data_size = dst_size;

    return bmp;
}

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t *create_media_grid(lv_obj_t *parent);
static void scan_media_files(void);
static void create_media_thumbnail(lv_obj_t *parent, media_file_t *media, int index);
static void media_click_event_cb(lv_event_t *e);
static void fullscreen_click_event_cb(lv_event_t *e);
static void show_fullscreen_media(const char *filepath, bool is_video, bool is_audio);
static bool is_gif_file(const char *filename);
static void hide_fullscreen_media(void);
static void cleanup_media_files(void);
static bool is_video_file(const char *filename);
static bool is_image_file(const char *filename);
static bool is_audio_file(const char *filename);
static void load_custom_font(void);
static void scale_image_to_fit(lv_obj_t * img, int32_t target_w, int32_t target_h);
static void play_audio(const char *filepath);
static void stop_audio(void);
static bool is_bmp_file(const char *filename);
static bool is_jpg_file(const char *filename);
static bool is_png_file(const char *filename);

/**********************
 *  STATIC VARIABLES
 **********************/
static media_file_t media_files[MAX_MEDIA_FILES];
static int media_count = 0;
static lv_obj_t *fullscreen_container = NULL;
static lv_obj_t *fullscreen_image = NULL;
static lv_obj_t *fullscreen_video = NULL;
static lv_font_t *font_cjk = NULL;
static pid_t audio_pid = -1;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t *lv_demo_player_main_create(lv_obj_t *parent)
{
    LV_LOG_USER("啟動媒體播放器演示");

    // Test filesystem
    lv_fs_res_t res = lv_fs_is_ready('P');
    LV_LOG_USER("POSIX 文件系統就緒: %d", res);

    // Test opening a file
    lv_fs_file_t f;
    res = lv_fs_open(&f, "P:/opt/media/test_200x200.jpg", LV_FS_MODE_RD);
    if (res == LV_FS_RES_OK)
    {
        LV_LOG_USER("文件打開成功");
        lv_fs_close(&f);
    }
    else
    {
        LV_LOG_USER("無法打開文件: %d", res);
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
    lv_label_set_text(title, "媒體播放器");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, font_cjk, 0);
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

    // Calculate needed rows based on media count
    int needed_rows = (media_count + GRID_COLS - 1) / GRID_COLS;
    if (needed_rows < 1)
        needed_rows = 1;
    if (needed_rows > GRID_ROWS)
        needed_rows = GRID_ROWS;

    for (int i = 0; i < needed_rows; i++)
    {
        row_dsc[i] = THUMBNAIL_SIZE + 40;
    }
    row_dsc[needed_rows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    // Create thumbnails for media files
    for (int i = 0; i < media_count && i < GRID_COLS * needed_rows; i++)
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
            bool is_audio = is_audio_file(ent->d_name);

            if (is_video) {
                LV_LOG_USER("發現視頻文件: %s", ent->d_name);
            }
            if (is_audio)
            {
                LV_LOG_USER("發現音頻文件: %s", ent->d_name);
            }

            if (is_video || is_image || is_audio)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", MEDIA_PATH, ent->d_name);

                // Check file size for GIF files
                if (is_gif_file(ent->d_name))
                {
                    struct stat st;
                    if (stat(filepath, &st) == 0 && st.st_size > MAX_GIF_SIZE)
                    {
                        LV_LOG_WARN("跳過過大的 GIF 文件: %s (%lld bytes)", ent->d_name, (long long)st.st_size);
                        continue; // Skip large GIF files
                    }
                }

                media_files[media_count].filename = strdup(ent->d_name);
                media_files[media_count].filepath = strdup(filepath);

                if (media_files[media_count].filename == NULL || media_files[media_count].filepath == NULL) {
                    LV_LOG_ERROR("無法為媒體文件分配內存: %s", ent->d_name);
                    // Free any allocated memory
                    if (media_files[media_count].filename) free(media_files[media_count].filename);
                    if (media_files[media_count].filepath) free(media_files[media_count].filepath);
                    continue; // Skip this file
                }

                media_files[media_count].is_video = is_video;
                media_files[media_count].is_gif = is_gif_file(ent->d_name);
                media_files[media_count].is_audio = is_audio;
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

    if (media->is_audio)
    {
        // Use icon for audio files
        img = lv_image_create(thumb_cont);
        lv_image_set_src(img, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(img, lv_color_white(), 0);
        // Do not set CJK font for symbols as it might not contain them
        // lv_obj_set_style_text_font(img, font_cjk, 0);
        lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
        lv_obj_center(img);
    }
    else if (media->is_gif)
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
        LV_LOG_ERROR("無法為 %s 創建圖片組件", media->filename);
        return;
    }

    lv_obj_center(img);

    if (media->is_video)
    {
        LV_LOG_USER("處理視頻縮略圖: %s", media->filename);
#if LV_USE_FFMPEG
        // Use FFmpeg player to display video thumbnail
        lv_obj_delete(img); // Remove the placeholder image
        lv_obj_t *player = lv_ffmpeg_player_create(thumb_cont);
        if (player == NULL) {
            LV_LOG_ERROR("無法為視頻 %s 創建 ffmpeg 播放器", media->filename);
            // fallback to icon
            img = lv_image_create(thumb_cont);
            lv_image_set_src(img, LV_SYMBOL_VIDEO);
            lv_obj_set_style_text_color(img, lv_color_white(), 0);
            // lv_obj_set_style_text_font(img, font_cjk, 0);
            lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            lv_obj_center(img);
        } else {
            LV_LOG_USER("設置視頻源: %s", media->filepath);
            lv_result_t rr = lv_ffmpeg_player_set_src(player, media->filepath);
            if (rr != LV_RESULT_OK) {
                LV_LOG_ERROR("ffmpeg 無法打開視頻 %s (結果: %d)", media->filepath, rr);
                lv_obj_delete(player);
                // fallback to icon
                img = lv_image_create(thumb_cont);
                lv_image_set_src(img, LV_SYMBOL_VIDEO);
                lv_obj_set_style_text_color(img, lv_color_white(), 0);
                // lv_obj_set_style_text_font(img, font_cjk, 0);
                lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                lv_obj_center(img);
            } else {
                LV_LOG_USER("視頻源設置成功，開始播放縮略圖");
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
    else if (media->is_gif)
    {
        // GIF: Try FFmpeg first for better scaling support
#if LV_USE_FFMPEG
        img = lv_ffmpeg_player_create(thumb_cont);
        if (img)
        {
            if (lv_ffmpeg_player_set_src(img, media->filepath) == LV_RESULT_OK)
            {
                lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                lv_obj_center(img);
                lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CONTAIN);
                lv_ffmpeg_player_set_auto_restart(img, true);
                lv_ffmpeg_player_set_cmd(img, LV_FFMPEG_PLAYER_CMD_START);
                LV_LOG_USER("FFmpeg 加載 GIF 縮圖成功: %s", media->filename);
            }
            else
            {
                lv_obj_delete(img);
                img = NULL;
            }
        }
#endif
        // Fallback to lv_gif
        if (img == NULL)
        {
            char prefixed_path[PATH_MAX + 3];
            snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", media->filepath);
            img = lv_gif_create(thumb_cont);
            lv_gif_set_src(img, prefixed_path);
            lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            lv_obj_center(img);
            // lv_gif might not support inner align, but we try
            // lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CONTAIN);
        }
    }
    else
    {
        // Static Images (BMP, JPG, PNG, etc.)
        char prefixed_path[PATH_MAX + 3];
        snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", media->filepath);

        img = NULL;

        // Check if JPG
        const char *ext = lv_fs_get_ext(media->filepath);
        bool is_jpg = (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0);

        LV_LOG_USER("Checking file: %s, ext: %s, is_jpg: %d", media->filename, ext ? ext : "NULL", is_jpg);

        if (is_jpg)
        {
            bitmap_wrapper_t *full_bmp = decode_jpg_to_bitmap(prefixed_path);
            if (full_bmp)
            {
                bitmap_wrapper_t *thumb_bmp = create_thumbnail_bitmap(full_bmp, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);

                // Free full bmp
                if (full_bmp->data)
                    free(full_bmp->data);
                free(full_bmp);

                if (thumb_bmp)
                {
                    img = lv_image_create(thumb_cont);
                    lv_image_set_src(img, &thumb_bmp->dsc);
                    lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                    lv_obj_center(img);
                    lv_obj_add_event_cb(img, bitmap_free_cb, LV_EVENT_DELETE, thumb_bmp);
                }
            }
        }

        if (img == NULL)
        {
            img = lv_image_create(thumb_cont);
            lv_image_set_src(img, prefixed_path);
        }

        // Check if loaded
        int32_t w = lv_image_get_src_width(img);
        if (w <= 0)
        {
            LV_LOG_WARN("lv_image 加載失敗 %s，嘗試 FFmpeg", media->filename);
            lv_obj_delete(img);
            img = NULL;

#if LV_USE_FFMPEG
            // Fallback to FFmpeg
            img = lv_ffmpeg_player_create(thumb_cont);
            if (img)
            {
                if (lv_ffmpeg_player_set_src(img, media->filepath) == LV_RESULT_OK)
                {
                    lv_ffmpeg_player_set_cmd(img, LV_FFMPEG_PLAYER_CMD_START);
                    lv_ffmpeg_player_set_auto_restart(img, false);
                }
                else
                {
                    lv_obj_delete(img);
                    img = NULL;
                }
            }
#endif
        }

        if (img)
        {
            lv_obj_set_size(img, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
            lv_obj_center(img);

            // Manual scaling for all images loaded via lv_image
            // We check if it's NOT an ffmpeg player (although ffmpeg player struct might differ,
            // checking type or just assuming if we created it via lv_image_create it's an image)
            // Actually, if we fell back to FFmpeg, img is a player.
            // lv_image_get_src_width might not work on ffmpeg player or return 0.

            // Check if it is a native image widget
            bool is_native_image = true;
#if LV_USE_FFMPEG
            if (lv_obj_check_type(img, &lv_ffmpeg_player_class))
            {
                is_native_image = false;
            }
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
        LV_LOG_ERROR("無法為 %s 創建標籤", media->filename);
        // Continue without label rather than crashing
        return;
    }

    // Use CJK font for label
    lv_obj_set_style_text_font(label, font_cjk, 0);

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
        show_fullscreen_media(media->filepath, media->is_video, media->is_audio);
    }
}

static void show_fullscreen_media(const char *filepath, bool is_video, bool is_audio)
{
    // Stop any previous audio
    stop_audio();

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

    if (is_audio)
    {
        // Show audio icon
        fullscreen_image = lv_image_create(fullscreen_container);
        lv_image_set_src(fullscreen_image, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(fullscreen_image, lv_color_white(), 0);
        // Use a larger font if possible, or scale the symbol
        // Since we don't have a huge font, we can just center it.
        // Or use a placeholder image if we had one.
        lv_obj_center(fullscreen_image);

        // Play audio in background
        play_audio(filepath);
    }
    else if (is_video)
    {
        // Play audio in background for video as well (since lv_ffmpeg might not support audio)
        play_audio(filepath);

#if LV_USE_FFMPEG
        // Create FFmpeg video player
        fullscreen_video = lv_ffmpeg_player_create(fullscreen_container);
        if (fullscreen_video == NULL) {
            LV_LOG_ERROR("無法為 %s 創建 ffmpeg 播放器", filepath);
        } else {
            lv_result_t r = lv_ffmpeg_player_set_src(fullscreen_video, filepath);
            if (r != LV_RESULT_OK) {
                LV_LOG_ERROR("lv_ffmpeg_player_set_src 失敗 %s (結果: %d)", filepath, r);
                lv_obj_delete(fullscreen_video);
                fullscreen_video = NULL;
            } else {
                LV_LOG_USER("全屏視頻源設置成功，開始播放");
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
    else if (is_gif)
    {
        // GIF: Try FFmpeg first for better scaling support
#if LV_USE_FFMPEG
        fullscreen_video = lv_ffmpeg_player_create(fullscreen_container);
        if (fullscreen_video)
        {
            if (lv_ffmpeg_player_set_src(fullscreen_video, filepath) == LV_RESULT_OK)
            {
                lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
                lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
                lv_obj_set_size(fullscreen_video, screen_w, screen_h);
                lv_obj_center(fullscreen_video);
                lv_image_set_inner_align(fullscreen_video, LV_IMAGE_ALIGN_CONTAIN);
                lv_ffmpeg_player_set_auto_restart(fullscreen_video, true);
                lv_ffmpeg_player_set_cmd(fullscreen_video, LV_FFMPEG_PLAYER_CMD_START);
                LV_LOG_USER("全屏 FFmpeg 加載 GIF 成功: %s", filepath);
            }
            else
            {
                lv_obj_delete(fullscreen_video);
                fullscreen_video = NULL;
            }
        }
#endif
        // Fallback to lv_gif
        if (fullscreen_video == NULL)
        {
            char prefixed_path[PATH_MAX + 3];
            snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", filepath);
            fullscreen_image = lv_gif_create(fullscreen_container);
            lv_gif_set_src(fullscreen_image, prefixed_path);

            lv_coord_t screen_w = lv_display_get_horizontal_resolution(NULL);
            lv_coord_t screen_h = lv_display_get_vertical_resolution(NULL);
            lv_obj_set_size(fullscreen_image, screen_w, screen_h);
            lv_image_set_inner_align(fullscreen_image, LV_IMAGE_ALIGN_CONTAIN);
        }
    }
    else
    {
        // Static Images (BMP, JPG, PNG, etc.)
        char prefixed_path[PATH_MAX + 3];
        snprintf(prefixed_path, sizeof(prefixed_path), "P:%s", filepath);

        fullscreen_image = NULL;

        // Check if JPG
        const char *ext = lv_fs_get_ext(filepath);
        bool is_jpg = (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0);

        if (is_jpg)
        {
            bitmap_wrapper_t *full_bmp = decode_jpg_to_bitmap(prefixed_path);
            if (full_bmp)
            {
                fullscreen_image = lv_image_create(fullscreen_container);
                lv_image_set_src(fullscreen_image, &full_bmp->dsc);
                lv_obj_add_event_cb(fullscreen_image, bitmap_free_cb, LV_EVENT_DELETE, full_bmp);
            }
        }

        if (fullscreen_image == NULL)
        {
            fullscreen_image = lv_image_create(fullscreen_container);
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
        LV_LOG_WARN("無法加載 Arial Unicode.ttf，嘗試 PingFang.ttc");
        // PingFang might not work as it is a collection, but worth a try if supported
        font_cjk = lv_tiny_ttf_create_file("P:/System/Library/Fonts/PingFang.ttc", 16);
    }

    if (font_cjk == NULL) {
        LV_LOG_WARN("無法加載系統字體，回退到內置字體");
    } else {
        LV_LOG_USER("成功加載自定義 CJK 字體");
    }
#else
    LV_LOG_WARN("未啟用 TinyTTF 文件支持");
#endif

    if (font_cjk == NULL) {
        // Fallback to built-in SC font
        font_cjk = (lv_font_t *)&lv_font_source_han_sans_sc_16_cjk;
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
    stop_audio();

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

static bool is_audio_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;
    return (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0 ||
            strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".flac") == 0 ||
            strcasecmp(ext, ".m4a") == 0 || strcasecmp(ext, ".aac") == 0);
}

static bool is_bmp_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;
    return (strcasecmp(ext, ".bmp") == 0);
}

static bool is_jpg_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;
    return (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0);
}

static bool is_png_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext)
        return false;
    return (strcasecmp(ext, ".png") == 0);
}

static void play_audio(const char *filepath)
{
    stop_audio();

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        // Use ffplay to play audio without display, auto exit when done, and quiet output
        // We need to construct the full path if it's not absolute, but here filepath is likely absolute or relative to CWD
        // The filepath passed from show_fullscreen_media is usually absolute (/opt/media/...)
        // But wait, in scan_media_files we construct it as /opt/media/...
        // However, we are running from project root. /opt/media is likely mapped to P:/opt/media in LVGL but real path on disk?
        // The user's log shows: scan_media_files: 發現視頻文件: 遊戲影片 demo.mp4
        // And filepath constructed as /opt/media/filename
        // But is /opt/media a real path on the mac?
        // If not, the previous code worked because maybe it does exist or mapped.
        // Let's assume the path in `media->filepath` is correct for the system.

        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", filepath, NULL);
        exit(1); // Should not reach here
    }
    else if (pid > 0)
    {
        audio_pid = pid;
    }
}

static void stop_audio(void)
{
    if (audio_pid > 0)
    {
        kill(audio_pid, SIGKILL);
        waitpid(audio_pid, NULL, 0);
        audio_pid = -1;
    }
}

static bool is_gif_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".gif") == 0);
}

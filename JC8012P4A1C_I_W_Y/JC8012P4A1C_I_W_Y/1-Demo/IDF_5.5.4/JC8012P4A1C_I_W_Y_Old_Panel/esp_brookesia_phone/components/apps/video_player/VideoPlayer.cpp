/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <fcntl.h>
#include <dirent.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_lvgl_simple_player/media_src_storage.h"
#include "esp_lvgl_simple_player/esp_lvgl_simple_player.h"
#include "VideoPlayer.hpp"

#define APP_SUPPORT_VIDEO_FILE_EXT  ".mjpeg"
#define APP_SUPPORT_AUDIO_FILE_EXT_1 ".mp3"
#define APP_SUPPORT_AUDIO_FILE_EXT_2 ".wav"
#define APP_BGM_DIR   BSP_SD_MOUNT_POINT "/music"
#define APP_MAX_VIDEO_NUM           (15)
#define APP_VIDEO_FRAME_BUF_SIZE    (BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8)
#define APP_CACHE_BUF_SIZE          (64 * 1024)
#define APP_BREAKING_NEWS_TEXT      "This example demonstrates the JPEG decoding capability of the ESP32-P4"

/* NVS config - 与 Setting 应用共享同一个 namespace */
#define NVS_STORAGE_NAMESPACE       "storage"
#define NVS_KEY_AUDIO_VOLUME        "volume"

using namespace std;

static const char *TAG = "AppVideoPlayer";

LV_IMG_DECLARE(breaking_news);
LV_IMG_DECLARE(img_app_video_player);

AppVideoPlayer::AppVideoPlayer(uint16_t width, uint16_t height):
    ESP_Brookesia_PhoneApp("Video Player", &img_app_video_player, true), // auto_resize_visual_area
    _screen_width(width),
    _screen_height(height),
    _video_name(NULL),
    img_breaking_news(NULL),
    row_edit(NULL),
    lbl_breaking_news(NULL)
{
}

AppVideoPlayer::~AppVideoPlayer()
{
}

bool AppVideoPlayer::run(void)
{
    app_show_ui();

    return true;
}

bool AppVideoPlayer::pause(void)
{
    esp_lvgl_simple_player_pause();

    return true;
}

bool AppVideoPlayer::resume(void)
{
    esp_lvgl_simple_player_resume();

    return true;
}

bool AppVideoPlayer::back(void)
{
    return notifyCoreClosed();
}

bool AppVideoPlayer::close(void)
{
    bsp_display_unlock();
    esp_lvgl_simple_player_del();
    bsp_display_lock(100);

    return true;
}

bool AppVideoPlayer::init(void)
{
    return true;
}

void AppVideoPlayer::app_show_ui(void)
{
    uint8_t i = 0;
    
    // 根据传入的屏幕分辨率按比例计算各区域尺寸（原始参考分辨率: 800x1280）
    uint16_t sw = _screen_width;
    uint16_t sh = _screen_height;
    uint16_t video_h = sh * 1150 / 1280;      // 视频区域高度（缩短与底部控制栏的间距）
    uint16_t control_bar_y = sh * 1040 / 1280; // 控制栏 Y 坐标（适当上移）
    uint16_t control_bar_h = sh * 100 / 1280;   // 控制栏高度（增大底部区域）
    
    // Add SD card status check before searching for files
    ESP_LOGI(TAG, "=== Video Player Starting ===");
    
    /* 从 NVS 读取系统音量并应用到 Codec，使视频播放跟随系统音量 */
    {
        nvs_handle_t nvs_handle;
        if (nvs_open(NVS_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
            int32_t volume = 0;
            if (nvs_get_i32(nvs_handle, NVS_KEY_AUDIO_VOLUME, &volume) == ESP_OK) {
                bsp_extra_codec_volume_set((int)volume, NULL);
                ESP_LOGI(TAG, "Synced system volume: %ld", volume);
            }
            nvs_close(nvs_handle);
        }
    }
    
    ESP_LOGI(TAG, "Checking SD card status...");
    
    struct stat st;
    if (stat(BSP_SD_MOUNT_POINT, &st) == 0 && S_ISDIR(st.st_mode)) {
        ESP_LOGI(TAG, "SD card mount point exists: %s", BSP_SD_MOUNT_POINT);
        
        // Try to list directory contents for debugging
        DIR *test_dir = opendir(BSP_SD_MOUNT_POINT);
        if (test_dir) {
            ESP_LOGI(TAG, "SD card directory is accessible");
            
            // Count total files in root directory
            int file_count = 0;
            struct dirent *entry;
            while ((entry = readdir(test_dir)) != NULL) {
                file_count++;
                if (file_count <= 10) {  // Only log first 10 files
                    ESP_LOGI(TAG, "  Root dir item: %s (type=%d)", entry->d_name, entry->d_type);
                }
            }
            ESP_LOGI(TAG, "Total items in SD root: %d", file_count);
            closedir(test_dir);
        } else {
            ESP_LOGE(TAG, "Cannot open SD card directory!");
        }
    } else {
        ESP_LOGE(TAG, "SD card mount point does NOT exist: %s", BSP_SD_MOUNT_POINT);
        ESP_LOGE(TAG, "SD card may not be mounted properly");
    }
    
    int sel_file = searchMideaFiles();

    // 检查是否有可用的视频文件
    if (_midea_info_vect.empty()) {
        ESP_LOGE(TAG, "No video files found on SD card");
    
        
        // 显示错误提示界面
        lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        
        lv_obj_t * label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Video failed to open\n\nPlease insert SD card\n\nPlace .mjpeg files under the root directory");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 80); 
        return;
    }

    // 设置全屏黑色背景，禁用滚动
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);

    /* Create main container with flex column layout */
    lv_obj_t *cont_main = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont_main, sw, sh);
    lv_obj_set_pos(cont_main, 0, 0);
    lv_obj_set_style_bg_color(cont_main, lv_color_black(), 0);
    lv_obj_set_style_border_width(cont_main, 0, 0);
    lv_obj_set_style_pad_all(cont_main, 0, 0);
    lv_obj_set_flex_flow(cont_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(cont_main, LV_OBJ_FLAG_SCROLLABLE);

    /* Video display area - auto-adapted to screen resolution */
    lv_obj_t *cont_video = lv_obj_create(cont_main);
    lv_obj_set_width(cont_video, sw);
    lv_obj_set_height(cont_video, video_h);
    lv_obj_align(cont_video, LV_ALIGN_CENTER, 0, 0); // 居中对齐
    lv_obj_set_style_bg_color(cont_video, lv_color_black(), 0);
    lv_obj_set_style_border_width(cont_video, 0, 0);
    lv_obj_set_style_pad_all(cont_video, 0, 0);
    lv_obj_clear_flag(cont_video, LV_OBJ_FLAG_SCROLLABLE);

    /* Control bar area - File selector and buttons in one row */
    lv_obj_t *cont_control_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont_control_bar, sw, control_bar_h);
    lv_obj_set_pos(cont_control_bar, 0, control_bar_y);
    lv_obj_set_style_bg_color(cont_control_bar, lv_color_hex(0x1a1a1a), 0);  // 深色背景
    lv_obj_set_style_border_width(cont_control_bar, 3, 0);  // 设置边框宽度为3px
    lv_obj_set_style_border_color(cont_control_bar, lv_color_white(), 0);  // 白色边框
    lv_obj_set_style_border_side(cont_control_bar, LV_BORDER_SIDE_FULL, 0);  // 四边都有边框
    lv_obj_set_style_pad_all(cont_control_bar, 20, 0);  // 增大内边距，让按键不贴着边缘
    lv_obj_set_flex_flow(cont_control_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_control_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 两端对齐
    lv_obj_clear_flag(cont_control_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* File selector dropdown - compact width */
    lv_obj_t *dropdown = lv_dropdown_create(cont_control_bar);
    lv_obj_set_width(dropdown, sw - 260);  // 自适应宽度：屏幕宽度减去按钮区域和边距
    lv_obj_set_height(dropdown, 45);
    lv_obj_set_style_bg_color(dropdown, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(dropdown, lv_color_white(), 0);
    
    // 添加所有找到的视频文件到下拉列表
    std::string options;
    for (size_t idx = 0; idx < _midea_info_vect.size(); idx++) {
        options += _midea_info_vect[idx].video_name;
        if (idx < _midea_info_vect.size() - 1) {
            options += "\n";
        }
    }
    lv_dropdown_set_options(dropdown, options.c_str());
    lv_dropdown_set_selected(dropdown, sel_file);
    lv_obj_add_event_cb(dropdown, file_changed, LV_EVENT_VALUE_CHANGED, this);

    /* Button group container - aligned to right */
    lv_obj_t *cont_buttons = lv_obj_create(cont_control_bar);
    lv_obj_set_size(cont_buttons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_buttons, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(cont_buttons, 0, 0);
    lv_obj_set_style_pad_all(cont_buttons, 0, 0);
    lv_obj_set_style_pad_column(cont_buttons, 15, 0);  // 按钮之间的间隔增加到15px
    lv_obj_set_flex_flow(cont_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_buttons, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Play button */
    lv_obj_t *btn_play = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_play, 55, 45);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x2196F3), 0);  // Blue color
    lv_obj_set_style_radius(btn_play, 8, 0);
    lv_obj_t *label_play = lv_label_create(btn_play);
    lv_label_set_text(label_play, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(label_play, &lv_font_montserrat_20, 0);
    lv_obj_center(label_play);
    lv_obj_add_event_cb(btn_play, [](lv_event_t *e) {
        esp_lvgl_simple_player_play();
    }, LV_EVENT_CLICKED, NULL);

    /* Pause button */
    lv_obj_t *btn_pause = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_pause, 55, 45);
    lv_obj_set_style_bg_color(btn_pause, lv_color_hex(0xFF9800), 0);  // Orange color
    lv_obj_set_style_radius(btn_pause, 8, 0);
    lv_obj_t *label_pause = lv_label_create(btn_pause);
    lv_label_set_text(label_pause, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(label_pause, &lv_font_montserrat_20, 0);
    lv_obj_center(label_pause);
    lv_obj_add_event_cb(btn_pause, [](lv_event_t *e) {
        esp_lvgl_simple_player_pause();
    }, LV_EVENT_CLICKED, NULL);

    /* Stop button */
    lv_obj_t *btn_stop = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_stop, 55, 45);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xF44336), 0);  // Red color
    lv_obj_set_style_radius(btn_stop, 8, 0);
    lv_obj_t *label_stop = lv_label_create(btn_stop);
    lv_label_set_text(label_stop, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(label_stop, &lv_font_montserrat_20, 0);
    lv_obj_center(label_stop);
    lv_obj_add_event_cb(btn_stop, [](lv_event_t *e) {
        esp_lvgl_simple_player_stop();
    }, LV_EVENT_CLICKED, NULL);

    /* Create player with video container */
    snprintf(_video_path, sizeof(_video_path), "%s/%s", BSP_SD_MOUNT_POINT, _video_name);
    
    // 检查视频文件是否存在
    FILE *test_file = fopen(_video_path, "r");
    if (test_file == NULL) {
        ESP_LOGE(TAG, "Video file not found: %s", _video_path);
        // 显示错误提示
        lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        
        lv_obj_t * label = lv_label_create(lv_scr_act());
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to open:\n%s", _video_name);
        lv_label_set_text(label, error_msg);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    fclose(test_file);
    
    // 查找当前视频的匹配音频文件，并复制到成员变量中
    _bgm_path[0] = '\0';  // 初始化为空字符串
    for (const auto &video_info : _midea_info_vect) {
        if (video_info.video_name == _video_name) {
            if (!video_info.bgm_path.empty()) {
                snprintf(_bgm_path, sizeof(_bgm_path), "%s", video_info.bgm_path.c_str());
                ESP_LOGI(TAG, "Playing video with background music: %s", _bgm_path);
            }
            break;
        }
    }
    
    const char *bgm_path = (_bgm_path[0] != '\0') ? _bgm_path : NULL;
    if (!bgm_path) {
        ESP_LOGW(TAG, "No background music found for video: %s", _video_name);
    }
    
    esp_lvgl_simple_player_cfg_t player_cfg = {
        .video_path = _video_path,
        .bgm_path = bgm_path,  // 设置背景音乐路径
        .screen = cont_video,
        .buff_size = (uint32_t)sw * sh * BSP_LCD_BITS_PER_PIXEL / 8,  // 根据屏幕分辨率动态计算帧缓冲
        .cache_buff_size = APP_CACHE_BUF_SIZE,
        .cache_buff_in_psram = true,
        .screen_width = sw,      // 根据屏幕分辨率自适应宽度
        .screen_height = video_h, // 根据屏幕分辨率自适应高度
        .flags = {
            .hide_controls = true,    // Hide buttons (play/pause/stop)
            .hide_slider = false,     // Show progress slider
            .hide_status = true,      // Hide status text
            .auto_width = false,
            .auto_height = false,
        },
    };
    
    // 创建播放器，检查返回值
    lv_obj_t *player_screen = esp_lvgl_simple_player_create(&player_cfg);
    if (player_screen == NULL) {
        ESP_LOGE(TAG, "Failed to create video player");
        // 显示错误提示
        lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        
        lv_obj_t * label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Failed to initialize\nvideo player");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    /* Start playing */
    esp_lvgl_simple_player_play();
}

uint8_t AppVideoPlayer::searchMideaFiles(void)
{
    int sel_file = 0;
    int i = 0;
    struct dirent *dir;
    DIR *d;

    _midea_info_vect.clear();

    ESP_LOGI(TAG, "========== SD Card Video File Search Debug ==========");
    ESP_LOGI(TAG, "SD Mount Point: %s", BSP_SD_MOUNT_POINT);
    ESP_LOGI(TAG, "Looking for files with extension: %s", APP_SUPPORT_VIDEO_FILE_EXT);

    // Search and store video files
    if (DIR *d = opendir(BSP_SD_MOUNT_POINT)) {
        ESP_LOGI(TAG, "Successfully opened directory: %s", BSP_SD_MOUNT_POINT);
        
        int total_files = 0;
        int video_files = 0;
        
        while (struct dirent *dir = readdir(d)) {
            total_files++;
            
            // Log all files for debugging
            ESP_LOGD(TAG, "  [%d] File: %s (type: %d)", total_files, dir->d_name, dir->d_type);
            
            if (dir->d_type != DT_DIR && strstr(dir->d_name, APP_SUPPORT_VIDEO_FILE_EXT)) {
                if (_midea_info_vect.size() >= APP_MAX_VIDEO_NUM) {
                    ESP_LOGE(TAG, "Too many video files (max: %d)", APP_MAX_VIDEO_NUM);
                    break;
                }
                ESP_LOGI(TAG, "  >>> Found video file #%d: %s", video_files + 1, dir->d_name);
                std::string video_path_str = std::string(BSP_SD_MOUNT_POINT) + "/" + dir->d_name;
                std::string bgm_path = findMatchingAudio(dir->d_name);
                _midea_info_vect.push_back({string(dir->d_name), bgm_path});
                video_files++;
            }
        }
        
        closedir(d);  // Always close the directory
        
        ESP_LOGI(TAG, "Search complete: Total files=%d, Video files=%d", total_files, video_files);
    } else {
        ESP_LOGE(TAG, "Failed to open directory: %s", BSP_SD_MOUNT_POINT);
        // Try to check if SD card is mounted by checking a common path
        struct stat st;
        if (stat(BSP_SD_MOUNT_POINT, &st) == 0) {
            ESP_LOGI(TAG, "Directory exists but cannot be opened (permission issue?)");
        } else {
            ESP_LOGE(TAG, "Directory does not exist - SD card may not be mounted");
        }
    }

    // Select the video file based on 'sel_file'
    if (sel_file >= 0 && sel_file < _midea_info_vect.size()) {
        _video_name = _midea_info_vect[sel_file].video_name.c_str();
        ESP_LOGI(TAG, "Selected video: %s", _video_name);
    } else {
        ESP_LOGW(TAG, "No video files available for selection");
    }
    
    ESP_LOGI(TAG, "=====================================================");

    return sel_file;
}

std::string AppVideoPlayer::findMatchingAudio(const std::string &video_name)
{
    // 从视频文件名生成可能的音频文件名（去掉扩展名）
    std::string base_name = video_name;
    size_t dot_pos = base_name.rfind('.');
    if (dot_pos != std::string::npos) {
        base_name = base_name.substr(0, dot_pos);
    }
    
    ESP_LOGI(TAG, "Searching for audio file matching: %s", base_name.c_str());
    
    // 尝试在SD卡根目录查找匹配的音频文件
    const char *audio_exts[] = {APP_SUPPORT_AUDIO_FILE_EXT_1, APP_SUPPORT_AUDIO_FILE_EXT_2};
    
    for (const char *ext : audio_exts) {
        std::string audio_path = std::string(BSP_SD_MOUNT_POINT) + "/" + base_name + ext;
        
        // 检查文件是否存在
        FILE *test_file = fopen(audio_path.c_str(), "r");
        if (test_file != NULL) {
            fclose(test_file);
            ESP_LOGI(TAG, "Found matching audio: %s", audio_path.c_str());
            return audio_path;
        }
    }
    
    // 如果没有找到同名音频，尝试在music子目录中查找
    for (const char *ext : audio_exts) {
        std::string audio_path = std::string(APP_BGM_DIR) + "/" + base_name + ext;
        
        FILE *test_file = fopen(audio_path.c_str(), "r");
        if (test_file != NULL) {
            fclose(test_file);
            ESP_LOGI(TAG, "Found matching audio in music dir: %s", audio_path.c_str());
            return audio_path;
        }
    }
    
    // 最后尝试查找任何音频文件作为默认背景音乐
    if (DIR *d = opendir(BSP_SD_MOUNT_POINT)) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_DIR) {
                std::string filename = dir->d_name;
                if (filename.size() > 4) {
                    std::string ext = filename.substr(filename.size() - 4);
                    // 转换为小写进行比较
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == APP_SUPPORT_AUDIO_FILE_EXT_1 || ext == APP_SUPPORT_AUDIO_FILE_EXT_2) {
                        std::string audio_path = std::string(BSP_SD_MOUNT_POINT) + "/" + filename;
                        ESP_LOGI(TAG, "Using default audio file: %s", audio_path.c_str());
                        closedir(d);
                        return audio_path;
                    }
                }
            }
        }
        closedir(d);
    }
    
    ESP_LOGW(TAG, "No audio file found");
    return "";
}

void AppVideoPlayer::file_changed(lv_event_t * e)
{
    AppVideoPlayer *app = (AppVideoPlayer *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    char *video_path = app->_video_path;
    char video_name[64];
    bool found_video_file = false;

    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_dropdown_get_selected_str(obj, video_name, sizeof(video_name));
        snprintf(video_path, sizeof(app->_video_path), "%s/%s", BSP_SD_MOUNT_POINT, video_name);
        ESP_LOGI(TAG, "Selected file: %s", video_path);

        for (auto &it : app->_midea_info_vect) {
            if (strcmp(it.video_name.c_str(), video_name) == 0) {
                found_video_file = true;
                break;
            }
        }
        if (!found_video_file) {
            ESP_LOGE(TAG, "File not found in the map");
            return;
        }

        // 检查视频文件是否可以打开
        FILE *test_file = fopen(video_path, "r");
        if (test_file == NULL) {
            ESP_LOGE(TAG, "Cannot open video file: %s", video_path);
            // 显示错误提示对话框
            bsp_display_lock(0);
            lv_obj_t * mbox = lv_msgbox_create(NULL, "Error", "Cannot open video file", NULL, false);
            lv_obj_center(mbox);
            bsp_display_unlock();
            return;
        }
        fclose(test_file);

        // 查找新视频的匹配音频，并复制到成员变量中
        app->_bgm_path[0] = '\0';  // 初始化为空字符串
        for (const auto &video_info : app->_midea_info_vect) {
            if (video_info.video_name == video_name) {
                if (!video_info.bgm_path.empty()) {
                    snprintf(app->_bgm_path, sizeof(app->_bgm_path), "%s", video_info.bgm_path.c_str());
                    ESP_LOGI(TAG, "Switching to video with background music: %s", app->_bgm_path);
                }
                break;
            }
        }
        
        const char *bgm_path = (app->_bgm_path[0] != '\0') ? app->_bgm_path : NULL;

        esp_lvgl_simple_player_stop();

        bsp_display_unlock();
        if (esp_lvgl_simple_player_wait_task_stop(-1) != ESP_OK) {
            ESP_LOGE(TAG, "Player task stop timeout");
        }
        bsp_display_lock(100);

        esp_lvgl_simple_player_change_file(video_path, bgm_path);
        esp_lvgl_simple_player_play();
    }
}

// void AppVideoPlayer::breaking_news_changed(lv_event_t * e)
// {
//     AppVideoPlayer *app = (AppVideoPlayer *)lv_event_get_user_data(e);
//     lv_event_code_t code = lv_event_get_code(e);
//     lv_obj_t * obj = lv_event_get_target(e);
//     lv_obj_t * img_breaking_news = app->img_breaking_news;

//     if(code == LV_EVENT_VALUE_CHANGED) {
//         if (img_breaking_news) {
//             const bool state = (lv_obj_get_state(obj) & LV_STATE_CHECKED);
//             if (state) {
//                 lv_obj_clear_flag(img_breaking_news, LV_OBJ_FLAG_HIDDEN);
//             } else {
//                 lv_obj_add_flag(img_breaking_news, LV_OBJ_FLAG_HIDDEN);
//             }
//         }
//     }
// }

// void AppVideoPlayer::hide_controls_changed(lv_event_t * e)
// {
//     lv_event_code_t code = lv_event_get_code(e);
//     lv_obj_t * obj = lv_event_get_target(e);
//     if(code == LV_EVENT_VALUE_CHANGED) {
//         const bool state = (lv_obj_get_state(obj) & LV_STATE_CHECKED);
//         esp_lvgl_simple_player_hide_controls(state);
//     }
// }

/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "file_iterator.h"

class AppVideoPlayer: public ESP_Brookesia_PhoneApp {
public:
    AppVideoPlayer(void);
    ~AppVideoPlayer();

    bool run(void);
    bool pause(void);
    bool resume(void);
    bool back(void);
    bool close(void);

    bool init(void) override;

private:
    typedef struct {
        std::string video_name;
        std::string bgm_path;  // 添加背景音乐路径
    } MideaInfo_t;

    void app_show_ui(void);
    const char *searchDefaultBGM(const char *video_name);
    uint8_t searchMideaFiles(void);
    std::string findMatchingAudio(const std::string &video_name);  // 查找匹配的音频文件

    static void file_changed(lv_event_t * e);
    static void breaking_news_changed(lv_event_t * e);
    static void hide_controls_changed(lv_event_t * e);
    static void edit_event_cb(lv_event_t * e);
    static void save_event_cb(lv_event_t * e);
    static void audio_player_callback(audio_player_cb_ctx_t *ctx);

    char _video_path[64];
    char _bgm_path[128];  // 添加背景音乐路径缓冲区
    const char *_video_name;
    std::vector<MideaInfo_t> _midea_info_vect;
    lv_obj_t * img_breaking_news;
    lv_obj_t * row_edit;
    lv_obj_t * lbl_breaking_news;
    file_iterator_instance_t *_file_iterator;
};
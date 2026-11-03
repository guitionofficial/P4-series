/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <inttypes.h>
#include <time.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "nvs.h"


#include "ui/ui.h"
#include "Setting.hpp"
#include "app_sntp.h"
#include "app_iperf.h"

#include "esp_brookesia_versions.h"

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

// static bool is_ethernet_ = false;

#define ENABLE_DEBUG_LOG                (0)

#define HOME_REFRESH_TASK_STACK_SIZE    (1024 * 4)
#define HOME_REFRESH_TASK_PRIORITY      (1)
#define HOME_REFRESH_TASK_PERIOD_MS     (2000)

#define WIFI_SCAN_TASK_STACK_SIZE       (1024 * 6)
#define WIFI_SCAN_TASK_PRIORITY         (1)
#define WIFI_SCAN_TASK_PERIOD_MS        (5 * 1000)

#define WIFI_CONNECT_TASK_STACK_SIZE    (1024 * 4)
#define WIFI_CONNECT_TASK_PRIORITY      (4)
#define WIFI_CONNECT_TASK_STACK_CORE    (0)
#define WIFI_CONNECT_UI_WAIT_TIME_MS    (1 * 1000)
#define WIFI_CONNECT_UI_PANEL_SIZE      (1 * 1000)
#define WIFI_CONNECT_RET_WAIT_TIME_MS   (10 * 1000)

#define SCREEN_BRIGHTNESS_MIN           (20)
#define SCREEN_BRIGHTNESS_MAX           (BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX)

#define SPEAKER_VOLUME_MIN              (0)
#define SPEAKER_VOLUME_MAX              (100)

#define NVS_STORAGE_NAMESPACE           "storage"
#define NVS_KEY_WIFI_ENABLE             "wifi_en"
#define NVS_KEY_BLE_ENABLE              "ble_en"
#define NVS_KEY_AUDIO_VOLUME            "volume"
#define NVS_KEY_DISPLAY_BRIGHTNESS      "brightness"

#define UI_MAIN_ITEM_LEFT_OFFSET        (20)
#define UI_WIFI_LIST_UP_OFFSET          (20)
#define UI_WIFI_LIST_UP_PAD             (20)
#define UI_WIFI_LIST_DOWN_PAD           (20)
#define UI_WIFI_LIST_H_PERCENT          (75)
#define UI_WIFI_LIST_ITEM_H             (60)
#define UI_WIFI_LIST_ITEM_FONT          (&lv_font_montserrat_26)
#define UI_WIFI_KEYBOARD_H_PERCENT      (30)
#define UI_WIFI_ICON_LOCK_RIGHT_OFFSET       (-10)
#define UI_WIFI_ICON_SIGNAL_RIGHT_OFFSET     (-50)
#define UI_WIFI_ICON_CONNECT_RIGHT_OFFSET    (-90)

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_4
static int adc_raw[2][10];
static int voltage[2][10];


using namespace std;

#define SCAN_LIST_SIZE      25

static const char TAG[] = "EUI_Setting";

TaskHandle_t wifi_scan_handle_task;

static EventGroupHandle_t s_wifi_event_group;

static char st_wifi_ssid[32];
static char st_wifi_password[64];

static uint8_t base_mac_addr[6] = {0};
static char mac_str[18] = {0};

static lv_obj_t* panel_wifi_btn[SCAN_LIST_SIZE];
static lv_obj_t* label_wifi_ssid[SCAN_LIST_SIZE];
static lv_obj_t* label_wifi_rssi[SCAN_LIST_SIZE];
static lv_obj_t* img_img_wifi_lock[SCAN_LIST_SIZE];
static lv_obj_t* wifi_image[SCAN_LIST_SIZE];
static lv_obj_t* wifi_connect[SCAN_LIST_SIZE];

static int brightness = 0;

LV_IMG_DECLARE(img_wifisignal_absent);
LV_IMG_DECLARE(img_wifisignal_wake);
LV_IMG_DECLARE(img_wifisignal_moderate);
LV_IMG_DECLARE(img_wifisignal_good);
LV_IMG_DECLARE(img_wifi_lock);
LV_IMG_DECLARE(img_wifi_connect_success);
LV_IMG_DECLARE(img_wifi_connect_fail);

typedef enum {
    WIFI_EVENT_CONNECTED = BIT(0),
    WIFI_EVENT_INIT_DONE = BIT(1),
    WIFI_EVENT_UI_INIT_DONE = BIT(2),
    WIFI_EVENT_SCANING = BIT(3),
    ETE_EVENT_CONNECT = BIT(4),
} wifi_event_id_t;

LV_IMG_DECLARE(img_app_setting);
extern lv_obj_t *ui_Min;
extern lv_obj_t *ui_Hour;
extern lv_obj_t *ui_Sec;
extern lv_obj_t *ui_Date;
extern lv_obj_t *ui_Clock_Number;

AppSettings::AppSettings():
    ESP_Brookesia_PhoneApp("Settings", &img_app_setting, false),                  // auto_resize_visual_area
    _is_ui_resumed(false),
    _is_ui_del(true),
    _screen_index(UI_MAIN_SETTING_INDEX),
    _screen_list({nullptr}),
    _rs485_initialized(false),
    _rs485_recv_timer(nullptr),
    _rs485_recv_buf(nullptr),
    _rs485_screen_loaded(false)
{
}

AppSettings::~AppSettings()
{
}

bool AppSettings::run(void)
{
    _is_ui_del = false;

    // Initialize Squareline UI
    ui_setting_init();

    // Get MAC
    esp_read_mac(base_mac_addr, ESP_MAC_EFUSE_FACTORY);
    snprintf(mac_str, sizeof(mac_str), "%02X-%02X-%02X-%02X-%02X-%02X",
             base_mac_addr[0], base_mac_addr[1], base_mac_addr[2],
             base_mac_addr[3], base_mac_addr[4], base_mac_addr[5]);


    // Initialize custom UI
    extraUiInit();
    
    // if(!is_ethernet_)
    // {
    // 	is_ethernet_ = true;
    // 	initEthernet();
    // }

    // Upate UI by NVS parametres
    updateUiByNvsParam();

    xEventGroupSetBits(s_wifi_event_group, WIFI_EVENT_UI_INIT_DONE);

    return true;
}

bool AppSettings::back(void)
{
    _is_ui_resumed = false;

    if (_screen_index == UI_WIFI_CONNECT_INDEX) {
        lv_scr_load(ui_ScreenSettingWiFi);
    } else if (_screen_index == UI_ETHERNET_INDEX) {
        // Stop any running speed test when navigating back
        if (app_iperf_is_running()) {
            app_iperf_stop();
        }
        lv_scr_load(ui_ScreenSettingMain);
    } else if (_screen_index == UI_RS485_TEST_INDEX) {
        _rs485_screen_loaded = false;
        lv_scr_load(ui_ScreenSettingMain);
    } else if (_screen_index != UI_MAIN_SETTING_INDEX) {
        lv_scr_load(ui_ScreenSettingMain);
    } else {
        while(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING) {
            ESP_LOGI(TAG, "WiFi is scanning, please wait");
            vTaskDelay(pdMS_TO_TICKS(100));
            stopWifiScan();
        } 
        notifyCoreClosed();
    }

    return true;
}

bool AppSettings::close(void)
{
    while(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING) {
        ESP_LOGI(TAG, "WiFi is scanning, please wait");
        vTaskDelay(pdMS_TO_TICKS(100));
        stopWifiScan();
    } 
    
    _is_ui_del = true;

    // Cleanup RS485
    _rs485_screen_loaded = false;
    if (_rs485_recv_timer) {
        lv_timer_del(_rs485_recv_timer);
        _rs485_recv_timer = nullptr;
    }
    if (_rs485_initialized) {
        uart_port_t uart_num = (uart_port_t)RS485_UART_NUM;
        uart_driver_delete(uart_num);
        _rs485_initialized = false;
        ESP_LOGI(TAG, "RS485: UART driver deleted");
    }
    if (_rs485_recv_buf) {
        free(_rs485_recv_buf);
        _rs485_recv_buf = nullptr;
    }

    return true;
}

bool AppSettings::init(void)
{
    ESP_Brookesia_Phone *phone = getPhone();
    ESP_Brookesia_PhoneHome& home = phone->getHome();
    status_bar = home.getStatusBar();
    backstage = home.getRecentsScreen();

     i2c_master_get_bus_handle(1,&RX8028_handle);

     i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x32,
        .scl_speed_hz = 100000,
    };

    i2c_master_bus_add_device(RX8028_handle, &dev_cfg, &RX8028_dev);

    // adc_oneshot_chan_cfg_t config = {
    //     .atten = EXAMPLE_ADC_ATTEN,
    //     .bitwidth = ADC_BITWIDTH_DEFAULT,
    // };

    // adc_oneshot_unit_init_cfg_t init_config2 = {
    //     .unit_id = ADC_UNIT_2,
    //     .ulp_mode = ADC_ULP_MODE_DISABLE,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    // //-------------ADC2 Calibration Init---------------//
    // do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, EXAMPLE_ADC2_CHAN0, EXAMPLE_ADC_ATTEN, &adc2_cali_handle);

    // //-------------ADC2 Config---------------//
    // ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN0, &config));

    adc_battery_estimation_t config = {
        .internal = {
            .adc_unit = ADC_UNIT_2,
            .adc_bitwidth = ADC_BITWIDTH_DEFAULT,
            .adc_atten = EXAMPLE_ADC_ATTEN,
        },
        .adc_channel = EXAMPLE_ADC2_CHAN0,
        .upper_resistor = 85000,
        .lower_resistor = 100000,
        .battery_points = default_battery_points,
        .battery_points_count = DEFAULT_POINTS_COUNT,
        .charging_detect_cb = NULL,
        .charging_detect_user_data = NULL,
    };

    adc_battery_estimation_handle = adc_battery_estimation_create(&config);

    // Initialize NVS parameters
    _nvs_param_map[NVS_KEY_WIFI_ENABLE] = false;
    _nvs_param_map[NVS_KEY_BLE_ENABLE] = false;
    _nvs_param_map[NVS_KEY_AUDIO_VOLUME] = bsp_extra_codec_volume_get();
    _nvs_param_map[NVS_KEY_AUDIO_VOLUME] = max(min((int)_nvs_param_map[NVS_KEY_AUDIO_VOLUME], SPEAKER_VOLUME_MAX), SPEAKER_VOLUME_MIN);
    // _nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS] = bsp_display_brightness_get();
    _nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS] = brightness;
    _nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS] = max(min((int)_nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS], SCREEN_BRIGHTNESS_MAX), SCREEN_BRIGHTNESS_MIN);
    // Load NVS parameters if exist
    loadNvsParam();
    // Update System parameters
    bsp_extra_codec_volume_set(_nvs_param_map[NVS_KEY_AUDIO_VOLUME], (int *)&_nvs_param_map[NVS_KEY_AUDIO_VOLUME]);
    bsp_display_brightness_set(_nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS]);

    xTaskCreate(euiRefresTask, "Home Refresh", HOME_REFRESH_TASK_STACK_SIZE, this, HOME_REFRESH_TASK_PRIORITY, NULL);
    xTaskCreate(euiBatteryTask, "Battey Refresh", HOME_REFRESH_TASK_STACK_SIZE, this, HOME_REFRESH_TASK_PRIORITY, NULL);
    xTaskCreate(wifiScanTask, "WiFi Scan", WIFI_SCAN_TASK_STACK_SIZE, this, WIFI_SCAN_TASK_PRIORITY, NULL);
    xTaskCreate(ethernetMonitorTask, "Eth Mon", HOME_REFRESH_TASK_STACK_SIZE, this, HOME_REFRESH_TASK_PRIORITY, NULL);


    return true;
}

bool AppSettings::pause(void)
{
    _is_ui_resumed = true;

    return true;
}

bool AppSettings::resume(void)
{
    _is_ui_resumed = false;

    return true;
}

void AppSettings::extraUiInit(void)
{
    /* Main */
    lv_label_set_text(ui_LabelPanelSettingMainContainer3Volume, "Audio");
    lv_label_set_text(ui_LabelPanelSettingMainContainer4Light, "Display");
    // Record the screen index and install the screen loaded event callback
    _screen_list[UI_MAIN_SETTING_INDEX] = ui_ScreenSettingMain;
    lv_obj_add_event_cb(ui_ScreenSettingMain, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    /* WiFi */
    // Switch
    lv_obj_add_event_cb(ui_SwitchPanelScreenSettingWiFiSwitch, onSwitchPanelScreenSettingWiFiSwitchValueChangeEventCallback,
                        LV_EVENT_VALUE_CHANGED, this);
    // List
    // lv_obj_clear_flag(ui_PanelScreenSettingWiFiList, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_PanelScreenSettingWiFiList, LV_DIR_VER);
    lv_obj_set_height(ui_PanelScreenSettingWiFiList, lv_pct(UI_WIFI_LIST_H_PERCENT));
    lv_obj_align_to(ui_PanelScreenSettingWiFiList, ui_PanelScreenSettingWiFiSwitch, LV_ALIGN_OUT_BOTTOM_MID, 0,
                    UI_WIFI_LIST_UP_OFFSET);
    lv_obj_set_style_pad_all(ui_PanelScreenSettingWiFiList, 0, 0);
    lv_obj_set_style_pad_top(ui_PanelScreenSettingWiFiList, UI_WIFI_LIST_UP_PAD, 0);
    lv_obj_set_style_pad_bottom(ui_PanelScreenSettingWiFiList, UI_WIFI_LIST_DOWN_PAD, 0);
    for(int i = 0; i < SCAN_LIST_SIZE; i++) {
        panel_wifi_btn[i] = lv_obj_create(ui_PanelScreenSettingWiFiList);
        lv_obj_set_size(panel_wifi_btn[i], lv_pct(100), UI_WIFI_LIST_ITEM_H);
        lv_obj_set_style_radius(panel_wifi_btn[i], 0, 0);
        lv_obj_set_style_border_width(panel_wifi_btn[i], 0, 0);
        lv_obj_set_style_text_font(panel_wifi_btn[i], UI_WIFI_LIST_ITEM_FONT, 0);
        lv_obj_add_flag(panel_wifi_btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag( panel_wifi_btn[i], LV_OBJ_FLAG_SCROLLABLE );
        lv_obj_set_style_bg_color(panel_wifi_btn[i], lv_color_hex(0xCBCBCB), LV_PART_MAIN | LV_STATE_PRESSED );
        lv_obj_set_style_bg_opa(panel_wifi_btn[i], 255, LV_PART_MAIN| LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel_wifi_btn[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
        lv_obj_set_style_border_opa(panel_wifi_btn[i], 255, LV_PART_MAIN| LV_STATE_DEFAULT);

        label_wifi_ssid[i] = lv_label_create(panel_wifi_btn[i]);
        lv_obj_set_align(label_wifi_ssid[i], LV_ALIGN_LEFT_MID);

        label_wifi_rssi[i] = lv_label_create(panel_wifi_btn[i]);
        lv_obj_center(label_wifi_rssi[i]);

        img_img_wifi_lock[i] = lv_img_create(panel_wifi_btn[i]);
        lv_obj_align(img_img_wifi_lock[i], LV_ALIGN_RIGHT_MID, UI_WIFI_ICON_LOCK_RIGHT_OFFSET, 0);
        lv_obj_add_flag(img_img_wifi_lock[i], LV_OBJ_FLAG_HIDDEN);

        wifi_image[i] = lv_img_create(panel_wifi_btn[i]);
        lv_obj_align(wifi_image[i], LV_ALIGN_RIGHT_MID, UI_WIFI_ICON_SIGNAL_RIGHT_OFFSET, 0);

        wifi_connect[i] = lv_label_create(panel_wifi_btn[i]);
        lv_label_set_text(wifi_connect[i], LV_SYMBOL_OK);
        lv_obj_align(wifi_connect[i], LV_ALIGN_RIGHT_MID, UI_WIFI_ICON_CONNECT_RIGHT_OFFSET, 0);
        lv_obj_add_flag(wifi_connect[i], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(panel_wifi_btn[i], onButtonWifiListClickedEventCallback, LV_EVENT_CLICKED, (void*)label_wifi_ssid[i]);
        if(!(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING)) {
            lv_obj_add_flag(ui_PanelScreenSettingWiFiList, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_SpinnerScreenSettingWiFi, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_add_flag(ui_ButtonScreenSettingWiFiReturn, LV_OBJ_FLAG_HIDDEN);
    // Connect
    lv_obj_add_flag(ui_SpinnerScreenSettingVerification, LV_OBJ_FLAG_HIDDEN);
    _panel_wifi_connect = lv_obj_create(ui_ScreenSettingVerification);
    lv_obj_set_size(_panel_wifi_connect, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(_panel_wifi_connect, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(_panel_wifi_connect, LV_OPA_50, 0);
    lv_obj_center(_panel_wifi_connect);
    _img_wifi_connect = lv_img_create(_panel_wifi_connect);
    lv_obj_center(_img_wifi_connect);
    _spinner_wifi_connect = lv_spinner_create(_panel_wifi_connect, 1000, 600);
    lv_obj_set_size(_spinner_wifi_connect, lv_pct(20), lv_pct(20));
    lv_obj_center(_spinner_wifi_connect);
    processWifiConnect(WIFI_CONNECT_HIDE);
    // Keyboard
    lv_textarea_set_password_mode(ui_TextAreaScreenSettingVerificationPassword, true);
    // lv_obj_set_size(ui_KeyboardScreenSettingVerification, lv_pct(100), lv_pct(UI_WIFI_KEYBOARD_H_PERCENT));
    // lv_obj_align(ui_KeyboardScreenSettingVerification, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(ui_KeyboardScreenSettingVerification, onKeyboardScreenSettingVerificationClickedEventCallback,
                        LV_EVENT_CLICKED, this);
    // Record the screen index and install the screen loaded event callback
    lv_obj_add_flag(ui_ButtonScreenSettingBLEReturn, LV_OBJ_FLAG_HIDDEN);
    _screen_list[UI_WIFI_SCAN_INDEX] = ui_ScreenSettingWiFi;
    lv_obj_add_event_cb(ui_ScreenSettingWiFi, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);
    _screen_list[UI_WIFI_CONNECT_INDEX] = ui_ScreenSettingVerification;
    lv_obj_add_event_cb(ui_ScreenSettingVerification, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    /* Bluetooth */
    lv_obj_add_event_cb(ui_SwitchPanelScreenSettingBLESwitch, onSwitchPanelScreenSettingBLESwitchValueChangeEventCallback,
                        LV_EVENT_VALUE_CHANGED, this);
    // Record the screen index and install the screen loaded event callback
    _screen_list[UI_BLUETOOTH_SETTING_INDEX] = ui_ScreenSettingBLE;
    lv_obj_add_event_cb(ui_ScreenSettingBLE, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    /* Display */
    lv_slider_set_range(ui_SliderPanelScreenSettingLightSwitch1, SCREEN_BRIGHTNESS_MIN, SCREEN_BRIGHTNESS_MAX);
    lv_obj_add_event_cb(ui_SliderPanelScreenSettingLightSwitch1, onSliderPanelLightSwitchValueChangeEventCallback,
                        LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_flag(ui_ButtonScreenSettingLightReturn, LV_OBJ_FLAG_HIDDEN);
    // Record the screen index and install the screen loaded event callback
    _screen_list[UI_BRIGHTNESS_SETTING_INDEX] = ui_ScreenSettingLight;
    lv_obj_add_event_cb(ui_ScreenSettingLight, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    /* Audio */
    lv_slider_set_range(ui_SliderPanelScreenSettingVolumeSwitch, SPEAKER_VOLUME_MIN, SPEAKER_VOLUME_MAX);
    lv_obj_add_event_cb(ui_SliderPanelScreenSettingVolumeSwitch, onSliderPanelVolumeSwitchValueChangeEventCallback,
                        LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_flag(ui_ButtonScreenSettingVolumeReturn, LV_OBJ_FLAG_HIDDEN);
    // Record the screen index and install the screen loaded event callback
    _screen_list[UI_VOLUME_SETTING_INDEX] = ui_ScreenSettingVolume;
    lv_obj_add_event_cb(ui_ScreenSettingVolume, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    /* About */
    lv_label_set_text(ui_LabelPanelPanelScreenSettingAbout4, "ESP_Brookesia");
    lv_obj_add_flag(ui_ButtonScreenSettingAboutReturn, LV_OBJ_FLAG_HIDDEN);
    // Record the screen index and install the screen loaded event callback
    _screen_list[UI_ABOUT_SETTING_INDEX] = ui_ScreenSettingAbout;
    lv_obj_add_event_cb(ui_ScreenSettingAbout, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    lv_label_set_text(ui_LabelPanelPanelScreenSettingAbout3, mac_str);
    {
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        char chip_ver[32];
        snprintf(chip_ver, sizeof(chip_ver), "v%d.%d", chip_info.revision / 100, chip_info.revision % 100);
        lv_label_set_text(ui_LabelPanelPanelScreenSettingAbout6, chip_ver);
    }
    lv_label_set_text(ui_LabelPanelPanelScreenSettingAbout2, "ESP32-P4-Function-EV-Board");
    lv_obj_set_x( ui_LabelPanelPanelScreenSettingAbout2, 167 );

    extern const char *SOFTWARE_VERSION;
    lv_label_set_text(ui_LabelPanelPanelScreenSettingAbout5, SOFTWARE_VERSION);

    /* Ethernet Screen */
    _screen_list[UI_ETHERNET_INDEX] = ui_ScreenSettingEthernet;
    lv_obj_add_event_cb(ui_ScreenSettingEthernet, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);
    lv_obj_add_event_cb(ui_SwitchEthernet, onSwitchEthernetValueChangeEventCallback,
                        LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_ButtonEthernetServerStart, onButtonEthernetServerStartClickedEventCallback,
                        LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_ButtonEthernetClientStart, onButtonEthernetClientStartClickedEventCallback,
                        LV_EVENT_CLICKED, this);

    /* RS485 Test Screen */
    _screen_list[UI_RS485_TEST_INDEX] = ui_ScreenSettingRS485;
    lv_obj_add_event_cb(ui_ScreenSettingRS485, onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);
    // Attach send button event
    lv_obj_add_event_cb(ui_ButtonRS485Send, onBtnRS485SendClick, LV_EVENT_CLICKED, this);
    // Attach clear button event
    lv_obj_add_event_cb(ui_BtnRS485Clear, onBtnRS485SendClick, LV_EVENT_CLICKED, this);
    // NOTE: rs485Init() and receive timer are created in onScreenLoadEventCallback
    // when entering the RS485 screen, and cleaned up when leaving, to avoid
    // GPIO conflict with I2S audio (UART_RTS=GPIO10 shared with I2S_WS).
}

void AppSettings::processWifiConnect(WifiConnectState_t state)
{
    switch (state) {
    case WIFI_CONNECT_HIDE:
        lv_obj_add_flag(_panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_img_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_spinner_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        break;
    case WIFI_CONNECT_RUNNING:
        lv_obj_clear_flag(_panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_img_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_spinner_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        break;
    case WIFI_CONNECT_SUCCESS:
        lv_obj_clear_flag(_panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_img_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(_img_wifi_connect, &img_wifi_connect_success);
        lv_obj_add_flag(_spinner_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        break;
    case WIFI_CONNECT_FAIL:
        lv_obj_clear_flag(_panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_img_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(_img_wifi_connect, &img_wifi_connect_fail);
        lv_obj_add_flag(_spinner_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }
}

bool AppSettings::loadNvsParam(void)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    for (auto& key_value : _nvs_param_map) {
        err = nvs_get_i32(nvs_handle, key_value.first.c_str(), &key_value.second);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Load %s: %d", key_value.first.c_str(), key_value.second);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            err = nvs_set_i32(nvs_handle, key_value.first.c_str(), key_value.second);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error (%s) setting %s", esp_err_to_name(err), key_value.first.c_str());
            }
            ESP_LOGW(TAG, "The value of %s is not initialized yet, set it to default value: %d", key_value.first.c_str(),
                     key_value.second);
            break;
        default:
            break;
        }
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS changes", esp_err_to_name(err));
        return false;
    }
    nvs_close(nvs_handle);

    return true;
}

bool AppSettings::setNvsParam(std::string key, int value)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_i32(nvs_handle, key.c_str(), value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) setting %s", esp_err_to_name(err), key.c_str());
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS changes", esp_err_to_name(err));
        return false;
    }
    nvs_close(nvs_handle);

    return true;
}

void AppSettings::updateUiByNvsParam(void)
{
    if (_nvs_param_map[NVS_KEY_WIFI_ENABLE]) {
        lv_obj_add_state(ui_SwitchPanelScreenSettingWiFiSwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_SwitchPanelScreenSettingWiFiSwitch, LV_STATE_CHECKED);
    }

    if (_nvs_param_map[NVS_KEY_BLE_ENABLE]) {
        lv_obj_add_state(ui_SwitchPanelScreenSettingBLESwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_SwitchPanelScreenSettingBLESwitch, LV_STATE_CHECKED);
    }

    lv_slider_set_value(ui_SliderPanelScreenSettingLightSwitch1, _nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS], LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderPanelScreenSettingVolumeSwitch, _nvs_param_map[NVS_KEY_AUDIO_VOLUME], LV_ANIM_OFF);
}

esp_err_t AppSettings::initWifi()
{
    s_wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_CONNECTED);
    xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_INIT_DONE);
    xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_SCANING);
    xEventGroupClearBits(s_wifi_event_group, ETE_EVENT_CONNECT);
    if(!(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_UI_INIT_DONE)) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_UI_INIT_DONE);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        this,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t AppSettings::initEthernet()
{
     // Initialize Ethernet driver
    // uint8_t eth_port_cnt = 0;
    // esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));


    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_netifs[0] = esp_netif_new(&cfg);
        eth_netif_glues[0] = esp_eth_new_netif_glue(eth_handles[0]);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[0], eth_netif_glues[0]));
    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };
        char if_key_str[10];
        char if_desc_str[10];
        char num_str[3];
        for (int i = 0; i < eth_port_cnt; i++) {
            itoa(i, num_str, 10);
            strcat(strcpy(if_key_str, "ETH_"), num_str);
            strcat(strcpy(if_desc_str, "eth"), num_str);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i*5;
            eth_netifs[i] = esp_netif_new(&cfg_spi);
            eth_netif_glues[i] = esp_eth_new_netif_glue(eth_handles[i]);
            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[i], eth_netif_glues[i]));
        }
    }

    // Register user defined event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
    return ESP_OK;
}

esp_err_t AppSettings::deInitEthernet(void)
{
      ESP_LOGI(TAG, "stop and deinitialize Ethernet network...");
    // Stop Ethernet driver state machine and destroy netif
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_stop(eth_handles[i]));
        ESP_ERROR_CHECK(esp_eth_del_netif_glue(eth_netif_glues[i]));
        esp_netif_destroy(eth_netifs[i]);
    }
    ESP_ERROR_CHECK(example_eth_deinit(eth_handles, eth_port_cnt));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_got_ip_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
    return ESP_OK;
}


void AppSettings::startWifiScan(void)
{
    ESP_LOGI(TAG, "Start Wi-Fi scan");
    xEventGroupSetBits(s_wifi_event_group, WIFI_EVENT_SCANING);
    lv_obj_clear_flag(ui_SpinnerScreenSettingWiFi, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_SwitchPanelScreenSettingWiFiSwitch, LV_OBJ_FLAG_CLICKABLE);
}

void AppSettings::stopWifiScan(void)
{
    ESP_LOGI(TAG, "Stop Wi-Fi scan");
    xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_SCANING);
    lv_obj_add_flag(ui_PanelScreenSettingWiFiList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SpinnerScreenSettingWiFi, LV_OBJ_FLAG_HIDDEN);
    deinitWifiListButton();
}

void AppSettings::scanWifiAndUpdateUi(void)
{
    bool psk_flag = false;

    uint16_t number = SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_start();
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
#if ENABLE_DEBUG_LOG
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
#endif

    bsp_display_lock(0);
    if(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING) {
        deinitWifiListButton();
    }
    bsp_display_unlock();

    for (int i = 0; (i < SCAN_LIST_SIZE) && (i < ap_count); i++) {
#if ENABLE_DEBUG_LOG
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
#endif

        if(ap_info[i].authmode != WIFI_AUTH_OPEN && ap_info[i].authmode != WIFI_AUTH_OWE) {
            psk_flag = true;
        }
#if ENABLE_DEBUG_LOG
        ESP_LOGI(TAG, "psk_flag: %d", psk_flag);
#endif

        if(ap_info[i].rssi > -100 && ap_info[i].rssi <= -80) {
            _wifi_signal_strength_level = WIFI_SIGNAL_STRENGTH_WEAK;
        } else if(ap_info[i].rssi > -80 && ap_info[i].rssi <= -60) {
            _wifi_signal_strength_level = WIFI_SIGNAL_STRENGTH_MODERATE;
        } else if(ap_info[i].rssi > -60) {
            _wifi_signal_strength_level = WIFI_SIGNAL_STRENGTH_GOOD;
        } else {
            _wifi_signal_strength_level = WIFI_SIGNAL_STRENGTH_NONE;
        }
#if ENABLE_DEBUG_LOG
        ESP_LOGI(TAG, "signal_strength: %d", _wifi_signal_strength_level);
#endif

        bsp_display_lock(0);
        if(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING) {
            initWifiListButton(label_wifi_ssid[i], img_img_wifi_lock[i], wifi_image[i], label_wifi_rssi[i],wifi_connect[i],
                                ap_info[i].ssid, ap_info[i].rssi,psk_flag, _wifi_signal_strength_level);     
        }
        bsp_display_unlock();
    }
}

void AppSettings::initWifiListButton(lv_obj_t* lv_label_ssid, lv_obj_t* lv_img_wifi_lock, lv_obj_t* lv_wifi_img,lv_obj_t *lv_label_rssi,
                                     lv_obj_t *lv_wifi_connect, uint8_t* ssid, int8_t rssi ,bool psk, WifiSignalStrengthLevel_t signal_strength)
{
    lv_label_set_text_fmt(lv_label_ssid, "%s", (const char*)ssid);
    lv_label_set_text_fmt(lv_label_rssi,"%ddB",rssi);
    lv_obj_center(lv_label_rssi);
    
    if (strcmp((const char*)ssid, (const char*)st_wifi_ssid) == 0) {
        lv_obj_clear_flag(lv_wifi_connect, LV_OBJ_FLAG_HIDDEN);
    }

    if(psk) {
        lv_img_set_src(lv_img_wifi_lock, &img_wifi_lock);
        lv_obj_clear_flag(lv_img_wifi_lock, LV_OBJ_FLAG_HIDDEN);
    }

    if (signal_strength == WIFI_SIGNAL_STRENGTH_GOOD) {
        lv_img_set_src(lv_wifi_img, &img_wifisignal_good);
    } else if (signal_strength == WIFI_SIGNAL_STRENGTH_MODERATE) {
        lv_img_set_src(lv_wifi_img, &img_wifisignal_moderate);
    } else if (signal_strength == WIFI_SIGNAL_STRENGTH_WEAK) {
        lv_img_set_src(lv_wifi_img, &img_wifisignal_wake);
    } else {
        lv_img_set_src(lv_wifi_img, &img_wifisignal_absent);
    }
}

void AppSettings::deinitWifiListButton(void)
{
    for (int i = 0; i < SCAN_LIST_SIZE; i++) {
        lv_obj_add_flag(img_img_wifi_lock[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_connect[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void AppSettings::euiRefresTask(void *arg)
{
    AppSettings *app = (AppSettings *)arg;
    time_t now;
    struct tm timeinfo;
    bool is_time_pm = false;
    // char textBuf[50];
    uint16_t free_sram_size_kb = 0;
    uint16_t total_sram_size_kb = 0;
    uint16_t free_psram_size_kb = 0;
    uint16_t total_psram_size_kb = 0;

    uint8_t data_add[1] = {0x01};
    uint8_t data_time[4];

    if (app == NULL) {
        ESP_LOGE(TAG, "App instance is NULL");
        goto err;
    }

    
    while (1) {
        /* Update status bar */
        // time
        // time(&now);
        // localtime_r(&now, &timeinfo);
        i2c_master_transmit_receive(app->RX8028_dev,data_add,1,app->time_rx,2,100);
        //  ESP_LOGI(TAG,"time %02d:%02d,time_16 :%02x:%02x",app->time_rx[1],app->time_rx[0],app->time_rx[1],app->time_rx[0]);
        //  is_time_pm = (timeinfo.tm_hour >= 12);
        is_time_pm = (app->time_rx[1] >= 12);

        bsp_display_lock(0);
        if(!app->status_bar->setClock(app->time_rx[1], app->time_rx[0], is_time_pm)) {
            ESP_LOGE(TAG, "Set clock failed");
        }
        bsp_display_unlock();

        // Update WiFi icon state
        if((xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_CONNECTED)) {
            app_sntp_init();

            time(&now);
            localtime_r(&now, &timeinfo);
            data_time[0] = 0x00;
            data_time[1] = timeinfo.tm_sec/10 *16 + timeinfo.tm_sec%10;
            data_time[2] = timeinfo.tm_min/10 *16 + timeinfo.tm_min%10;
            data_time[3] = timeinfo.tm_hour/10 *16 + timeinfo.tm_hour%10;

            i2c_master_transmit(app->RX8028_dev,data_time,4,100);

            bsp_display_lock(0);
            if(app->_wifi_signal_strength_level == WIFI_SIGNAL_STRENGTH_NONE) {
                app->status_bar->setWifiIconState(0);
            } else if(app->_wifi_signal_strength_level == WIFI_SIGNAL_STRENGTH_WEAK) {
                app->status_bar->setWifiIconState(1);
            } else if(app->_wifi_signal_strength_level == WIFI_SIGNAL_STRENGTH_MODERATE) {
                app->status_bar->setWifiIconState(2);
            } else if (app->_wifi_signal_strength_level == WIFI_SIGNAL_STRENGTH_GOOD) {
                app->status_bar->setWifiIconState(3);
            }
            bsp_display_unlock();
        }
         if((xEventGroupGetBits(s_wifi_event_group) & ETE_EVENT_CONNECT)) {
            bsp_display_lock(0);
            // app->status_bar->setWifiIconState(1);
            app->status_bar->setEthernetIconState(1);
            bsp_display_unlock();
        } else {
             bsp_display_lock(0);
            // app->status_bar->setWifiIconState(0);
            app->status_bar->setEthernetIconState(0);
            bsp_display_unlock();
        }

        /* Updte Smart Gadget app */
        // app->updateGadgetTime(timeinfo);

        // Update memory in backstage
        if(app->backstage->checkVisible()) {
            free_sram_size_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
            total_sram_size_kb = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
            free_psram_size_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
            total_psram_size_kb = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;
            ESP_LOGI(TAG, "Free sram size: %d KB, total sram size: %d KB, "
                        "free psram size: %d KB, total psram size: %d KB",
                        free_sram_size_kb, total_sram_size_kb, free_psram_size_kb, total_psram_size_kb);

            bsp_display_lock(0);
            if(!app->backstage->setMemoryLabel(free_sram_size_kb, total_sram_size_kb, free_psram_size_kb, total_psram_size_kb)) {
                ESP_LOGE(TAG, "Update memory usage failed");
            }
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(HOME_REFRESH_TASK_PERIOD_MS));
    }

err:
    vTaskDelete(NULL);
}



 void AppSettings::euiBatteryTask(void *arg)
 {
    AppSettings *app = (AppSettings *)arg;

    while(1)
    {
       
        float capacity = 0;
        adc_battery_estimation_get_capacity(app->adc_battery_estimation_handle, &capacity);
        
        adc_battery_estimation_get_charging_state(app->adc_battery_estimation_handle,&app->charge_flag);
        // printf("Battery capacity: %.1f%%\n", capacity);
         
 
         bsp_display_lock(0);

         if(!app->status_bar->setBatteryPercent(app->charge_flag,capacity))
         {
             ESP_LOGE(TAG,"Set battery failed");
         }
        //  if(app->charge_flag)
        //  {
        //     app->status_bar->hideBatteryPercent();
        //  }
        //  else
        //  {
        //     app->status_bar->showBatteryPercent();
        //  }
         bsp_display_unlock();

         vTaskDelay(pdMS_TO_TICKS(5000));
    }
    
    vTaskDelete(NULL);
 }


void AppSettings::wifiScanTask(void *arg)
{
    AppSettings *app = (AppSettings *)arg;
    esp_err_t ret = ESP_OK;

    if (app == NULL) {
        ESP_LOGE(TAG, "App instance is NULL");
        goto err;
    }

    ret = app->initWifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init Wi-Fi failed");
        goto err;
    }

    if (ret == ESP_OK) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_EVENT_INIT_DONE);
        ESP_LOGI(TAG, "wifi_init done");
    } else {
        ESP_LOGE(TAG, "wifi_init failed");
    }

    while (true) {
        if((xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_INIT_DONE) &&
           (xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_UI_INIT_DONE)){
            lv_obj_add_flag(ui_SwitchPanelScreenSettingWiFiSwitch, LV_OBJ_FLAG_CLICKABLE);
            xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_INIT_DONE);
            xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_UI_INIT_DONE);
        }

        if(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING){
            app->scanWifiAndUpdateUi();
            vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_TASK_PERIOD_MS));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

err:
    vTaskDelete(NULL);
}

void AppSettings::wifiConnectTask(void *arg)
{
    AppSettings *app = (AppSettings *)arg;
    wifi_config_t wifi_config = { 0 };

    esp_wifi_disconnect();
    app->status_bar->setWifiIconState(0);

    memcpy(st_wifi_ssid, lv_label_get_text(ui_LabelScreenSettingVerificationSSID), sizeof(wifi_config.sta.ssid));
    memcpy(st_wifi_password, lv_textarea_get_text(ui_TextAreaScreenSettingVerificationPassword), sizeof(wifi_config.sta.ssid));

    memcpy(wifi_config.sta.ssid, st_wifi_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, st_wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_LOGI(TAG, "SSID:%s, password:%s.", wifi_config.sta.ssid, wifi_config.sta.password);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_EVENT_CONNECTED,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_RET_WAIT_TIME_MS));

    if (bits & WIFI_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Connected successfully");

        if (!app->_is_ui_del) {
            bsp_display_lock(0);
            app->processWifiConnect(WIFI_CONNECT_SUCCESS);
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_UI_WAIT_TIME_MS));

        if (!app->_is_ui_del) {
            bsp_display_lock(0);
            app->processWifiConnect(WIFI_CONNECT_HIDE);
            // lv_obj_clear_flag(ui_KeyboardScreenSettingVerification, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_text(ui_TextAreaScreenSettingVerificationPassword, "");
            app->back();
            bsp_display_unlock();
        }

        // app->updateGadgetTime(timeinfo);
    } else {
        ESP_LOGI(TAG, "Connect failed");

        if (!app->_is_ui_del) {
            bsp_display_lock(0);
            app->processWifiConnect(WIFI_CONNECT_FAIL);
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_UI_WAIT_TIME_MS));

        if (!app->_is_ui_del) {
            bsp_display_lock(0);
            app->processWifiConnect(WIFI_CONNECT_HIDE);
            // lv_obj_clear_flag(ui_KeyboardScreenSettingVerification, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_text(ui_TextAreaScreenSettingVerificationPassword, "");
            // app->back();
            bsp_display_unlock();
        }
    }

    // if (!app->_is_ui_del) {
    //     xEventGroupSetBits(s_wifi_event_group, WIFI_EVENT_SCANING);
    //     app->startWifiScan();
    // }

    vTaskDelete(NULL);
}

void AppSettings::wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    AppSettings *app = (AppSettings *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_EVENT_CONNECTED);
        ESP_LOGI(TAG, "connected to ap SSID:%s, password:%s.", st_wifi_ssid, st_wifi_password);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_CONNECTED);
        ESP_LOGI(TAG, "disconnected from ap SSID:%s, password:%s.", st_wifi_ssid, st_wifi_password);
        memset(st_wifi_ssid, 0, sizeof(st_wifi_ssid));

        // app->back();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        if(lv_obj_has_flag(ui_PanelScreenSettingWiFiList, LV_OBJ_FLAG_HIDDEN) &&
           xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_SCANING) {
            if (!app->_is_ui_del) {
                bsp_display_lock(0);
                lv_obj_clear_flag(ui_PanelScreenSettingWiFiList, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SpinnerScreenSettingWiFi, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SwitchPanelScreenSettingWiFiSwitch, LV_OBJ_FLAG_CLICKABLE);
                app->status_bar->setWifiIconState(0);
                bsp_display_unlock();
            }
        }
    }
}

void AppSettings::onKeyboardScreenSettingVerificationClickedEventCallback(lv_event_t *e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);

    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    lv_keyboard_set_textarea(target, ui_TextAreaScreenSettingVerificationPassword);

    if(lv_keyboard_get_selected_btn(target) == 39) {
        app->processWifiConnect(WIFI_CONNECT_RUNNING);
        // lv_obj_add_flag(ui_KeyboardScreenSettingVerification, LV_OBJ_FLAG_HIDDEN);

        app->stopWifiScan();

        xTaskCreatePinnedToCore(wifiConnectTask, "wifi Connect", WIFI_CONNECT_TASK_STACK_SIZE, app,
                                WIFI_CONNECT_TASK_PRIORITY, NULL, WIFI_CONNECT_TASK_STACK_CORE);
    }

end:
    return;
}

void AppSettings::onScreenLoadEventCallback( lv_event_t * e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    SettingScreenIndex_t last_scr_index = app->_screen_index;

    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    for (int i = 0; i < UI_MAX_INDEX; i++) {
        if (app->_screen_list[i] == lv_event_get_target(e)) {
            app->_screen_index = (SettingScreenIndex_t)i;
            break;
        }
    }

    if (last_scr_index == UI_WIFI_SCAN_INDEX) {
        app->stopWifiScan();
    }

    // Stop speed test when leaving the speed test screen
    if (last_scr_index == UI_ETHERNET_INDEX) {
        if (app_iperf_is_running()) {
            app_iperf_stop();
        }
    }

    if ((app->_screen_index == UI_WIFI_SCAN_INDEX) && (app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] == true)) {
        app->startWifiScan();
    }

    // Update Ethernet info when entering the speed test screen
    if (app->_screen_index == UI_ETHERNET_INDEX) {
        // Update local IP display
        app->updateEthernetScreenUi();
    }

    // RS485 lifecycle: deinit when leaving, init when entering
    if (last_scr_index == UI_RS485_TEST_INDEX && app->_screen_index != UI_RS485_TEST_INDEX) {
        // Leaving RS485 screen -> cleanup UART to free GPIO10 for I2S audio
        if (app->_rs485_recv_timer) {
            lv_timer_del(app->_rs485_recv_timer);
            app->_rs485_recv_timer = nullptr;
        }
        if (app->_rs485_initialized) {
            uart_port_t uart_num = (uart_port_t)RS485_UART_NUM;
            uart_driver_delete(uart_num);
            app->_rs485_initialized = false;
            ESP_LOGI(TAG, "RS485: UART deinitialized (freed GPIO for audio)");
        }
    }
    if (app->_screen_index == UI_RS485_TEST_INDEX && !app->_rs485_initialized) {
        // Entering RS485 screen -> init UART and create receive timer
        app->rs485Init();
        if (app->_rs485_initialized && app->_rs485_recv_timer == nullptr) {
            app->_rs485_recv_timer = lv_timer_create(onRS485RecvTimer, 50, app);
        }
    }

    // RS485 screen loaded state
    app->_rs485_screen_loaded = (app->_screen_index == UI_RS485_TEST_INDEX);

end:
    return;
}

void AppSettings::onSwitchPanelScreenSettingWiFiSwitchValueChangeEventCallback( lv_event_t * e) {
    lv_state_t state = lv_obj_get_state(ui_SwitchPanelScreenSettingWiFiSwitch);

    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    if (state & LV_STATE_CHECKED) {
        app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] = true;
        app->setNvsParam(NVS_KEY_WIFI_ENABLE, 1);
        if (app->_screen_index == UI_WIFI_SCAN_INDEX) {
            app->startWifiScan();
        }
    } else {
        app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] = false;
        app->setNvsParam(NVS_KEY_WIFI_ENABLE, 0);
        if (app->_screen_index == UI_WIFI_SCAN_INDEX) {
            app->stopWifiScan();
            if (xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_CONNECTED) {
                ESP_ERROR_CHECK(esp_wifi_disconnect());
                app->status_bar->setWifiIconState(0);
            }
        }
    }

end:
    return;
}

void AppSettings::onButtonWifiListClickedEventCallback(lv_event_t * e)
{
    lv_obj_t *label_wifi_ssid = (lv_obj_t*)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_area_t btn_click_area;
    lv_point_t point;

    lv_obj_get_click_area(btn, &btn_click_area);
    lv_indev_get_point(lv_indev_get_act(), &point);
    if ((point.x < btn_click_area.x1) || (point.x > btn_click_area.x2) ||
        (point.y < btn_click_area.y1) || (point.y > btn_click_area.y2)) {
        return;
    }

    lv_scr_load(ui_ScreenSettingVerification);
    lv_label_set_text_fmt(ui_LabelScreenSettingVerificationSSID, "%s", lv_label_get_text(label_wifi_ssid));

    xEventGroupClearBits(s_wifi_event_group, WIFI_EVENT_SCANING);

    esp_wifi_scan_stop();
}

void AppSettings::onSwitchPanelScreenSettingBLESwitchValueChangeEventCallback( lv_event_t * e) {
    lv_state_t state = lv_obj_get_state(ui_SwitchPanelScreenSettingBLESwitch);

    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    if (state & LV_STATE_CHECKED) {
        app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] = true;
        app->setNvsParam(NVS_KEY_BLE_ENABLE, 1);
    } else {
        app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] = false;
        app->setNvsParam(NVS_KEY_BLE_ENABLE, 0);
    }

end:
    return;
}

void AppSettings::onSliderPanelVolumeSwitchValueChangeEventCallback( lv_event_t * e) {
    int volume = lv_slider_get_value(ui_SliderPanelScreenSettingVolumeSwitch);

    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    if (volume != app->_nvs_param_map[NVS_KEY_AUDIO_VOLUME]) {
        if ((bsp_extra_codec_volume_set(volume, NULL) != ESP_OK) && (bsp_extra_codec_volume_get() != volume)) {
            ESP_LOGE(TAG, "Set volume failed");
            lv_slider_set_value(ui_SliderPanelScreenSettingVolumeSwitch, app->_nvs_param_map[NVS_KEY_AUDIO_VOLUME], LV_ANIM_OFF);
            return;
        }
        app->_nvs_param_map[NVS_KEY_AUDIO_VOLUME] = volume;
        app->setNvsParam(NVS_KEY_AUDIO_VOLUME, volume);
    }

end:
    return;
}

void AppSettings::onSliderPanelLightSwitchValueChangeEventCallback( lv_event_t * e) {
    brightness = lv_slider_get_value(ui_SliderPanelScreenSettingLightSwitch1);

    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    if (brightness != app->_nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS]) {
        // if ((bsp_display_brightness_set(brightness) != ESP_OK) && (bsp_display_brightness_get() != brightness)) {
        if (bsp_display_brightness_set(brightness) != ESP_OK) {
            ESP_LOGE(TAG, "Set brightness failed");
            lv_slider_set_value(ui_SliderPanelScreenSettingLightSwitch1, app->_nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS], LV_ANIM_OFF);
            return;
        }
        app->_nvs_param_map[NVS_KEY_DISPLAY_BRIGHTNESS] = brightness;
        app->setNvsParam(NVS_KEY_DISPLAY_BRIGHTNESS, brightness);
    }

end:
    return;
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}


void AppSettings::eth_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    AppSettings *app = (AppSettings *)arg;
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        // if(lvgl_port_lock(-1))
        // {
        //     app->status_bar->setEthernetIconState(0);
        //     lvgl_port_unlock();
        // }
        xEventGroupClearBits(s_wifi_event_group, ETE_EVENT_CONNECT);
        
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
void AppSettings::eth_got_ip_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
    AppSettings *app = (AppSettings *)arg;

    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~"); 

    ESP_LOGI(TAG, "Ethernet Link Down");
    // if(lvgl_port_lock(-1))
    // {
    //     app->status_bar->setEthernetIconState(1);
    //     lvgl_port_unlock();
    // }
    xEventGroupSetBits(s_wifi_event_group, ETE_EVENT_CONNECT);
}

void AppSettings::onSwitchEthernetValueChangeEventCallback(lv_event_t *e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");
    {
    bool status = lv_obj_has_state(ui_SwitchEthernet, LV_STATE_CHECKED);
    if (status) {
        ESP_LOGI(TAG, "Ethernet enabling...");
        app->initEthernet();
    } else {
        ESP_LOGI(TAG, "Ethernet disabling...");
        if (app_iperf_is_running()) {
            app_iperf_stop();
        }
        app->deInitEthernet();
    }
    }

end:
    return;
}

void AppSettings::onButtonEthernetServerStartClickedEventCallback(lv_event_t *e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");
    {
    /* Check if ethernet is connected */
    bool eth_ok = (xEventGroupGetBits(s_wifi_event_group) & ETE_EVENT_CONNECT);
    if (!eth_ok) {
        lv_obj_clear_flag(ui_LabelEthernetServerWarn, LV_OBJ_FLAG_HIDDEN);
        goto end;
    }
    lv_obj_add_flag(ui_LabelEthernetServerWarn, LV_OBJ_FLAG_HIDDEN);

    if (app_iperf_is_running()) {
        app_iperf_stop();
        lv_label_set_text(ui_LabelEthernetServerButton, "Start Server");
        lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Stopped");
        lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_ButtonEthernetServerStart, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(ui_LabelEthernetServerButton, "Starting...");
        lv_obj_set_style_bg_color(ui_ButtonEthernetServerStart, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Starting...");
        lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0xFF9800), LV_PART_MAIN | LV_STATE_DEFAULT);

        esp_err_t ret = app_iperf_start_udp_server();
        if (ret == ESP_OK) {
            lv_label_set_text(ui_LabelEthernetServerButton, "Stop Server");
            lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Running - waiting for client");
            lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(ui_LabelEthernetServerButton, "Start Server");
            lv_obj_set_style_bg_color(ui_ButtonEthernetServerStart, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Failed to start");
            lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    }

end:
    return;
}

void AppSettings::onButtonEthernetClientStartClickedEventCallback(lv_event_t *e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");
    {
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("STA_DEF");
    if (sta_netif == NULL) sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    bool wifi_ok = false;
    if (sta_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            wifi_ok = true;
        }
    }

    if (!wifi_ok) {
        lv_obj_clear_flag(ui_LabelEthernetClientWarn, LV_OBJ_FLAG_HIDDEN);
        goto end;
    }
    lv_obj_add_flag(ui_LabelEthernetClientWarn, LV_OBJ_FLAG_HIDDEN);

    /* Mutual exclusion: if Ethernet test running, stop it */
    if (app_iperf_is_running() && app_iperf_get_active_iface() == APP_IPERF_IFACE_ETHERNET) {
        app_iperf_stop();
        lv_label_set_text(ui_LabelEthernetServerButton, "Start Server");
        lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Idle");
        lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_ButtonEthernetServerStart, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (app_iperf_is_running() && app_iperf_get_active_iface() == APP_IPERF_IFACE_WIFI) {
        app_iperf_stop();
        lv_label_set_text(ui_LabelEthernetClientButton, "Start Test");
        lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Stopped");
        lv_obj_set_style_text_color(ui_LabelEthernetClientStatus, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_ButtonEthernetClientStart, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);
        goto end;
    }

    lv_label_set_text(ui_LabelEthernetClientButton, "Starting...");
    lv_obj_set_style_bg_color(ui_ButtonEthernetClientStart, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Starting...");
    lv_obj_set_style_text_color(ui_LabelEthernetClientStatus, lv_color_hex(0xFF9800), LV_PART_MAIN | LV_STATE_DEFAULT);

    esp_err_t ret = app_iperf_start_wifi_server();
    if (ret == ESP_OK) {
        lv_label_set_text(ui_LabelEthernetClientButton, "Stop Test");
        lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Running - waiting for client");
        lv_obj_set_style_text_color(ui_LabelEthernetClientStatus, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(ui_LabelEthernetClientButton, "Start Test");
        lv_obj_set_style_bg_color(ui_ButtonEthernetClientStart, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Failed to start");
        lv_obj_set_style_text_color(ui_LabelEthernetClientStatus, lv_color_hex(0xF44336), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    }

end:
    return;
}

void AppSettings::ethernetMonitorTask(void *arg)
{
    AppSettings *app = (AppSettings *)arg;
    ESP_BROOKESIA_CHECK_NULL_GOTO(app, end, "Invalid app pointer");

    while (1) {
        if (app->_screen_index == UI_ETHERNET_INDEX) {
            bsp_display_lock(0);
            app->updateEthernetScreenUi();
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

end:
    vTaskDelete(NULL);
}

void AppSettings::updateEthernetScreenUi(void)
{
    if (ui_SwitchEthernet == NULL) return;
    bool eth_connected = (xEventGroupGetBits(s_wifi_event_group) & ETE_EVENT_CONNECT);
    bool switch_on = lv_obj_has_state(ui_SwitchEthernet, LV_STATE_CHECKED);

    /* Get IP address */
    char ip_buf[32] = "IP: --";
    if (switch_on && eth_connected) {
        esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
        if (eth_netif == NULL) eth_netif = esp_netif_get_handle_from_ifkey("ETH_0");
        if (eth_netif != NULL) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
                snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&ip_info.ip));
            }
        }
    }

    /* Update IP in left panel */
    if (eth_connected && switch_on) {
        char panel_ip[48];
        snprintf(panel_ip, sizeof(panel_ip), "Ethernet IP: %s", ip_buf);
        lv_label_set_text(ui_LabelEthernetServerIP, panel_ip);
    } else {
        lv_label_set_text(ui_LabelEthernetServerIP, "IP: --");
    }

    /* Get WiFi IP */
    char wifi_ip_buf[32] = "IP: --";
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("STA_DEF");
    if (sta_netif == NULL) sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            snprintf(wifi_ip_buf, sizeof(wifi_ip_buf), IPSTR, IP2STR(&ip_info.ip));
        }
    }
    lv_label_set_text(ui_LabelEthernetClientIP, wifi_ip_buf);

    /* Update iperf status for both panels */
    const app_iperf_status_t *iperf = app_iperf_get_status();
    lv_obj_add_flag(ui_LabelEthernetServerWarn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelEthernetClientWarn, LV_OBJ_FLAG_HIDDEN);

    /* Shared speed/status */
    char speed_buf[48];
    if (iperf && iperf->max_speed_mbps > 0.001f) {
        snprintf(speed_buf, sizeof(speed_buf), "Peak: %.1f Mbps", iperf->max_speed_mbps);
    } else {
        snprintf(speed_buf, sizeof(speed_buf), "Peak: -- Mbps");
    }

    if (app_iperf_is_running() && app_iperf_get_active_iface() == APP_IPERF_IFACE_ETHERNET) {
        lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Running");
        lv_obj_set_style_text_color(ui_LabelEthernetServerStatus, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_LabelEthernetServerSpeed, speed_buf);
    } else if (app_iperf_is_running() && app_iperf_get_active_iface() == APP_IPERF_IFACE_WIFI) {
        lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Running");
        lv_obj_set_style_text_color(ui_LabelEthernetClientStatus, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_LabelEthernetClientSpeed, speed_buf);
    } else if (!app_iperf_is_running()) {
        lv_label_set_text(ui_LabelEthernetServerStatus, "Status: Idle");
        lv_label_set_text(ui_LabelEthernetClientStatus, "Status: Idle");
    }
}

/* ================ RS485 ================ */

/* Helper: get current timestamp string [HH:MM:SS] */
static void getTimestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) != NULL && timeinfo.tm_year > (2024 - 1900)) {
        snprintf(buf, len, "[%02d:%02d:%02d]", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        // Fallback if time not set yet
        uint32_t ms = esp_timer_get_time() / 1000;
        uint32_t sec = ms / 1000;
        uint32_t min = sec / 60;
        sec %= 60;
        min %= 60;
        snprintf(buf, len, "[%02d:%02d.%03d]", min, sec, (unsigned int)(ms % 1000));
    }
}

void AppSettings::rs485Init(void)
{
    if (_rs485_initialized) {
        return;
    }

    // Allocate receive buffer
    _rs485_recv_buf = (uint8_t *)heap_caps_malloc(RS485_BUF_SIZE, MALLOC_CAP_INTERNAL);
    if (!_rs485_recv_buf) {
        ESP_LOGE(TAG, "RS485: Failed to allocate receive buffer");
        return;
    }
    memset(_rs485_recv_buf, 0, RS485_BUF_SIZE);

    uart_config_t uart_config = {
        .baud_rate = RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_port_t uart_num = (uart_port_t)RS485_UART_NUM;

    esp_err_t ret = uart_driver_install(uart_num, RS485_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS485: Failed to install UART driver: %d", ret);
        return;
    }

    ret = uart_param_config(uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS485: Failed to configure UART params: %d", ret);
        uart_driver_delete(uart_num);
        return;
    }

    ret = uart_set_pin(uart_num, RS485_TXD, RS485_RXD, RS485_RTS, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS485: Failed to set UART pins: %d", ret);
        uart_driver_delete(uart_num);
        return;
    }

    ret = uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS485: Failed to set RS485 mode: %d", ret);
        uart_driver_delete(uart_num);
        return;
    }

    uart_set_rx_timeout(uart_num, 3);

    _rs485_initialized = true;

    ESP_LOGI(TAG, "RS485 initialized: TXD=%d, RXD=%d, RTS=%d, BAUD=%d",
             RS485_TXD, RS485_RXD, RS485_RTS, RS485_BAUD);

    // Update status bar
    lv_label_set_text(ui_LabelRS485Status, "Status: Ready");
    lv_obj_set_style_text_color(ui_LabelRS485Status, lv_color_hex(0xA5D6A7), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void AppSettings::rs485Send(const char *data)
{
    if (!_rs485_initialized) {
        ESP_LOGE(TAG, "RS485: UART not initialized");
        return;
    }

    uart_port_t uart_num = (uart_port_t)RS485_UART_NUM;
    int len = strlen(data);
    uart_write_bytes(uart_num, data, len);
    uart_wait_tx_done(uart_num, portMAX_DELAY);

    ESP_LOGI(TAG, "RS485 TX: %s (%d bytes)", data, len);
}

void AppSettings::addRS485ConsoleText(const char *text)
{
    if (!ui_LabelRS485Console) return;

    // Append text to the console label
    char buf[4096];
    const char *current = lv_label_get_text(ui_LabelRS485Console);
    size_t current_len = strlen(current);
    size_t new_len = strlen(text);

    if (current_len + new_len + 1 > sizeof(buf)) {
        // Buffer full, start fresh
        lv_label_set_text(ui_LabelRS485Console, text);
    } else {
        snprintf(buf, sizeof(buf), "%s%s", current, text);
        lv_label_set_text(ui_LabelRS485Console, buf);
    }
}

/* RS485: Send button clicked */
void AppSettings::onBtnRS485SendClick(lv_event_t *e)
{
    AppSettings *app = (AppSettings *)lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (!app) return;

    // Hide keyboard when Send or Clear is pressed
    lv_obj_add_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);

    if (target == ui_ButtonRS485Send) {
        // Send content from textarea (defaults to "Hello,Guition")
        const char *text = lv_textarea_get_text(ui_TextAreaRS485Input);
        if (text == NULL || strlen(text) == 0) {
            text = "Hello,Guition";
        }

        // Add newline if not present
        char send_buf[256];
        snprintf(send_buf, sizeof(send_buf), "%s\n", text);

        char ts[32];
        getTimestamp(ts, sizeof(ts));

        if (app->_rs485_initialized) {
            app->rs485Send(send_buf);

            char display_buf[384];
            snprintf(display_buf, sizeof(display_buf), "%s >>> TX: %s\n", ts, send_buf);
            ui_RS485_ConsoleAppend(display_buf);
        } else {
            char display_buf[384];
            snprintf(display_buf, sizeof(display_buf), "%s >>> ERROR: RS485 not initialized\n", ts);
            ui_RS485_ConsoleAppend(display_buf);
        }
    } else if (target == ui_BtnRS485Clear) {
        // Clear console (resets to initial text and scrolls to top)
        ui_RS485_ConsoleClear();
    }
}

/* RS485: Timer callback to check for received data */
void AppSettings::onRS485RecvTimer(lv_timer_t *timer)
{
    AppSettings *app = (AppSettings *)timer->user_data;
    if (!app || !app->_rs485_screen_loaded || !app->_rs485_initialized || !app->_rs485_recv_buf) {
        return;
    }

    uart_port_t uart_num = (uart_port_t)RS485_UART_NUM;
    int len = uart_read_bytes(uart_num, app->_rs485_recv_buf, RS485_BUF_SIZE - 1, 0);

    if (len > 0) {
        app->_rs485_recv_buf[len] = '\0';

        ESP_LOGI(TAG, "RS485 RX: %s (%d bytes)", (char *)app->_rs485_recv_buf, len);

        // Get timestamp
        char ts[32];
        getTimestamp(ts, sizeof(ts));

        // Display received data with timestamp
        char display_buf[384];
        // Strip trailing newline for cleaner output
        char *rx_str = (char *)app->_rs485_recv_buf;
        size_t rx_len = strlen(rx_str);
        if (rx_len > 0 && rx_str[rx_len - 1] == '\n') {
            rx_str[rx_len - 1] = '\0';
        }
        snprintf(display_buf, sizeof(display_buf),
                 "%s <<< RX [%d]: %s\n",
                 ts, len, rx_str);
        ui_RS485_ConsoleAppend(display_buf);

        // Update status
        char status[64];
        snprintf(status, sizeof(status), "Status: RX %d bytes", len);
        lv_label_set_text(ui_LabelRS485Status, status);
        lv_obj_set_style_text_color(ui_LabelRS485Status, lv_color_hex(0xA5D6A7), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
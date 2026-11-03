/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <map>
#include <string>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_event.h"
#include "adc_battery_estimation.h"
#include "esp_wifi.h"
#include "esp_eth.h"
#include "ethernet_init.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "app_iperf.h"
#include "driver/uart.h"

#define ETH_PORT_CNT    1


class AppSettings: public ESP_Brookesia_PhoneApp {
public:
    AppSettings();
    ~AppSettings();

    bool run(void);
    bool back(void);
    bool close(void);

    bool init(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    typedef enum {
        UI_MAIN_SETTING_INDEX = 0,
        UI_WIFI_SCAN_INDEX,
        UI_WIFI_CONNECT_INDEX,
        UI_BLUETOOTH_SETTING_INDEX,
        UI_VOLUME_SETTING_INDEX,
        UI_BRIGHTNESS_SETTING_INDEX,
        UI_ABOUT_SETTING_INDEX,
        UI_ETHERNET_INDEX,
        UI_RS485_TEST_INDEX,
        UI_MAX_INDEX,
    } SettingScreenIndex_t;

    typedef enum {
        WIFI_SIGNAL_STRENGTH_NONE = 0,
        WIFI_SIGNAL_STRENGTH_WEAK = 1,
        WIFI_SIGNAL_STRENGTH_MODERATE = 2,
        WIFI_SIGNAL_STRENGTH_GOOD = 3,
    } WifiSignalStrengthLevel_t;

    typedef enum {
        WIFI_CONNECT_HIDE = 0,
        WIFI_CONNECT_RUNNING,
        WIFI_CONNECT_SUCCESS,
        WIFI_CONNECT_FAIL,
    } WifiConnectState_t;

    /* Operations */
    // UI
    void extraUiInit(void);
    void processWifiConnect(WifiConnectState_t state);
    void initWifiListButton(lv_obj_t* lv_label_ssid, lv_obj_t* lv_img_wifi_lock, lv_obj_t* lv_wifi_img,lv_obj_t *lv_label_rssi,
                              lv_obj_t *lv_wifi_connect, uint8_t* ssid, int8_t rssi ,bool psk, WifiSignalStrengthLevel_t signal_strength);
    void deinitWifiListButton(void);
    // NVS Parameters
    bool loadNvsParam(void);
    bool setNvsParam(std::string key, int value);
    void updateUiByNvsParam(void);
    // WiFi
    esp_err_t initWifi(void);
    void startWifiScan(void);
    void stopWifiScan(void);
    void scanWifiAndUpdateUi(void);

    //ethernet
    esp_err_t initEthernet(void);
    esp_err_t deInitEthernet(void);

    // Smart Gadget
    // void updateGadgetTime(struct tm timeinfo);

    esp_err_t initBattery(void);
    static void getBatteryPercent(void *arg);

    /* Task */
    static void euiRefresTask(void *arg);
    static void euiBatteryTask(void *arg);
    static void wifiScanTask(void *arg);
    static void wifiConnectTask(void *arg);
    /* Event Handler */
    // WiFi
    static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    // ethernet
    static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void eth_got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    /* UI Event Callback */
    // Main
    static void onScreenLoadEventCallback( lv_event_t * e);
    // WiFi
    static void onSwitchPanelScreenSettingWiFiSwitchValueChangeEventCallback( lv_event_t * e);
    static void onButtonWifiListClickedEventCallback(lv_event_t * e);
    static void onKeyboardScreenSettingVerificationClickedEventCallback(lv_event_t *e);
    // Bluetooth
    static void onSwitchPanelScreenSettingBLESwitchValueChangeEventCallback( lv_event_t * e);
    // Audio
    static void onSliderPanelVolumeSwitchValueChangeEventCallback( lv_event_t * e);
    // Brightness
    static void onSliderPanelLightSwitchValueChangeEventCallback( lv_event_t * e);

    // Ethernet Screen
    static void onSwitchEthernetValueChangeEventCallback( lv_event_t * e);
    static void onButtonEthernetServerStartClickedEventCallback( lv_event_t * e);
    static void onButtonEthernetClientStartClickedEventCallback( lv_event_t * e);
    static void ethernetMonitorTask(void *arg);
    void updateEthernetScreenUi(void);

    // RS485
    void rs485Init(void);
    void rs485Send(const char *data);
    void addRS485ConsoleText(const char *text);
    static void onBtnRS485SendClick(lv_event_t *e);
    static void onRS485RecvTimer(lv_timer_t *timer);

    bool _is_ui_resumed;
    bool _is_ui_del;
    int adc_raw[2][10];
    int voltage[2][10];
    SettingScreenIndex_t _screen_index;
    WifiSignalStrengthLevel_t _wifi_signal_strength_level;
    lv_obj_t *_panel_wifi_connect;
    lv_obj_t *_spinner_wifi_connect;
    lv_obj_t *_img_wifi_connect;
    std::array<lv_obj_t *, UI_MAX_INDEX> _screen_list;
    std::map<std::string, int32_t> _nvs_param_map;
    const ESP_Brookesia_StatusBar *status_bar; 
    const ESP_Brookesia_RecentsScreen *backstage;

    bool do_calibration2;
    bool charge_flag;
    adc_oneshot_unit_handle_t adc2_handle;
    adc_cali_handle_t adc2_cali_handle;

    adc_battery_estimation_handle_t adc_battery_estimation_handle;

    i2c_master_bus_handle_t  RX8028_handle;
    i2c_master_dev_handle_t  RX8028_dev;
    uint8_t time_rx[2];

    uint8_t eth_port_cnt = ETH_PORT_CNT;
    esp_eth_handle_t *eth_handles;
    esp_netif_t *eth_netifs[ETH_PORT_CNT];
    esp_eth_netif_glue_handle_t eth_netif_glues[ETH_PORT_CNT];

    // RS485 members
    bool _rs485_initialized;
    lv_timer_t *_rs485_recv_timer;
    uint8_t *_rs485_recv_buf;
    bool _rs485_screen_loaded;
    static constexpr int RS485_UART_NUM = 1;
    static constexpr int RS485_TXD = 26;
    static constexpr int RS485_RXD = 27;
    static constexpr int RS485_RTS = -1;
    static constexpr int RS485_BAUD = 115200;
    static constexpr int RS485_BUF_SIZE = 512;
};

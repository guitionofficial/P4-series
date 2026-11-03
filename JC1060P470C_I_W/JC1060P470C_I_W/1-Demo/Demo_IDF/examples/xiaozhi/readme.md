进入github下载代码：[https://github.com/78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)；

esp-idf版本建议v5.5

在终端中设置为esp32p4

```c
idf.py set-target esp32p4
```

将提供的jc1060p470文件夹添加到main/boards文件夹中；

将mipi_lcd_display.cc和mipi_lcd_display.h头文件添加到main/display文件夹中；

添加jd9165库文件，添加完成后用提供的文件夹进行替换

```c
idf.py add-dependency "espressif/esp_lcd_jd9165^1.0.2"
```

添加wifi_remote库文件

```c
idf.py add-dependency "espressif/esp_wifi_remote^0.4.1"
```

修改main/Kconfig.projbuild文件，在choice BOARD_TYPE中添加

```c
config BOARD_TYPE_JC1060P470
        bool "jc1060p470"
```

修改main/CMakeLists.txt文件，在87行下方添加

```c
elseif(CONFIG_BOARD_TYPE_JC1060P470)
    set(BOARD_TYPE "jc1060p470") 
```

将main/CMakeLists.tx文件中的if(CONFIG_IDF_TARGET_ESP32S3)修改为if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4)；

将system_info.cc文件34行修改为

```c
esp_wifi_get_mac(WIFI_IF_STA,mac);
```

注释main/application.cc中44和45两行，并将其复制到board.StartNetwork()下方，如下所示

```c
    board.StartNetwork();
    ota_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    ota_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
```

在main/application.cc，application.h,background_task.cc中ctrl+f搜索CONFIG_IDF_TARGET_ESP32S3，将其替换为

```c
CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
```

在终端输入idf.py menuconfig,进入xiaozhi Assistant，进入Board Type选择jc1060p470，返回top页面，进入ESP Speech Recognition，进入select wake words，选择自己需要的唤醒词，保存配置并退出

打开78__esp-wifi-connect/wifi_configuration_ap.cc,修改GetSsid函数，

```c
std::string WifiConfigurationAp::GetSsid()
{
    // Get MAC and use it to generate a unique SSID
    uint8_t mac[6];
    // ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    esp_wifi_get_mac(WIFI_IF_AP,mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X%02X", ssid_prefix_.c_str(), mac[3], mac[4], mac[5]);
    return std::string(ssid);
}
```

将std::string ssid = GetSsid();移到ESP_ERROR_CHECK(esp_wifi_init(&cfg));下方

```c
    // Initialize the WiFi stack in Access Point mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Get the SSID
    std::string ssid = GetSsid();
```


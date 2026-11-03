#include <stdio.h>
#include <algorithm>
#include <fcntl.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "driver/jpeg_decode.h"
#include "ImageDisplay.hpp"
#include "app_gui/app_image_display.h"

#define APP_SUPPORT_IMAGE_FILE_EXT ".jpg"
#define IMAGE_DIR   BSP_SPIFFS_MOUNT_POINT "/image"
#define APP_IMAGE_FRAME_BUF_SIZE   (1024 * 1280 * 2)
#define APP_CACHE_BUF_SIZE         (64 * 1024)

typedef enum {
    IMAGE_EVENT_TASK_RUN = BIT(0),
    IMAGE_EVENT_DELETE = BIT(1),
    IMAGE_EVENT_DIR = BIT(2),
} image_event_id_t;

static void image_change_display(file_iterator_instance_t *ft,int index);

static int image_count = 0;
static int count_now = 0;

static jpeg_decoder_handle_t jpgd_handle;

static jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
};

static jpeg_decode_cfg_t decode_cfg_rgb = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};


static jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {
    .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
};

static jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
    .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
};

using namespace std;

static const char *TAG = "AppImageDisplay";

static EventGroupHandle_t image_event_group;
static uint8_t *output_buf[2];
static size_t output_buf_size[2];

LV_IMG_DECLARE(img_app_img_display);

AppImageDisplay::AppImageDisplay():
    ESP_Brookesia_PhoneApp("Image", &img_app_img_display, true),
    _image_name(NULL),
    _image_file_iterator(NULL)
{
}

AppImageDisplay::~AppImageDisplay()
{
}

bool AppImageDisplay::run(void)
{
    app_image_display_init();

    lv_obj_add_event_cb(lv_scr_act(),image_change_cb,LV_EVENT_GESTURE,this);
    xEventGroupClearBits(image_event_group, IMAGE_EVENT_DELETE);
    xEventGroupSetBits(image_event_group, IMAGE_EVENT_TASK_RUN);

    for(int i=0;i<2;i++)
    {
        output_buf_size[i] = 0;
         output_buf[i] = (uint8_t*)jpeg_alloc_decoder_mem(1024 * 600 *2, &rx_mem_cfg, &output_buf_size[i]);
    }
   

    image_count = file_iterator_get_count(_image_file_iterator);
    ESP_LOGI(TAG,"image file count = %d",image_count);

    xTaskCreatePinnedToCore((TaskFunction_t)image_delay_change, "Image Init", 2048, this, 3, NULL, 0);

    // image_change_display(_image_file_iterator,count_now);

    xEventGroupSetBits(image_event_group, IMAGE_EVENT_TASK_RUN);

    return true;
}

bool AppImageDisplay::pause(void)
{
    // app_image_display_pause();
    xEventGroupClearBits(image_event_group, IMAGE_EVENT_TASK_RUN);

    return true;
}

bool AppImageDisplay::resume(void)
{
    // app_image_display_resume();
    xEventGroupSetBits(image_event_group, IMAGE_EVENT_DIR);
    xEventGroupSetBits(image_event_group, IMAGE_EVENT_TASK_RUN);
    return true;
}

bool AppImageDisplay::back(void)
{
    return notifyCoreClosed();
}

bool AppImageDisplay::close(void)
{
    // app_image_display_close();
    xEventGroupSetBits(image_event_group, IMAGE_EVENT_DELETE);
    for(int i=0;i<2;i++)
    {
        free(output_buf[i]);
    }
    return true;
}

bool AppImageDisplay::init(void)
{
    image_event_group = xEventGroupCreate();
     xEventGroupClearBits(image_event_group, IMAGE_EVENT_TASK_RUN);
     xEventGroupClearBits(image_event_group, IMAGE_EVENT_DELETE);
     xEventGroupClearBits(image_event_group, IMAGE_EVENT_DIR);

    if (bsp_extra_file_instance_init(IMAGE_DIR, &_image_file_iterator) != ESP_OK) {
        ESP_LOGE(TAG, "bsp_extra_file_instance_init failed");
        return false;
    }

    return true;
}
static int img_cnt =0;

static void image_change_display(file_iterator_instance_t *ft,int index)
{
    if(xEventGroupGetBits(image_event_group) & IMAGE_EVENT_DIR)
    {
        xEventGroupClearBits(image_event_group,IMAGE_EVENT_DIR);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    jpeg_decoder_handle_t local_jpgd_handle = NULL;
    esp_err_t err = jpeg_new_decoder_engine(&decode_eng_cfg, &local_jpgd_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create jpeg decoder engine, err=%d", err);
        return;
    }
    
    if(img_cnt == 0)
        img_cnt = 1;
    else
        img_cnt = 0;

    jpeg_decode_picture_info_t image_info;
    uint8_t *input_buf = NULL;

    const char *image_name = file_iterator_get_name_from_index(ft,index);
    ESP_LOGI(TAG,"image name = %s",image_name);

    static char image_path[256];
    file_iterator_get_full_path_from_index(ft,index,image_path,256);
    ESP_LOGI(TAG,"index = %d",index);
    ESP_LOGI(TAG,"image path = %s",image_path);

    FILE *image_fp = fopen(image_path,"rb");
    if(image_fp == NULL)
    {
        ESP_LOGE(TAG,"fopen file failed");
        jpeg_del_decoder_engine(local_jpgd_handle);
        return;
    }

    fseek(image_fp,0,SEEK_END);
    int image_size_fp = ftell(image_fp);
    fseek(image_fp,0,SEEK_SET);

    size_t input_buffer_size_image = 0;
    input_buf =  (uint8_t*)jpeg_alloc_decoder_mem(image_size_fp, &tx_mem_cfg, &input_buffer_size_image);
    if(input_buf == NULL)
    {
        ESP_LOGE(TAG,"alloc input buf failed");
        fclose(image_fp);
        jpeg_del_decoder_engine(local_jpgd_handle);
        return;
    }
    fread(input_buf,1,input_buffer_size_image,image_fp);
    fclose(image_fp);
    
    err = jpeg_decoder_get_info(input_buf, input_buffer_size_image, &image_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get jpeg info, err=%d", err);
        free(input_buf);
        jpeg_del_decoder_engine(local_jpgd_handle);
        return;
    }
    
    ESP_LOGI(TAG,"image width = %d,image hight = %d",image_info.width,image_info.height);

   
    if(output_buf[img_cnt] == NULL)
    {
        ESP_LOGE(TAG,"alloc output buf failed");
        free(input_buf);
        jpeg_del_decoder_engine(local_jpgd_handle);
        return;
    }
    uint32_t out_size_image = 0;
    
    err = jpeg_decoder_process(local_jpgd_handle, &decode_cfg_rgb, input_buf, input_buffer_size_image, output_buf[img_cnt], output_buf_size[img_cnt], &out_size_image);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode jpeg image, err=%d", err);
        free(input_buf);
        jpeg_del_decoder_engine(local_jpgd_handle);
        return;
    }

    bsp_display_lock(0);
    // lv_img_dsc_t img_dsc = {
    //     .header ={
    //         .cf = LV_IMG_CF_TRUE_COLOR,
    //         .always_zero = 0,
    //         .reserved = 0,
    //         .w = image_info.width,
    //         .h = image_info.height,
    //     },
    //     .data_size = out_size_image,
    //     .data = output_buf,
    // };

    // lv_img_set_src(app_image_mian,&img_dsc);
    
    lv_canvas_set_buffer(app_image_mian, output_buf[img_cnt], image_info.width, image_info.height, LV_IMG_CF_TRUE_COLOR);
    // lv_refr_now(NULL);
    bsp_display_unlock();

    free(input_buf);
    jpeg_del_decoder_engine(local_jpgd_handle);

}

void AppImageDisplay::image_change_cb(lv_event_t *e)
{
    AppImageDisplay *img_dis = (AppImageDisplay *)e ->user_data;
    lv_event_code_t event = lv_event_get_code(e);
    if(event == LV_EVENT_GESTURE) {
        lv_indev_wait_release(lv_indev_get_act());
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir){
        case LV_DIR_LEFT:
        count_now ++;
        if(count_now > image_count-1)
            count_now = 0;
        printf("to left\n");
            break;
        case LV_DIR_RIGHT:
            count_now--;
            if(count_now < 0)
                count_now = image_count -1;
            // imganmitoright();
        printf("to right\n");
            break;
        }
        image_change_display(img_dis ->_image_file_iterator,count_now);
    }
}

void AppImageDisplay::image_delay_change(AppImageDisplay *app)
{
    xEventGroupWaitBits(image_event_group, IMAGE_EVENT_TASK_RUN, pdFALSE, pdTRUE, portMAX_DELAY);

    while (1)
    {
        if(xEventGroupGetBits(image_event_group) & IMAGE_EVENT_DIR)
        {
            xEventGroupClearBits(image_event_group,IMAGE_EVENT_DIR);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        count_now ++;
        if(count_now > image_count-1)
            count_now = 0;
        
        image_change_display(app ->_image_file_iterator,count_now);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (xEventGroupGetBits(image_event_group) & IMAGE_EVENT_DELETE) {

            ESP_LOGI(TAG, "Image Display detect task exit");
            break;
        }
        xEventGroupWaitBits(image_event_group, IMAGE_EVENT_TASK_RUN, pdFALSE, pdTRUE, portMAX_DELAY);
        
    }
    ESP_LOGI(TAG, "Image Display detect task exit");

    vTaskDelete(NULL);
}
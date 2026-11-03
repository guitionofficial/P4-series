/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "sdkconfig.h"

/* Example configurations */
#define EXAMPLE_RECV_BUF_SIZE   (2400)
#define EXAMPLE_SAMPLE_RATE     (44100)   
#define EXAMPLE_MCLK_MULTIPLE   (256)  
#define EXAMPLE_MCLK_FREQ_HZ    (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)

#ifndef CONFIG_EXAMPLE_VOICE_VOLUME
#define CONFIG_EXAMPLE_VOICE_VOLUME 60
#endif
#define EXAMPLE_VOICE_VOLUME    CONFIG_EXAMPLE_VOICE_VOLUME

#ifndef CONFIG_EXAMPLE_MIC_GAIN
#define CONFIG_EXAMPLE_MIC_GAIN 20
#endif
#define EXAMPLE_MIC_GAIN        CONFIG_EXAMPLE_MIC_GAIN

#ifndef CONFIG_EXAMPLE_PA_CTRL_IO
#define CONFIG_EXAMPLE_PA_CTRL_IO 11
#endif
#define EXAMPLE_PA_CTRL_IO      (CONFIG_EXAMPLE_PA_CTRL_IO >= 0 ? (gpio_num_t)CONFIG_EXAMPLE_PA_CTRL_IO : (gpio_num_t)(-1))

/* I2C port and GPIOs */
#define I2C_NUM         (1)
#define I2C_SCL_IO      GPIO_NUM_8
#define I2C_SDA_IO      GPIO_NUM_7

/* I2S port and GPIOs */
#define I2S_NUM         I2S_NUM_0
#define I2S_MCK_IO      GPIO_NUM_13
#define I2S_BCK_IO      GPIO_NUM_12    //
#define I2S_WS_IO       GPIO_NUM_10
#define I2S_DO_IO       GPIO_NUM_9
#define I2S_DI_IO       GPIO_NUM_48
#define BSP_POWER_AMP_IO      (GPIO_NUM_11)
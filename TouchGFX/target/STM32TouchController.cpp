/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : STM32TouchController.cpp
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN STM32TouchController */

#include <STM32TouchController.hpp>
#include "main.h"

extern "C" I2C_HandleTypeDef hi2c3;

#define FT5336_I2C_ADDR    0x70U    /* 7-bit address 0x38, HAL 8-bit form */
#define FT5336_TD_STATUS   0x02U    /* touch count + first point X/Y */

/* 1 = finger is down right now, 0 = released (used by screenView) */
extern "C" volatile uint8_t g_touch_pressed = 0;

void STM32TouchController::init()
{
}

bool STM32TouchController::sampleTouch(int32_t& x, int32_t& y)
{
    uint8_t buf[5];

    if (HAL_I2C_Mem_Read(&hi2c3, FT5336_I2C_ADDR, FT5336_TD_STATUS,
                         I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), 10) != HAL_OK)
    {
        g_touch_pressed = 0;
        return false;
    }

    if (buf[0] == 0xFFU)            /* chip returns 0xFF when idle */
    {
        g_touch_pressed = 0;
        return false;
    }

    if ((buf[0] & 0x0FU) == 0U)     /* no touch point */
    {
        g_touch_pressed = 0;
        return false;
    }

    const int32_t rawX = ((int32_t)(buf[1] & 0x0FU) << 8) | buf[2];
    const int32_t rawY = ((int32_t)(buf[3] & 0x0FU) << 8) | buf[4];

    /* FT5336 按面板原生竖屏方向上报: 长边(0..479)对应显示器 x, 短边(0..271)对应 y,
       因此需要交换两个轴才能与横屏画面坐标一致。 */
    x = rawY;
    y = rawX;

    g_touch_pressed = 1;
    return true;
}

/* USER CODE END STM32TouchController */

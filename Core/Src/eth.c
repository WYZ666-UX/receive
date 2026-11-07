/* ethernet.c - STM32F746G-DISCO 板 ETH RMII 引脚与时钟配置 (LAN8720A) */
#include "stm32f7xx_hal.h"

/*
    RMII_REF_CLK  -> PA1
    RMII_MDIO     -> PA2
    RMII_CRS_DV   -> PA7
    RMII_MDC      -> PC1
    RMII_RXD0     -> PC4
    RMII_RXD1     -> PC5
    RMII_RXER     -> PG2
    RMII_TX_EN    -> PG11
    RMII_TXD0     -> PG13
    RMII_TXD1     -> PG14

    该板 LAN8720A 的 nRST 由硬件上电复位电路处理, 不需要 MCU 控制。
    本工程使用轮询接收, 不使能 ETH 中断。
*/

void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    (void)heth;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Alternate = GPIO_AF11_ETH;

    /* PA1 / PA2 / PA7 */
    GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PC1 / PC4 / PC5 */
    GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* PG2 / PG11 / PG13 / PG14 */
    GPIO_InitStructure.Pin = GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);

    __HAL_RCC_ETH_CLK_ENABLE();
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef *heth)
{
    (void)heth;

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14);

    __HAL_RCC_ETH_CLK_DISABLE();
}

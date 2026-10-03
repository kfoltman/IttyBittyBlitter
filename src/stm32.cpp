#include "stm32.h"

volatile uint16_t *STM32FMC::lcd_ctl; 
volatile uint16_t *STM32FMC::lcd_data;
DMA_HandleTypeDef STM32FMCWithDMA::dma;
std::function<void()> STM32FMCWithDMA::dmaCallback;

void STM32FMC::initMPU(uint32_t mpu_region)
{
    // MPU region for the TFT
    HAL_MPU_Disable();
    MPU_Region_InitTypeDef MPU_InitStruct;
    /* TEX0, C0, B0 = Strongly ordered */
    MPU_InitStruct.Number           = mpu_region;
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = 0x60000000;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_256MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void STM32FMC::initPins(int dataAddrLine)
{
    // FMC pins
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    // Set the GPIO properties for FMC pins
    fsmcPinRange(GPIOD, 0, 1);  // D2, D3
    fsmcPinRange(GPIOD, 4, 5);  // NOE, NWE
    fsmcPinRange(GPIOD, 7, 10); // NE1, D13..D14
    fsmcPinRange(GPIOD, 14, 15);// D0, D1
    fsmcPinRange(GPIOE, 7, 15); // D4..D12

    switch(dataAddrLine)
    {
        case 16: fsmcPinRange(GPIOD, 11, 11); break;
        case 17: fsmcPinRange(GPIOD, 12, 12); break;
        case 18: fsmcPinRange(GPIOD, 13, 13); break;
        case 19: fsmcPinRange(GPIOE, 3, 3); break;
        case 20: fsmcPinRange(GPIOE, 4, 4); break;
        case 21: fsmcPinRange(GPIOE, 5, 5); break;
        case 22: fsmcPinRange(GPIOE, 6, 6); break;
    }
    
    lcd_ctl = reinterpret_cast<volatile uint16_t *>(0x60000000);
    lcd_data = reinterpret_cast<volatile uint16_t *>(0x60000000 + (2 << dataAddrLine));
}

void STM32FMC::initFMC()
{
    // FMC itself
    __HAL_RCC_FMC_CLK_ENABLE();
    // Program the FMC
    uint32_t bcr =  (1 << 31) |
                    (0 << 14) |  // EXTMOD (diffferent timings for R vs W)
                    (1 << 12) |  // write enable
                    (1 << 7) | // reserved
                    (1 << 4) | // 16 bit bus width
                    (0 << 2) | // NOR flash
                    1;

    // Those timings are OK for 480 MHz. They can be reduced substantially
    // for lower clock rates!
    uint32_t btr = (15 << 24) | (15 << 20) | (15 << 16) | (25 << 8) | (15 << 4) | (15 << 0);
    *(volatile uint32_t *)0x52004000 = bcr;
    *(volatile uint32_t *)0x52004004 = btr;
}

bool STM32FMCWithDMA::initDMA(DMA_Stream_TypeDef *stream, IRQn_Type irq)
{
    dma.Instance = stream;
    dma.Init.Request = DMA_REQUEST_MEM2MEM;
    dma.Init.Direction = DMA_MEMORY_TO_MEMORY;
    dma.Init.PeriphInc = DMA_PINC_ENABLE;
    dma.Init.MemInc = DMA_MINC_DISABLE; // Do not increment the output address
    dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    dma.Init.Mode = DMA_NORMAL;
    dma.Init.Priority = DMA_PRIORITY_LOW;
    dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    dma.Init.MemBurst = DMA_MBURST_SINGLE;
    dma.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&dma) != HAL_OK)
        return false;
    HAL_NVIC_SetPriority(irq, 0, 0);
    HAL_NVIC_EnableIRQ(irq);
    return true;
}

void STM32FMCWithDMA::pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback)
{
    dmaCallback.swap(endCallback);
    uint32_t addr = (uint32_t)src;
    addr &= ~31;
    uint32_t addr2 = ((uint32_t)(src + count)) + 31;
    addr2 &= ~31;
    __disable_irq();
    SCB_CleanDCache_by_Addr((void *)addr, addr2 - addr);
    __enable_irq();
    HAL_DMA_Start_IT(&dma, (uint32_t)src, (uint32_t)lcd_data, count);
}

void STM32FMCWithDMA::onDMAInterrupt()
{
    HAL_DMA_IRQHandler(&dma);
    std::function<void()> callback;
    swap(callback, dmaCallback);
    callback();
}

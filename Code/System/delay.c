#include "delay.h"

#define DELAY_SYSTICK_MAX_LOAD 0x00FFFFFFUL

static void Delay_Ticks(uint32_t ticks)
{
    if (ticks == 0U)
    {
        return;
    }

    SysTick->LOAD = ticks - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
    {
    }
    SysTick->CTRL = 0U;
}

void Delay_us(uint32_t us)
{
    uint32_t ticks_per_us;
    uint32_t max_us_per_chunk;
    uint32_t chunk_us;

    ticks_per_us = SystemCoreClock / 1000000U;
    if (ticks_per_us == 0U)
    {
        ticks_per_us = 1U;
    }

    max_us_per_chunk = DELAY_SYSTICK_MAX_LOAD / ticks_per_us;
    if (max_us_per_chunk == 0U)
    {
        max_us_per_chunk = 1U;
    }

    while (us > 0U)
    {
        chunk_us = (us > max_us_per_chunk) ? max_us_per_chunk : us;
        Delay_Ticks(chunk_us * ticks_per_us);
        us -= chunk_us;
    }
}

void Delay_ms(uint32_t ms)
{
    while (ms > 0U)
    {
        Delay_us(1000U);
        ms--;
    }
}

void Delay_s(uint32_t s)
{
    while (s > 0U)
    {
        Delay_ms(1000U);
        s--;
    }
}

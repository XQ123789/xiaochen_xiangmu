/**
 * @file    ng_timer.c
 * @brief   计时服务模块实现 - 基于TIM2的毫秒计数器
 * @note    原项目delay.c使用SysTick轮询方式实现延时，为避免冲突，
 *          本模块使用TIM2定时器产生1ms中断作为系统毫秒计数器。
 *          阻塞式延时仍复用原delay_ms()，保证兼容性。
 *
 * 硬件连接：
 *   - TIM2 内部时钟，1ms中断
 *   - 系统时钟72MHz，预分频7199 -> 10kHz计数频率 -> 重装值9 -> 1ms中断
 *
 * 使用方法：
 *   1. 主循环中调用 ng_timer_init() 初始化
 *   2. 在 stm32f10x_it.c 的 TIM2_IRQHandler 中调用 ng_timer_tick()
 *   3. 使用 ng_timer_millis() 获取当前时间
 *   4. 使用 ng_timer_expired() 进行非阻塞超时检查
 */

#include "ng_timer.h"
#include "ng_config.h"

/* STM32标准外设库头文件 */
#include "stm32f10x.h"

/* 原项目延时函数头文件，用于阻塞式延时 */
#include "delay.h"

/* ====== 模块内部变量 ====== */

/* 毫秒计数器，volatile防止编译器优化掉中断中的修改 */
static volatile uint32_t tim2_tick_ms = 0;

/**
 * @brief  初始化TIM2定时器，产生1ms周期中断
 * @note   时钟配置：
 *         - STM32F103系统时钟 = 72MHz
 *         - TIM2挂在APB1总线上，定时器时钟 = 72MHz（APB1分频为1时）或144MHz（APB1分频>1时）
 *         - 标准库中APB1分频为2，所以TIM2时钟 = 72MHz
 *         - 预分频器: 7200-1 = 7199 -> 计数频率 = 72MHz/7200 = 10kHz
 *         - 重装值:   10-1 = 9   -> 中断频率 = 10kHz/10 = 1kHz = 1ms
 */
void ng_timer_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 使能TIM2时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* TIM2基本配置：预分频7199，重装值9 -> 1ms中断 */
    TIM_TimeBaseStructure.TIM_Period = 10 - 1;           /* 重装值：10个计数周期 */
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;      /* 预分频：72MHz/7200=10kHz */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* 使能TIM2更新中断（溢出中断） */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* 配置NVIC中断 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  /* 抢占优先级1 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         /* 子优先级0 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 清零计数器并启动TIM2 */
    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);

    /* 初始化时清零毫秒计数 */
    tim2_tick_ms = 0;
}

/**
 * @brief  TIM2中断服务函数中调用此函数，递增毫秒计数器
 * @note   需要在 stm32f10x_it.c 的 TIM2_IRQHandler 中添加：
 *         #include "ng_timer.h"
 *         void TIM2_IRQHandler(void) {
 *             if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
 *                 ng_timer_tick();
 *                 TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
 *             }
 *         }
 */
void ng_timer_tick(void)
{
    tim2_tick_ms++;
}

/**
 * @brief  获取系统启动以来的毫秒数
 * @return 毫秒计数值（32位，约49.7天溢出一次）
 */
uint32_t ng_timer_millis(void)
{
    return tim2_tick_ms;
}

/**
 * @brief  非阻塞超时检查
 * @param  start       起始时间戳（由ng_timer_millis()获取）
 * @param  interval_ms 需要等待的毫秒数
 * @return 1=已超时, 0=未超时
 * @note   使用差值比较，天然处理32位溢出问题：
 *         即使tim2_tick_ms溢出回绕，(当前值-起始值)仍然正确
 *         例如：start=0xFFFFFF00, interval=512, 溢出后current=0x00000100
 *         差值 = 0x00000100 - 0xFFFFFF00 = 0x200 = 512，结果正确
 */
int ng_timer_expired(uint32_t start, uint32_t interval_ms)
{
    uint32_t now = tim2_tick_ms;
    /* 差值比较法，自动处理溢出 */
    if ((now - start) >= interval_ms) {
        return 1;  /* 已超时 */
    }
    return 0;      /* 未超时 */
}

/**
 * @brief  阻塞式毫秒延时，内部调用原项目的delay_ms()
 * @param  ms 延时毫秒数
 * @note   原delay.c使用SysTick轮询方式，最大延时约1864ms（72MHz下）
 *         此函数直接复用，保证与原有代码的兼容性
 *         如需延时超过1864ms，需分段调用
 */
void ng_timer_delay_ms(uint32_t ms)
{
    /* 原delay_ms参数为u16(最大65535)，但实际受SysTick 24位限制约1864ms */
    while (ms > 1000) {
        delay_ms(1000);
        ms -= 1000;
    }
    if (ms > 0) {
        delay_ms((u16)ms);
    }
}

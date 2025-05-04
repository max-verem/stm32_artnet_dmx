#ifndef DMX_ENCODER_H
#define DMX_ENCODER_H

#include "stm32f4xx_hal.h"

#include "dmx.h"

extern uint8_t *dmx_encoder_buffer, *dmx_encoder_buffer_bak;
void dmx_encoder_init(TIM_HandleTypeDef* tim, GPIO_TypeDef *break_output_gpio, uint32_t break_output_pin, int break_freq_scale, UART_HandleTypeDef *huart);

#endif /* DMX_ENCODER_H */

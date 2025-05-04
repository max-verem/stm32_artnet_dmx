#include "dmx_encoder.h"

typedef struct dmx_encoder_desc
{
	TIM_HandleTypeDef* tim;
	GPIO_TypeDef *gpio;
	uint32_t pin;
	int break_freq_scale;
	UART_HandleTypeDef *uart;
} dmx_encoder_t;

static dmx_encoder_t instance;

#define DMX_SLOT_DUR 	44
#define DMX_BREAK_IDLE_CNT 10
#define DMX_BREAK_IDLE	(DMX_BREAK_IDLE_CNT * DMX_SLOT_DUR)
#define DMX_BREAK_BREAK	100
#define DMX_BREAK_MAB	12

static uint32_t preambula_buffer[DMX_BREAK_IDLE + DMX_BREAK_BREAK + DMX_BREAK_MAB];
static uint8_t dmx_encoder_buffers[2][DMX_BUF_SIZE];
uint8_t *dmx_encoder_buffer = dmx_encoder_buffers[0];
uint8_t *dmx_encoder_buffer_bak = dmx_encoder_buffers[1];


static void preambula_dma_restart()
{
	HAL_DMA_Start_IT(
		instance.tim->hdma[TIM_DMA_ID_UPDATE],
		(uint32_t)preambula_buffer,
		(uint32_t)&(instance.gpio->BSRR), (DMX_BREAK_IDLE + DMX_BREAK_BREAK + DMX_BREAK_MAB) / instance.break_freq_scale);
	__HAL_TIM_ENABLE_DMA(instance.tim, TIM_DMA_UPDATE ); 	//Enable the TIM Update DMA request
	__HAL_TIM_ENABLE(instance.tim);                 		//Enable the Peripheral
}

static void preambula_dma_cb_full(DMA_HandleTypeDef *hdma)
{
	/* send uart buffer */
	HAL_UART_Transmit_DMA(instance.uart, dmx_encoder_buffer, DMX_BUF_SIZE);
};

static void buffer_sent_cb(UART_HandleTypeDef *uart)
{
	preambula_dma_restart();
};

void dmx_encoder_init(TIM_HandleTypeDef* tim, GPIO_TypeDef *gpio, uint32_t pin, int break_freq_scale, UART_HandleTypeDef *uart)
{
	int i;
	uint32_t *tmp;

	/* setup instance */
	instance.tim = tim;
	instance.gpio = gpio;
	instance.pin = pin;
	instance.break_freq_scale = break_freq_scale;
	instance.uart = uart;

	/* init preambula buffer */
	tmp = preambula_buffer;
	for(i = 0; i < DMX_BREAK_IDLE / break_freq_scale; i++, tmp++)
		*tmp = pin << 0;
	for(i = 0; i < DMX_BREAK_BREAK / break_freq_scale; i++, tmp++)
		*tmp = pin << 16;
	for(i = 0; i < DMX_BREAK_MAB / break_freq_scale; i++, tmp++)
		*tmp = pin << 0;

	/* init callbacks */
	HAL_DMA_RegisterCallback(tim->hdma[TIM_DMA_ID_UPDATE], HAL_DMA_XFER_CPLT_CB_ID, preambula_dma_cb_full);
	HAL_UART_RegisterCallback(uart, HAL_UART_TX_COMPLETE_CB_ID, buffer_sent_cb);

	/* start transfer */
	preambula_dma_restart();
}

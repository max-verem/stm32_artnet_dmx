#include <stdio.h>

#include "main.h"
#include "artnet.h"
#include "W5500_regs.h"
#include "settings.h"

static SPI_HandleTypeDef* W5500_SPI;

#define W5500_SCS_HIGH \
	HAL_GPIO_WritePin(W5500_SCS_GPIO_Port, W5500_SCS_Pin, GPIO_PIN_SET)

#define W5500_SCS_LOW \
	HAL_GPIO_WritePin(W5500_SCS_GPIO_Port, W5500_SCS_Pin, GPIO_PIN_RESET)

#define W5500_RST_HIGH \
	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET)

#define W5500_RST_LOW \
	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET)

#define RX_PACKETS_POOL 4
static uint8_t tx_buf[16] = {0xFF}, rx_buf[16] = {0xFF},
	*rx_packets, rx_packets_pool[RX_PACKETS_POOL][2048];
static uint32_t rx_packets_idx = 0;
uint32_t artnet_OpPollReply_cnt = 1, artnet_OpDmx_cnt = 0;
static uint8_t artnet_OpPollReply_buf[512];
static uint32_t artnet_OpPollReply_len;
static const uint32_t artnet_OpPollReply_cnt_offset = 118;
int (*artnet_OpDmx_cb)(int idx, int len, uint8_t *buf) = NULL;

static uint8_t* dmx_buf[2] = {NULL, NULL};
static uint32_t dmx_len[2] = {0, 0};
static int dmx_data(unsigned char* buf, int len, int idx)
{
	if(idx == 0 || idx == 1)
	{
		dmx_buf[idx] = buf;
		dmx_len[idx] = len;
	};

	return 0;
};

static void W5500_dma_compleate_cb(SPI_HandleTypeDef *hspi);
static void W5500_dma_error_cb(SPI_HandleTypeDef *hspi);

int W5500_init(SPI_HandleTypeDef* hspi)
{
	uint8_t *tx;

	rx_packets = rx_packets_pool[rx_packets_idx];

	W5500_SPI = hspi;

	HAL_SPI_RegisterCallback(W5500_SPI, HAL_SPI_TX_RX_COMPLETE_CB_ID, W5500_dma_compleate_cb);
	HAL_SPI_RegisterCallback(W5500_SPI, HAL_SPI_ERROR_CB_ID, W5500_dma_error_cb);

	/* W5500 reset cycle */
	HAL_Delay(10);
	W5500_RST_LOW;
	HAL_Delay(10);
	W5500_RST_HIGH;
	HAL_Delay(10);

	// read version, should be 0x04
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(W5500_CR_VERSIONR, W5500_CR_BLOCK));
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
	W5500_SCS_HIGH;

	// setup Source Hardware Address
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_CR_SHAR, W5500_CR_BLOCK));
	tx = tx_buf + 3;
	*tx++ = settings_current->mac_address[0];
	*tx++ = settings_current->mac_address[1];
	*tx++ = settings_current->mac_address[2];
	*tx++ = settings_current->mac_address[3];
	*tx++ = settings_current->mac_address[4];
	*tx++ = settings_current->mac_address[5];
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 9, 5);
	W5500_SCS_HIGH;

	// setup Source IP Address
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_CR_SIPR, W5500_CR_BLOCK));
	tx = tx_buf + 3;
	*tx++ = settings_current->ip_address[0];
	*tx++ = settings_current->ip_address[1];
	*tx++ = settings_current->ip_address[2];
	*tx++ = settings_current->ip_address[3];
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 7, 5);
	W5500_SCS_HIGH;

	// setup Subnet Mask Address
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_CR_SUBR, W5500_CR_BLOCK));
	tx = tx_buf + 3;
	*tx++ = settings_current->ip_mask[0];
	*tx++ = settings_current->ip_mask[1];
	*tx++ = settings_current->ip_mask[2];
	*tx++ = settings_current->ip_mask[3];
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 7, 5);
	W5500_SCS_HIGH;

	// setup socket 0: mode
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_MR, W5500_SnR_BLOCK(0)));
	tx_buf[3] = 2; // UDP
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
	W5500_SCS_HIGH;

	// setup socket 0: port
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_PORT, W5500_SnR_BLOCK(0)));
	tx_buf[3] = (ARTNET_UDP_PORT >> 8) & 0xFF;
	tx_buf[4] = (ARTNET_UDP_PORT >> 0) & 0xFF;
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 5, 5);
	W5500_SCS_HIGH;

	// setup socket 0: open
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_CR, W5500_SnR_BLOCK(0)));
	tx_buf[3] = 0x01; // OPEN
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
	W5500_SCS_HIGH;

	// wait until command executed
	for(;;)
	{
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(W5500_Sn_CR, W5500_SnR_BLOCK(0)));
		W5500_SCS_LOW;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
		W5500_SCS_HIGH;

		if(!rx_buf[3])
			break;

		HAL_Delay(1);
	}

	// wait until socket in closed state
	for(;;)
	{
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(W5500_Sn_SR, W5500_SnR_BLOCK(0)));
		W5500_SCS_LOW;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
		W5500_SCS_HIGH;

		if(rx_buf[3] == 0x22 /* SOCK_UDP */)
			break;

		HAL_Delay(1);
	}

	/* compose pollReply packet */
	artnet_OpPollReply_len = artnet_compose_OpPollReply(artnet_OpPollReply_buf + 3);

	return 0;
};

volatile static uint32_t Sn_RX_RSR = 0, Sn_RX_RD = 0;
volatile static uint32_t Sn_TX_FSR, Sn_TX_RD, Sn_TX_WR;
volatile static uint32_t tx_packet_run = 0, tx_packet_done = 0;
volatile static uint32_t rx_packet_run = 0, rx_packet_done = 0;

static void W5500_dma_error_cb(SPI_HandleTypeDef *hspi)
{
	tx_packet_run = 0, tx_packet_done = 0;
	rx_packet_run = 0, rx_packet_done = 0;
};

static void W5500_dma_compleate_cb(SPI_HandleTypeDef *hspi)
{
	W5500_SCS_HIGH;

	if(tx_packet_run)
	{
		tx_packet_run = 0;
		tx_packet_done = 1;
	};

	if(rx_packet_run)
	{
		rx_packet_run = 0;
		rx_packet_done = 1;
	}
}

static void _artnet_OpPollReply_cnt_update()
{
	uint32_t cnt = artnet_OpPollReply_cnt;

	artnet_OpPollReply_buf[artnet_OpPollReply_cnt_offset + 3] = (cnt % 10) + 0x30;
	cnt /= 10;
	artnet_OpPollReply_buf[artnet_OpPollReply_cnt_offset + 2] = (cnt % 10) + 0x30;
	cnt /= 10;
	artnet_OpPollReply_buf[artnet_OpPollReply_cnt_offset + 1] = (cnt % 10) + 0x30;
	cnt /= 10;
	artnet_OpPollReply_buf[artnet_OpPollReply_cnt_offset + 0] = (cnt % 10) + 0x30;
};

int W5500_idle()
{
	/* wait for packet transfer compleates */
	if(tx_packet_run || rx_packet_run)
		return 0;

	/* check if sending packet finished */
	if(tx_packet_done)
	{
		tx_packet_done = 0;

		// update Sn_TX_WR
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_TX_WR, W5500_SnR_BLOCK(0)));
		W5500_SCS_LOW;
		tx_buf[3] = (Sn_TX_WR >> 8) & 0x00FF;
		tx_buf[4] = (Sn_TX_WR >> 0) & 0x00FF;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 4, 5);
		W5500_SCS_HIGH;

		// send SEND command
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_CR, W5500_SnR_BLOCK(0)));
		tx_buf[3] = 0x20; // SEND
		W5500_SCS_LOW;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
		W5500_SCS_HIGH;

		return 0;
	};

	/* check if reading packet finished */
	if(rx_packet_done)
	{
		uint32_t o;
    	uint8_t *artnet_OpPoll_ptr = NULL;

		rx_packet_done = 0;

		// update Sn_RX_RD
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_RX_RD, W5500_SnR_BLOCK(0)));
		W5500_SCS_LOW;
		Sn_RX_RD += Sn_RX_RSR;
		tx_buf[3] = (Sn_RX_RD >> 8) & 0x00FF;
		tx_buf[4] = (Sn_RX_RD >> 0) & 0x00FF;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 4, 5);
		W5500_SCS_HIGH;

		// send RECV command
		W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_CR, W5500_SnR_BLOCK(0)));
		tx_buf[3] = 0x40; // RECV
		W5500_SCS_LOW;
		HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 4, 5);
		W5500_SCS_HIGH;

		// process packets
        for(o = 3; o < (3 + Sn_RX_RSR);)
        {
        	int r;
        	uint32_t l;

        	l = rx_packets[o + 6];
        	l <<= 8;
        	l |= rx_packets[o + 7];

            r = artnet_parse_opcode(rx_packets + o + 8, l);
            if(r == artnet_OpPoll)
            {
            	artnet_OpPoll_ptr = rx_packets + o + 8;
            }
            else if(r == artnet_OpDmx)
            {
            	artnet_process_OpDmx(rx_packets + o + 8, dmx_data);
            };

        	o += 8 + l;
        }

        if(dmx_len[0] || dmx_len[1])
        {
        	int e = 0;

        	if(dmx_len[0] && artnet_OpDmx_cb)
        		e += artnet_OpDmx_cb(0, dmx_len[0], dmx_buf[0]);

        	if(dmx_len[1] && artnet_OpDmx_cb)
        		e += artnet_OpDmx_cb(1, dmx_len[1], dmx_buf[1]);

        	if(e)
        	{
        		rx_packets_idx++;
        		rx_packets_idx = rx_packets_idx % RX_PACKETS_POOL;
        		rx_packets = rx_packets_pool[rx_packets_idx];
        	};

    		artnet_OpDmx_cnt++;

            dmx_len[0] = dmx_len[1] = 0;
        };

        if(artnet_OpPoll_ptr) // send PollReply packet
        {
        	uint32_t tx = artnet_OpPollReply_len;
        	uint8_t* tx_packets = artnet_OpPollReply_buf;

        	/* update NodeReport counter */
        	artnet_OpPollReply_cnt++;
        	_artnet_OpPollReply_cnt_update();

        	// remote_ip
        	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_DIPR, W5500_SnR_BLOCK(0)));
        	tx_buf[3 + 0] = settings_current->ip_address[0] | (settings_current->ip_mask[0] ^ 0xFF);
        	tx_buf[3 + 1] = settings_current->ip_address[1] | (settings_current->ip_mask[1] ^ 0xFF);
        	tx_buf[3 + 2] = settings_current->ip_address[2] | (settings_current->ip_mask[2] ^ 0xFF);
        	tx_buf[3 + 3] = settings_current->ip_address[3] | (settings_current->ip_mask[3] ^ 0xFF);
        	W5500_SCS_LOW;
        	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 4, 5);
        	W5500_SCS_HIGH;

        	// remote_port
        	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_W(W5500_Sn_DPORT, W5500_SnR_BLOCK(0)));
        	tx_buf[3 + 0] = (ARTNET_UDP_PORT >> 8) & 0x00FF;
        	tx_buf[3 + 1] = (ARTNET_UDP_PORT >> 0) & 0x00FF;
        	W5500_SCS_LOW;
        	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 2, 5);
        	W5500_SCS_HIGH;

        	// Sn_TX_FSR, Sn_TX_RD, Sn_TX_WR
        	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(W5500_Sn_TX_FSR, W5500_SnR_BLOCK(0)));
        	W5500_SCS_LOW;
        	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 6, 5);
        	W5500_SCS_HIGH;

        	Sn_TX_FSR = Sn_TX_RD = Sn_TX_WR = 0;

        	Sn_TX_FSR |= rx_buf[3];
        	Sn_TX_FSR <<= 8;
        	Sn_TX_FSR |= rx_buf[4];

        	Sn_TX_RD |= rx_buf[5];
        	Sn_TX_RD <<= 8;
        	Sn_TX_RD |= rx_buf[6];

        	Sn_TX_WR |= rx_buf[7];
        	Sn_TX_WR <<= 8;
        	Sn_TX_WR |= rx_buf[8];

        	tx_packet_run = 1;

        	// write packet buffer
        	W5500_PUT_CONTROL(tx_packets, W5500_CONTROL_W(Sn_TX_WR, W5500_SnTX_BLOCK(0)));
        	W5500_SCS_LOW;
        	Sn_TX_WR += tx;
        	HAL_SPI_TransmitReceive_DMA(W5500_SPI, tx_packets, rx_packets, 3 + tx);
        };

		return 0;
	}

	Sn_RX_RSR = 0;
	Sn_RX_RD = 0;

	// Sn_RX_RSR , Sn_RX_RD
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(W5500_Sn_RX_RSR, W5500_SnR_BLOCK(0)));
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive(W5500_SPI, tx_buf, rx_buf, 3 + 4, 5);
	W5500_SCS_HIGH;

	Sn_RX_RSR |= rx_buf[3];
	Sn_RX_RSR <<= 8;
	Sn_RX_RSR |= rx_buf[4];

	Sn_RX_RD |= rx_buf[5];
	Sn_RX_RD <<= 8;
	Sn_RX_RD |= rx_buf[6];

	/* check if we need to read buffer */
	if(!Sn_RX_RSR)
		return 0;

	rx_packet_run = 1;

	// read buffer
	W5500_PUT_CONTROL(tx_buf, W5500_CONTROL_R(Sn_RX_RD, W5500_SnRX_BLOCK(0)));
	W5500_SCS_LOW;
	HAL_SPI_TransmitReceive_DMA(W5500_SPI, tx_buf, rx_packets, 3 + Sn_RX_RSR);

	return 0;
};

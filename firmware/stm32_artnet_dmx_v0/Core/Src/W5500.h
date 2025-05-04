#ifndef W5500_H
#define W5500_H

extern uint32_t artnet_OpPollReply_cnt, artnet_OpDmx_cnt;
extern int (*artnet_OpDmx_cb)(int idx, int len, uint8_t *buf);

int W5500_idle();
int W5500_init(SPI_HandleTypeDef* hspi);

#endif


#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

#define _VersInfoH 1
#define _VersInfoL 33

typedef struct settings_desc
{
	uint32_t sig1;
	uint8_t VersInfoH;
	uint8_t VersInfoL;
	uint8_t net_switch;
	uint8_t ip_address[4];
	uint8_t ip_mask[4];
	uint8_t mac_address[6];
	uint8_t OemHi;
	uint8_t OemLo;
	uint8_t EstaManLo;
	uint8_t EstaManHi;
	uint32_t PortAddress;
	uint32_t sig2;
} settings_t;

extern settings_t *settings_current;

int settings_init();
int settings_save();

#endif

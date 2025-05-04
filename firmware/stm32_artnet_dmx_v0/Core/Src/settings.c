#include "settings.h"
#include "main.h"

settings_t *settings_current = NULL;

// https://community.st.com/t5/stm32-mcus-products/stm32f401-reserving-flash-sector-for-config/td-p/639952
static const settings_t __attribute__((section (".settings_storage"))) settings_saved =
{
	.VersInfoH = _VersInfoH,
	.VersInfoL = _VersInfoL,
	.ip_address = {10, 1, 5, 62},
	.ip_mask = {255, 255, 0, 0},
	.mac_address = {0x00, 0x03, 0x47, 0xca, 0x8a, 0x54},
    .OemHi = 0xDE,
    .OemLo = 0xAD,
    .EstaManHi = 0xBE,
    .EstaManLo = 0xEF,
	.PortAddress = 0x0120,
	.sig1 = 0xEFBEADDE,
	.sig2 = 0xEFBEADDE
};

static unsigned char settings_buffer[sizeof(settings_t) + sizeof(uint32_t)];

int settings_init()
{
	uint32_t
		*dst = (uint32_t*)settings_buffer,
		*src = (uint32_t*)&settings_saved,
		cnt = (sizeof(settings_t) + sizeof(uint32_t) - 1) / sizeof(uint32_t);

	while(cnt)
	{
		*dst++ = *src++;
		cnt--;
	};

	settings_current = (settings_t*)settings_buffer;

	return 0;
}

int settings_save()
{
	uint32_t *src, *dst, cnt;
#if 0
	uint32_t SectorError;
	FLASH_EraseInitTypeDef flash_erase =
	{
		.TypeErase = FLASH_TYPEERASE_SECTORS,
		.Sector = 1, // we use second sector
		.NbSectors = 1
	};
#endif
	HAL_FLASH_Unlock();
#if 0
	HAL_FLASHEx_Erase(&flash_erase, &SectorError);
#else
	FLASH_Erase_Sector(1, FLASH_VOLTAGE_RANGE_3);
#endif

	src = (uint32_t*)settings_buffer;
	dst = (uint32_t*)&settings_saved;
	cnt = (sizeof(settings_t) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
	while(cnt)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)dst, *src);
		dst++; src++; cnt--;
		FLASH_FlushCaches();
	}

	HAL_FLASH_Lock();

	return 0;
}

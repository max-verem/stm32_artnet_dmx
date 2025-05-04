#include "cli.h"
#include "settings.h"
#include "artnet.h"
#include "usbd_cdc_if.h"

#define CLI_RECV_BUFFER_SIZE	1024
#define CLI_SEND_BUFFER_SIZE	1024
#define CLI_LINE_BUFFER_SIZE	64
static uint8_t
	cli_recv_buffer[CLI_RECV_BUFFER_SIZE],
	cli_send_buffer[CLI_SEND_BUFFER_SIZE],
	cli_line_buffer[CLI_LINE_BUFFER_SIZE];
volatile static uint32_t
	cli_recv_head = 0,
	cli_recv_tail = 0,
	cli_send_head = 0,
	cli_send_curr = 0, // index of not sent characters
	cli_send_tail = 0, // next available index to store character to send
	cli_line_size = 0;

static void cli_callback_recv_data(uint8_t* buf, uint32_t len)
{
	int i;

	for(i = 0; i < len; i++, cli_recv_tail++)
		cli_recv_buffer[cli_recv_tail % CLI_RECV_BUFFER_SIZE] = buf[i];
};

static void cli_callback_sent_data(uint8_t* buf, uint32_t len)
{
	cli_send_head += len;
};

#define CLI_OUTPUT_CHAR(C) 										\
{																\
	cli_send_buffer[cli_send_tail % CLI_RECV_BUFFER_SIZE] = C;	\
	cli_send_tail++;											\
}

#define CLI_OUTPUT_NIBBLE_HEX(C)								\
	CLI_OUTPUT_CHAR((C < 10) ? ('0' + C) : ('A' + C - 10))

#define CLI_OUTPUT_BYTE_HEX(C)									\
	CLI_OUTPUT_NIBBLE_HEX(((C >> 4) & 0x0F));					\
	CLI_OUTPUT_NIBBLE_HEX(((C >> 0) & 0x0F));					\

#define CLI_OUTPUT_BYTE_DEC(C)									\
	if(!C)														\
	{															\
		CLI_OUTPUT_CHAR('0');									\
	}															\
	else														\
	{															\
		uint32_t hex = C, bcd = 0;								\
		while(hex)												\
		{														\
			bcd <<= 4;											\
			bcd |= (hex % 10) + 1;								\
			hex /= 10;											\
		};														\
		for(; bcd; bcd >>= 4)									\
			CLI_OUTPUT_NIBBLE_HEX(((bcd & 0x0F) - 1));			\
	}

#define CLI_OUTPUT_NL 											\
	CLI_OUTPUT_CHAR('\r');										\
	CLI_OUTPUT_CHAR('\n')

#define CLI_OUTPUT_CHARS(CHARS, CNT)							\
{																\
	int i;														\
	for(i = 0; i < CNT; i++)									\
		CLI_OUTPUT_CHAR(CHARS[i]);								\
}

#define CLI_OUTPUT_MSG(MSG) CLI_OUTPUT_CHARS(MSG, sizeof(MSG))

const static uint8_t cli_msg_help[] =
"\r\n"
"# Welcome to STM32-ARTNET-DMX encoder!\r\n"
"# Commands:\r\n"
"#     info                - display current settings\r\n"
"#     mac_address <mac>   - setup mac address for ethernet interface (01:02:03:04:05:06)\r\n"
"#     net_switch <0|1>    - if no IP address set, this params defines network to be used 2.X.Y.Z (0) or 10.X.Y.Z (1)\r\n"
"#     ip_address <ip>     - setup ip address for ethernet interface (100.100.100.100)\r\n"
"#     ip_mask <mask>      - setup ip mask for ethernet interface (255.255.0.0)\r\n"
"#     oem <XXXX>          - oem code (16-bit, hex: DEAD)\r\n"
"#     estaman <XXXX>      - ESTA Manufacturer code (16-bit, hex: BEEF)\r\n"
"#     port_address <XXXX> - Net[7bits]/Sub-Net[4bits]/0[4bits] value (16-bit, hex: 0000)\r\n"
;

const static uint8_t cli_msg_parse_error[] =
"# ERROR! Failed to parse command and/or paramter!\r\n"
;

const static uint8_t cli_msg_prompt[] =
"> "
;

const static uint8_t cli_msg_param_updated[] =
"# OK: Parameter updated\r\n"
;

#define LTC_CHAR_IS_DIGIT(c) ((c >= '0') && (c <= '9'))
#define LTC_LINE_CHAR_IS_DIGIT(IDX) (LTC_CHAR_IS_DIGIT(cli_line_buffer[IDX]))

#define CHAR_IS_DIGIT(c) ((c >= '0') && (c <= '9'))
#define CHAR_IS_HEX(c) (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f')))
#define CHAR_HEX_VAL(c) (((c >= '0') && (c <= '9')) ? (c - '0') : ((c >= 'a') && (c <= 'f')) ? (c - 'a' + 10) : ((c >= 'A') && (c <= 'F')) ? (c - 'A' + 10) : 0)
#define CHAR_DEC_VAL(c) (((c >= '0') && (c <= '9')) ? (c - '0') : 0)

static void cli_line_process()
{
	int e = 0;

	if(cli_line_size == 0)
	{
		CLI_OUTPUT_MSG(cli_msg_help)
	}
	// info
	else if(cli_line_size == 4
	    && cli_line_buffer[0] == 'i'
		&& cli_line_buffer[1] == 'n'
		&& cli_line_buffer[2] == 'f'
		&& cli_line_buffer[3] == 'o')
	{
		CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# Product     : ");
			CLI_OUTPUT_MSG(LongName);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# Version     : ");
			CLI_OUTPUT_BYTE_DEC(settings_current->VersInfoH);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->VersInfoL);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("#--------------------------------\r\n");

		CLI_OUTPUT_MSG("# mac_address : ");
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[0]);
			CLI_OUTPUT_CHAR(':');
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[1]);
			CLI_OUTPUT_CHAR(':');
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[2]);
			CLI_OUTPUT_CHAR(':');
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[3]);
			CLI_OUTPUT_CHAR(':');
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[4]);
			CLI_OUTPUT_CHAR(':');
			CLI_OUTPUT_BYTE_HEX(settings_current->mac_address[5]);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# net_switch  : ");
			CLI_OUTPUT_NIBBLE_HEX(settings_current->net_switch);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# ip_address  : ");
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_address[0]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_address[1]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_address[2]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_address[3]);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# ip_mask     : ");
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_mask[0]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_mask[1]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_mask[2]);
			CLI_OUTPUT_CHAR('.');
			CLI_OUTPUT_BYTE_DEC(settings_current->ip_mask[3]);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# oem         : ");
			CLI_OUTPUT_BYTE_HEX(settings_current->OemHi);
			CLI_OUTPUT_BYTE_HEX(settings_current->OemLo);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# estaman     : ");
			CLI_OUTPUT_BYTE_HEX(settings_current->EstaManHi);
			CLI_OUTPUT_BYTE_HEX(settings_current->EstaManLo);
			CLI_OUTPUT_NL;

		CLI_OUTPUT_MSG("# port_address: ");
			CLI_OUTPUT_BYTE_HEX((settings_current->PortAddress >> 8));
			CLI_OUTPUT_BYTE_HEX((settings_current->PortAddress & 0x00FF));
			CLI_OUTPUT_NL;

		CLI_OUTPUT_NL;
	}

	// ip_address A.B.C.D
	else if(cli_line_size >= 18
		&& cli_line_buffer[0] == 'i'
		&& cli_line_buffer[1] == 'p'
		&& cli_line_buffer[2] == '_'
		&& cli_line_buffer[3] == 'a'
		&& cli_line_buffer[4] == 'd'
		&& cli_line_buffer[5] == 'd'
		&& cli_line_buffer[6] == 'r'
		&& cli_line_buffer[7] == 'e'
		&& cli_line_buffer[8] == 's'
		&& cli_line_buffer[9] == 's'
		&& cli_line_buffer[10] == ' ')
	{
		int i, j;
		unsigned char ip[4] = {0};

		for(i = 11, j = 0; i < cli_line_size && j < 4; i++)
		{
			if(cli_line_buffer[i] == '.')
				j++;
			else if(CHAR_IS_DIGIT(cli_line_buffer[i]))
			{
				ip[j] *= 10;
				ip[j] += CHAR_DEC_VAL(cli_line_buffer[i]);
			}
			else
			{
				e = 1;
				break;
			};
		};

		if(!e)
		{
			settings_current->ip_address[0] = ip[0];
			settings_current->ip_address[1] = ip[1];
			settings_current->ip_address[2] = ip[2];
			settings_current->ip_address[3] = ip[3];
			settings_save();
			CLI_OUTPUT_MSG(cli_msg_param_updated);
		};
	}

	// ip_mask A.B.C.D
	else if(cli_line_size >= 15
		&& cli_line_buffer[0] == 'i'
		&& cli_line_buffer[1] == 'p'
		&& cli_line_buffer[2] == '_'
		&& cli_line_buffer[3] == 'm'
		&& cli_line_buffer[4] == 'a'
		&& cli_line_buffer[5] == 's'
		&& cli_line_buffer[6] == 'k'
		&& cli_line_buffer[7] == ' ')
	{
		int i, j;
		unsigned char ip[4] = {0};

		for(i = 8, j = 0; i < cli_line_size && j < 4; i++)
		{
			if(cli_line_buffer[i] == '.')
				j++;
			else if(CHAR_IS_DIGIT(cli_line_buffer[i]))
			{
				ip[j] *= 10;
				ip[j] += CHAR_DEC_VAL(cli_line_buffer[i]);
			}
			else
			{
				e = 1;
				break;
			};
		};

		if(!e)
		{
			settings_current->ip_mask[0] = ip[0];
			settings_current->ip_mask[1] = ip[1];
			settings_current->ip_mask[2] = ip[2];
			settings_current->ip_mask[3] = ip[3];
			settings_save();
			CLI_OUTPUT_MSG(cli_msg_param_updated);
		};
	}

	// port_address HHHH
	else if(cli_line_size == 17
		&& cli_line_buffer[0] == 'p'
		&& cli_line_buffer[1] == 'o'
		&& cli_line_buffer[2] == 'r'
		&& cli_line_buffer[3] == 't'
		&& cli_line_buffer[4] == '_'
		&& cli_line_buffer[5] == 'a'
		&& cli_line_buffer[6] == 'd'
		&& cli_line_buffer[7] == 'd'
		&& cli_line_buffer[8] == 'r'
		&& cli_line_buffer[9] == 'e'
		&& cli_line_buffer[10] == 's'
		&& cli_line_buffer[11] == 's'
		&& cli_line_buffer[12] == ' '
		&& CHAR_IS_HEX(cli_line_buffer[13])
		&& CHAR_IS_HEX(cli_line_buffer[14])
		&& CHAR_IS_HEX(cli_line_buffer[15])
		&& CHAR_IS_HEX(cli_line_buffer[16]))
	{
		settings_current->PortAddress = CHAR_HEX_VAL(cli_line_buffer[13]);
		settings_current->PortAddress <<= 4;
		settings_current->PortAddress |= CHAR_HEX_VAL(cli_line_buffer[14]);
		settings_current->PortAddress <<= 4;
		settings_current->PortAddress |= CHAR_HEX_VAL(cli_line_buffer[15]);
		settings_current->PortAddress <<= 4;
		settings_current->PortAddress |= CHAR_HEX_VAL(cli_line_buffer[16]);

		settings_save();
		CLI_OUTPUT_MSG(cli_msg_param_updated);
	}

	// estaman HHHH
	else if(cli_line_size == 12
		&& cli_line_buffer[0] == 'e'
		&& cli_line_buffer[1] == 's'
		&& cli_line_buffer[2] == 't'
		&& cli_line_buffer[3] == 'a'
		&& cli_line_buffer[4] == 'm'
		&& cli_line_buffer[5] == 'a'
		&& cli_line_buffer[6] == 'n'
		&& cli_line_buffer[7] == ' '
		&& CHAR_IS_HEX(cli_line_buffer[8])
		&& CHAR_IS_HEX(cli_line_buffer[9])
		&& CHAR_IS_HEX(cli_line_buffer[10])
		&& CHAR_IS_HEX(cli_line_buffer[11]))
	{
		settings_current->EstaManHi = CHAR_HEX_VAL(cli_line_buffer[8]);
		settings_current->EstaManHi <<= 4;
		settings_current->EstaManHi |= CHAR_HEX_VAL(cli_line_buffer[9]);

		settings_current->EstaManLo = CHAR_HEX_VAL(cli_line_buffer[10]);
		settings_current->EstaManLo <<= 4;
		settings_current->EstaManLo |= CHAR_HEX_VAL(cli_line_buffer[11]);

		settings_save();
		CLI_OUTPUT_MSG(cli_msg_param_updated);
	}

	// oem HHHH
	else if(cli_line_size == 8
		&& cli_line_buffer[0] == 'o'
		&& cli_line_buffer[1] == 'e'
		&& cli_line_buffer[2] == 'm'
		&& cli_line_buffer[3] == ' '
		&& CHAR_IS_HEX(cli_line_buffer[4])
		&& CHAR_IS_HEX(cli_line_buffer[5])
		&& CHAR_IS_HEX(cli_line_buffer[6])
		&& CHAR_IS_HEX(cli_line_buffer[7]))
	{
		settings_current->OemHi = CHAR_HEX_VAL(cli_line_buffer[4]);
		settings_current->OemHi <<= 4;
		settings_current->OemHi |= CHAR_HEX_VAL(cli_line_buffer[5]);

		settings_current->OemLo = CHAR_HEX_VAL(cli_line_buffer[6]);
		settings_current->OemLo <<= 4;
		settings_current->OemLo |= CHAR_HEX_VAL(cli_line_buffer[7]);

		settings_save();
		CLI_OUTPUT_MSG(cli_msg_param_updated);
	}

	// net_switch 0
	else if(cli_line_size == 12
		&& cli_line_buffer[0] == 'n'
		&& cli_line_buffer[1] == 'e'
		&& cli_line_buffer[2] == 't'
		&& cli_line_buffer[3] == '_'
		&& cli_line_buffer[4] == 's'
		&& cli_line_buffer[5] == 'w'
		&& cli_line_buffer[6] == 'i'
		&& cli_line_buffer[7] == 't'
		&& cli_line_buffer[8] == 'c'
		&& cli_line_buffer[9] == 'h'
		&& cli_line_buffer[10] == ' '
		&& CHAR_IS_DIGIT(cli_line_buffer[11]))
	{
		settings_current->net_switch = cli_line_buffer[11] == '0' ? 0 : 1;

		settings_save();
		CLI_OUTPUT_MSG(cli_msg_param_updated);
	}

	// mac_address <mac>
	else if(cli_line_size == 29
		&& cli_line_buffer[0] == 'm'
		&& cli_line_buffer[1] == 'a'
		&& cli_line_buffer[2] == 'c'
		&& cli_line_buffer[3] == '_'
		&& cli_line_buffer[4] == 'a'
		&& cli_line_buffer[5] == 'd'
		&& cli_line_buffer[6] == 'd'
		&& cli_line_buffer[7] == 'r'
		&& cli_line_buffer[8] == 'e'
		&& cli_line_buffer[9] == 's'
		&& cli_line_buffer[10] == 's'
		&& cli_line_buffer[11] == ' '
		&& CHAR_IS_HEX(cli_line_buffer[12])
		&& CHAR_IS_HEX(cli_line_buffer[13])
		&& cli_line_buffer[14] == ':'
		&& CHAR_IS_HEX(cli_line_buffer[15])
		&& CHAR_IS_HEX(cli_line_buffer[16])
		&& cli_line_buffer[17] == ':'
		&& CHAR_IS_HEX(cli_line_buffer[18])
		&& CHAR_IS_HEX(cli_line_buffer[19])
		&& cli_line_buffer[20] == ':'
		&& CHAR_IS_HEX(cli_line_buffer[21])
		&& CHAR_IS_HEX(cli_line_buffer[22])
		&& cli_line_buffer[23] == ':'
		&& CHAR_IS_HEX(cli_line_buffer[24])
		&& CHAR_IS_HEX(cli_line_buffer[25])
		&& cli_line_buffer[26] == ':'
		&& CHAR_IS_HEX(cli_line_buffer[27])
		&& CHAR_IS_HEX(cli_line_buffer[28]))
	{
		settings_current->mac_address[0] = CHAR_HEX_VAL(cli_line_buffer[12]);
		settings_current->mac_address[0] <<= 4;
		settings_current->mac_address[0] |= CHAR_HEX_VAL(cli_line_buffer[13]);

		settings_current->mac_address[1] = CHAR_HEX_VAL(cli_line_buffer[15]);
		settings_current->mac_address[1] <<= 4;
		settings_current->mac_address[1] |= CHAR_HEX_VAL(cli_line_buffer[16]);

		settings_current->mac_address[2] = CHAR_HEX_VAL(cli_line_buffer[18]);
		settings_current->mac_address[2] <<= 4;
		settings_current->mac_address[2] |= CHAR_HEX_VAL(cli_line_buffer[19]);

		settings_current->mac_address[3] = CHAR_HEX_VAL(cli_line_buffer[21]);
		settings_current->mac_address[3] <<= 4;
		settings_current->mac_address[3] |= CHAR_HEX_VAL(cli_line_buffer[22]);

		settings_current->mac_address[4] = CHAR_HEX_VAL(cli_line_buffer[24]);
		settings_current->mac_address[4] <<= 4;
		settings_current->mac_address[4] |= CHAR_HEX_VAL(cli_line_buffer[25]);

		settings_current->mac_address[5] = CHAR_HEX_VAL(cli_line_buffer[27]);
		settings_current->mac_address[5] <<= 4;
		settings_current->mac_address[5] |= CHAR_HEX_VAL(cli_line_buffer[28]);

		settings_save();
		CLI_OUTPUT_MSG(cli_msg_param_updated);
	}
	else
		e = 1;

	if(e)
	{
		CLI_OUTPUT_MSG(cli_msg_parse_error);
		CLI_OUTPUT_NL;
		CLI_OUTPUT_MSG(cli_msg_prompt);
	}
	else
	{
		CLI_OUTPUT_NL;
		CLI_OUTPUT_MSG(cli_msg_prompt);
	}
}

void cli_idle()
{
	uint8_t c;
	int s, w, o;

	/* should we sent some data */
	if(cli_send_curr == cli_send_head && cli_send_curr < cli_send_tail)
	{
		/* find bytes need to send */
		s = cli_send_tail - cli_send_curr;

		/* calc max wrapped size */
		o = cli_send_head % CLI_SEND_BUFFER_SIZE;
		w = CLI_SEND_BUFFER_SIZE - o;
		if(w < s)
			s = w;

		/* check if max block send */
		if(s > 512)
			s = 512;

		CDC_Transmit_FS(cli_send_buffer + o, s);
		cli_send_curr += s;

		return;
	}

	/* check if we received data, then process it */
	while(cli_recv_head < cli_recv_tail)
	{
		/* fetch character */
		c = cli_recv_buffer[cli_recv_head % CLI_RECV_BUFFER_SIZE];
		cli_recv_head++;

		switch(c)
		{
			// <enter>
			case 0x0D:
			case 0x0A:
				CLI_OUTPUT_NL;
				cli_line_process();
				cli_line_size = 0;
				break;

			// <ctrl-z>
			case 0x1A:
				break;

			// <ctrl-r>
			case 0x12:
				break;

			// <tab>
			case 0x09:
				break;

			// <ctrl-x>
			case 0x18:
				break;

			// <ctrl-d>
			case 0x04:
				continue;

			// <ctrl-c>
			case 0x03:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_NL;
				CLI_OUTPUT_MSG(cli_msg_prompt);
				cli_line_size = 0;
				break;

			// <backspace>
			case 0x7F:
			// <del>
			case 0xB1:
				/* 'backspace' implemented in 3 characters */
				if(cli_line_size)
				{
					CLI_OUTPUT_CHAR('\b');
					CLI_OUTPUT_CHAR(' ');
					CLI_OUTPUT_CHAR('\b');
					cli_line_size--;
				}
				break;

			default:
				/* append to send buffer */
				CLI_OUTPUT_CHAR(c);

				/* save to line buffer */
				if(cli_line_size < CLI_LINE_BUFFER_SIZE)
				{
					cli_line_buffer[cli_line_size] = c;
					cli_line_size++;
				};
		}
	}
}

extern USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
static int8_t _CDC_Receive(uint8_t *Buf, uint32_t *Len)
{
	cli_callback_recv_data(Buf, *Len);
	USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
	return (USBD_OK);
}

static int8_t _CDC_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
	cli_callback_sent_data(Buf, *Len);
	return (USBD_OK);
};

void cli_init()
{
	USBD_Interface_fops_FS.Receive = _CDC_Receive;
	USBD_Interface_fops_FS.TransmitCplt = _CDC_TransmitCplt;
};

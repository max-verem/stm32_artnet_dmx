#ifndef W5500_REGS_H
#define W5500_REGS_H

/* Offset Address for Common Register */
#define W5500_CR_MR			0x0000	// Mode [1]
#define W5500_CR_GAR		0x0001	// Gateway Address [4]
#define W5500_CR_SUBR		0x0005	// Subnet Mask Address [4]
#define W5500_CR_SHAR		0x0009	// Source Hardware Address [6]
#define W5500_CR_SIPR		0x000F	// Source IP Address [4]
#define W5500_CR_INTLEVEL	0x0013	// Interrupt Low Level Timer [2]
#define W5500_CR_IR			0x0015	// Interrupt [1]
#define W5500_CR_IMR		0x0016	// Interrupt Mask [1]
#define W5500_CR_SIR		0x0017	// Socket Interrupt [1]
#define W5500_CR_SIMR		0x0018	// Socket Interrupt Mask [1]
#define W5500_CR_RTR		0x0019 	// Retry Time [2]
#define W5500_CR_RCR 		0x001B	// Retry Count [1]
#define W5500_CR_PTIMER		0x001C 	// PPP LCP Request Timer [1]
#define W5500_CR_PMAGIC 	0x001D 	// PPP LCP Magic number [1]
#define W5500_CR_PHAR 		0x001E 	// PPP Destination MAC Address [6]
#define W5500_CR_PSID 		0x0024 	// PPP Session Identification [2]
#define W5500_CR_PMRU 		0x0026 	// PPP Maximum Segment Size [2]
#define W5500_CR_UIPR 		0x0028 	// Unreachable IP address [4]
#define W5500_CR_UPORTR 	0x002C 	// Unreachable Port [2]
#define W5500_CR_PHYCFGR 	0x002E 	// PHY Configuration [1]
#define W5500_CR_VERSIONR 	0x0039 	// Chip version [1]

#define W5500_CR_BLOCK		0x0000
#define W5500_Sn_BLOCK(N)	(1 + N * 4)
#define W5500_SnR_BLOCK(N)	(W5500_Sn_BLOCK(N) + 0)
#define W5500_SnTX_BLOCK(N)	(W5500_Sn_BLOCK(N) + 1)
#define W5500_SnRX_BLOCK(N)	(W5500_Sn_BLOCK(N) + 2)

#define W5500_CONTROL(OFFSET, BLOCK, WRITE)	((OFFSET << 8) | (BLOCK << 3) | (WRITE << 2))
#define W5500_CONTROL_W(OFFSET, BLOCK) W5500_CONTROL(OFFSET, BLOCK, 1)
#define W5500_CONTROL_R(OFFSET, BLOCK) W5500_CONTROL(OFFSET, BLOCK, 0)
#define W5500_PUT_CONTROL(BUF, VAL) 	\
	BUF[0] = ((VAL) >> 16) & 0x00FF; 	\
	BUF[1] = ((VAL) >>  8) & 0x00FF; 	\
	BUF[2] = ((VAL) >>  0) & 0x00FF;


#define W5500_Sn_MR			0x0000	// Socket n Mode
#define W5500_Sn_CR			0x0001	// Socket n Command
#define W5500_Sn_IR			0x0002	// Socket n Interrupt
#define W5500_Sn_SR			0x0003	// Socket n Status
#define W5500_Sn_PORT		0x0004	// Socket n Source Port
#define W5500_Sn_DHAR		0x0006	// Socket n Destination Hardware Address
#define W5500_Sn_DIPR		0x000C	// Socket n Destination IP Address
#define W5500_Sn_DPORT		0x0010	// Socket n Destination Port
#define W5500_Sn_MSSR		0x0012	// Socket n Maximum Segment Size
#define W5500_Sn_TOS		0x0015	// Socket n IP TOS
#define W5500_Sn_TTL		0x0016	// Socket n IP TTL
#define W5500_Sn_RXBUF_SIZE	0x001E	// Socket n Receive Buffer Size
#define W5500_Sn_TXBUF_SIZE	0x001F	// Socket n Transmit Buffer Size
#define W5500_Sn_TX_FSR		0x0020	// Socket n TX Free Size
#define W5500_Sn_TX_RD		0x0022	// Socket n TX Read Pointer
#define W5500_Sn_TX_WR		0x0024	// Socket n TX Write Pointer
#define W5500_Sn_RX_RSR		0x0026	// Socket n RX Received Size [2]
#define W5500_Sn_RX_RD		0x0028	// Socket n RX Read Pointer [2]
#define W5500_Sn_RX_WR		0x002A	// Socket n RX Write Pointer
#define W5500_Sn_IMR		0x002C	// Socket n Interrupt Mask
#define W5500_Sn_FRAG		0x002D	// Socket n Fragment Offset in IP header
#define W5500_Sn_KPALVTR	0x002F	// Keep alive timer


#endif

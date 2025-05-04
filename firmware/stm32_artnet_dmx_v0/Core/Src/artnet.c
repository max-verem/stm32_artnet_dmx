#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include "artnet.h"
#include "settings.h"

static const unsigned char artnet_packet_signature[8] = "Art-Net";

static const unsigned char
    UbeaVersion = 0,
    Status1 = 0xD0, // Indicators in Normal Mode, All Port-Address set by front panel controls
    NumPortsHi = 0,
    NumPortsLo = 1,
    PortTypes[4] = {0x80 | 0, 0x80 | 0, 0, 0},
    GoodInput[4] = {0, 0, 0, 0},
    GoodOutputA[4] = {0x80, 0x80, 0, 0},
    SwIn[4] = {0, 0, 0, 0},
    SwOut[4] = {1, 2, 0, 0}
    ;

const unsigned char
    PortName[18] = "STM32_ARTNET_DMX",
    LongName[64] = "STM32 Art-Net DMX"
    ;

static int cnt_poll = 0;

int artnet_parse_opcode(unsigned char* buf, int len)
{
    int opcode = 0;

    if( ((uint32_t*)artnet_packet_signature)[0] != ((uint32_t*)buf)[0] || ((uint32_t*)artnet_packet_signature)[1] != ((uint32_t*)buf)[1] )
        return -EPROTOTYPE;

    buf += 8;

    opcode |= buf[1];
    opcode <<= 8;
    opcode |= buf[0];
    buf += 2;
#if 0
    // ProtVerHi
    if( *buf )
        return -EINVAL;
    buf++;

    // ProtVerLo
    if( *buf != 14)
        return -EINVAL;
#endif
    return opcode;
};

#define PUT_BYTE(BUF, BYTE)     *BUF = BYTE; BUF++;

#define PUT_2BYTES(BUF, BYTES)  \
    PUT_BYTE(BUF, BYTES[0]);    \
    PUT_BYTE(BUF, BYTES[1]);

#define PUT_4BYTES(BUF, BYTES)          \
    PUT_2BYTES(BUF, (&BYTES[0]));       \
    PUT_2BYTES(BUF, (&BYTES[2]));

#define PUT_8BYTES(BUF, BYTES)          \
    PUT_4BYTES(BUF, (&BYTES[0]));       \
    PUT_4BYTES(BUF, (&BYTES[4]));

#define PUT_16BYTES(BUF, BYTES)          \
    PUT_8BYTES(BUF, (&BYTES[0]));       \
    PUT_8BYTES(BUF, (&BYTES[8]));

#define PUT_32BYTES(BUF, BYTES)          \
    PUT_16BYTES(BUF, (&BYTES[0]));       \
    PUT_16BYTES(BUF, (&BYTES[16]))

#define PUT_64BYTES(BUF, BYTES)          \
    PUT_32BYTES(BUF, (&BYTES[0]));       \
    PUT_32BYTES(BUF, (&BYTES[32]));

#define PUT_16LE(BUF, VAL) \
    *BUF = ((VAL >> 0) & 0x00FF); BUF++; \
    *BUF = ((VAL >> 8) & 0x00FF); BUF++;

#define PUT_32LE(BUF, VAL) \
    *BUF = ((VAL >> 0) & 0x00FF); BUF++; \
    *BUF = ((VAL >> 8) & 0x00FF); BUF++; \
    *BUF = ((VAL >> 16) & 0x00FF); BUF++; \
    *BUF = ((VAL >> 24) & 0x00FF); BUF++;

int artnet_compose_OpPollReply(unsigned char* buf)
{
    unsigned char NetSwitch = (settings_current->PortAddress >> 8) & 0x7F;
    unsigned char SubSwitch = (settings_current->PortAddress >> 4) & 0x0F;
    unsigned char NodeReport[80];

    unsigned char* buf_tail = buf;

    /* 1 ID[8] Int8 - Array of 8 characters, the final character is a null termination. Value = ‘A’ ‘r’ ‘t’ ‘-‘ ‘N’ ‘e’ ‘t’ 0x00 */
    PUT_8BYTES(buf_tail, artnet_packet_signature);

    /* 2 OpCode Int16  OpPollReply Transmitted low byte first. */
    PUT_16LE(buf_tail, artnet_OpPollReply);

    /* 3 IP Address[4] Int8 - Array containing the Node’s IP address. First array entry is most significant byte of address. */
    PUT_4BYTES(buf_tail, settings_current->ip_address);

    /* 4 Port Int16 - The Port is always 0x1936 Transmitted low byte first. */
    PUT_16LE(buf_tail, ARTNET_UDP_PORT);

    /* 5 VersInfoH Int8 - High byte of Node’s firmware revision number. The Controller should only use this field to decide if a firmware update should proceed. The convention is that a higher number is a more recent release of firmware. */
    PUT_BYTE(buf_tail, settings_current->VersInfoH);

    /* 6 VersInfo Int8 - Low byte of Node’s firmware revision number. */
    PUT_BYTE(buf_tail, settings_current->VersInfoL);

    /* 7 NetSwitch Int8 - Bits 14-8 of the 15 bit Port-Address are encoded into the bottom 7 bits of this field. This is used in combination with SubSwitch and SwIn[] or SwOut[] to produce the full universe address. */
    PUT_BYTE(buf_tail, NetSwitch);

    /* 8 SubSwitch Int8 - Bits 7-4 of the 15 bit Port-Address are encoded into the bottom 4 bits of this field. This is used in combination with NetSwitch and SwIn[] or SwOut[] to produce the full universe address. */
    PUT_BYTE(buf_tail, SubSwitch);

    /* 9 OemHi Int8 - The high byte of the Oem value. */
    PUT_BYTE(buf_tail, settings_current->OemHi);

    /* 10 Oem Int8 - The low byte of the Oem value. The Oem word describes the equipment vendor and the feature set available. Bit 15 high indicates extended features available. Current registered codes are defined in Table 2. */
    PUT_BYTE(buf_tail, settings_current->OemLo);

    /* 11 Ubea Version Int8 - This field contains the firmware version of the User Bios Extension Area (UBEA). If the UBEA is not programmed, this field contains zero. */
    PUT_BYTE(buf_tail, UbeaVersion);

    /* 12 Status1 Int8 - General Status register containing bit fields as follows. */
    PUT_BYTE(buf_tail, Status1);

    /* 13 EstaMan Int16 - The ESTA manufacturer code. These codes are used to represent equipment manufacturer. They are assigned by ESTA. This field can be interpreted as two ASCII bytes representing the manufacturer initials. */
    PUT_BYTE(buf_tail, settings_current->EstaManLo);
    PUT_BYTE(buf_tail, settings_current->EstaManHi);

    /* 14 ShortName [18] Int8 - The array represents a null terminated short name for the Node. The Controller uses the ArtAddress packet to program this string. Max length is 17 characters plus the null. This is a fixed length field, although the string it contains can be shorter than the field. */
    PUT_16BYTES(buf_tail, PortName);
    PUT_2BYTES(buf_tail, (&PortName[16]));

    /* 15 LongName [64] Int8 - The array represents a null terminated long name for the Node. The Controller uses the ArtAddress packet to program this string. Max length is 63 characters plus the null. This is a fixed length field, although the string it contains can be shorter than the field. */
    PUT_64BYTES(buf_tail, LongName);

    /* 16 NodeReport [64] Int8 - The array is a textual report of the Node’s operating status or operational errors. It is primarily intended for ‘engineering’ data rather than ‘end user’ data. The field is formatted as: “#xxxx [yyyy..] zzzzz…” xxxx is a hex status code as defined in Table 3. yyyy is a decimal counter that increments every time the Node sends an ArtPollResponse. This allows the controller to monitor event changes in the Node. zzzz is an English text string defining the status. This is a fixed length field, although the string it contains can be shorter than the field. */
    snprintf((char*)NodeReport, sizeof(NodeReport),
        "#0001 [%.4d] node is online", cnt_poll % 10000);
    cnt_poll++;
    PUT_64BYTES(buf_tail, NodeReport);

    /* 17 NumPortsHi Int8 - The high byte of the word describing the number of input or output ports. The high byte is for future expansion and is currently zero. */
    PUT_BYTE(buf_tail, NumPortsHi);

    /* 18 NumPortsLo Int8 - The low byte of the word describing the number of input or output ports. If number of inputs is not equal to number of outputs, the largest value is taken. Zero is a legal value if no input or output ports are implemented. The maximum value is 4. */
    PUT_BYTE(buf_tail, NumPortsLo);

    /* 19 PortTypes [4] Int8 - This array defines the operation and protocol of each channel. (Ether-Lynx example = 0xc0, 0xc0, 0xc0, 0xc0). The array length is fixed, independent of the number of inputs or outputs physically available on the Node. */
    PUT_4BYTES(buf_tail, PortTypes);

    /* 20 GoodInput [4] Int8 - This array defines input status of the node. */
    PUT_4BYTES(buf_tail, GoodInput);

    /* 21 GoodOutput [4] Int8 - This array defines output status of the node. */
    PUT_4BYTES(buf_tail, GoodOutputA);

    /* 22 SwIn [4] Int8 - Bits 3-0 of the 15 bit Port-Address for each of the 4 possible input ports are encoded into the low nibble. */
    PUT_4BYTES(buf_tail, SwIn);

    /* 23 SwOut [4] Int8 - Bits 3-0 of the 15 bit Port-Address for each of the 4 possible output ports are encoded into the low nibble. */
    PUT_4BYTES(buf_tail, SwOut);

    /* 24 SwVideo Int8 */
    PUT_BYTE(buf_tail, 0x00);

    /* 25 SwMacro Int8 */
    PUT_BYTE(buf_tail, 0x00);

    /* 26 SwRemote Int8 */
    PUT_BYTE(buf_tail, 0x00);

    /* 27 Spare Int8 Not used, set to zero */
    PUT_BYTE(buf_tail, 0x00);

    /* 28 Spare Int8 Not used, set to zero */
    PUT_BYTE(buf_tail, 0x00);

    /* 29 Spare Int8 Not used, set to zero */
    PUT_BYTE(buf_tail, 0x00);

    /* 30 Style Int8 The Style code defines the equipment style of the device. See Table 4 for current Style codes. */
    PUT_BYTE(buf_tail, 0x00);

    /* 31 MAC Hi Int8 MAC Address Hi Byte. Set to zero if node cannot supply this information. */
    PUT_BYTE(buf_tail, settings_current->mac_address[0]);
    PUT_BYTE(buf_tail, settings_current->mac_address[1]);
    PUT_BYTE(buf_tail, settings_current->mac_address[2]);
    PUT_BYTE(buf_tail, settings_current->mac_address[3]);
    PUT_BYTE(buf_tail, settings_current->mac_address[4]);
    PUT_BYTE(buf_tail, settings_current->mac_address[5]);

    /* 37 BindIp[4] Int8 If this unit is part of a larger or modular product, this is the IP of the root device. */
    PUT_BYTE(buf_tail, 0x00);
    PUT_BYTE(buf_tail, 0x00);
    PUT_BYTE(buf_tail, 0x00);
    PUT_BYTE(buf_tail, 0x00);

    /* 38 BindIndex Int8 Set to zero if no binding, otherwise this number represents the order of bound devices. A lower number means closer to root device. */
    PUT_BYTE(buf_tail, 0x00);

    /* 39 Status2 Int8 0 Set = Product supports web browser configuration. */
    PUT_BYTE(buf_tail, 0x00);

    /* 40 Filler 26 x 8 Transmit as zero. For future expansion. */
    buf_tail += 26 * 7;

    return buf_tail - buf;
};

int artnet_process_OpDmx(unsigned char* buf, int (*callback)(unsigned char *buf, int len, int idx))
{
    int i, pa = 0, len = 0;

    // Length
    len = 0;
    len |= buf[16];
    len <<= 8;
    len |= buf[17];

    // PortAddress (Universe)
    pa |= buf[15];
    pa <<= 8;
    pa |= buf[14];
    if((pa & 0x7FF0) != settings_current->PortAddress)
        return -EADDRNOTAVAIL;

    // Switch
    pa &= 0x000F;
    for(i = 0; i < 4; i++)
    if(pa == SwOut[i])
        return callback(buf + 18, len, i);

    return -ENOENT;
};

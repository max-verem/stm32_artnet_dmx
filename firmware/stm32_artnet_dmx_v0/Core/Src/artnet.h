#ifndef ARTNET_H
#define ARTNET_H

enum
{
    artnet_OpPoll = 0x2000,
    artnet_OpPollReply = 0x2100,
    artnet_OpDmx = 0x5000,
    artnet_OpOutput = 0x5000,
    artnet_OpSync = 0x5200,
};

#define ARTNET_UDP_PORT 0x1936

extern const unsigned char PortName[18], LongName[64];

int artnet_parse_opcode(unsigned char* buf, int len);
int artnet_compose_OpPollReply(unsigned char* buf);
int artnet_process_OpDmx(unsigned char* buf, int (*callback)(unsigned char *buf, int len, int idx));

#endif

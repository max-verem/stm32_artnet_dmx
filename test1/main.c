#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "artnet.h"

int open_socket(struct sockaddr_in *p_local_addr)
{
    int f, fd, r;
    socklen_t l;

    /* create socket */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
        r = errno;
        fprintf(stderr, "%s: socket failed, r=%d (%s)\n",
            __FUNCTION__, r, strerror(r));
        return -r;
    };

    /* set options */
    f = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&f, sizeof(f));
    f = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void *)&f, sizeof(f));
    f = 1; setsockopt(fd, IPPROTO_IP, IP_PKTINFO, (const void *)&f, sizeof(f));
    f = 1; setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const void *)&f, sizeof(f));

    /* bind */
    memset(p_local_addr, 0, sizeof(struct sockaddr_in));
    p_local_addr->sin_family = AF_INET;
    p_local_addr->sin_addr.s_addr = INADDR_ANY;
    p_local_addr->sin_port = htons((unsigned short)0x1936);;
    r = bind(fd, (const struct sockaddr *)p_local_addr, sizeof(struct sockaddr_in));
    if(r < 0)
    {
        r = errno;
        fprintf(stderr, "%s: bind failed, r=%d (%s)\n", __FUNCTION__, r, strerror(r));
        return -r;
    };

    /* find a port we bound */
    l = sizeof(struct sockaddr_in);
    r = getsockname(fd, (struct sockaddr *)p_local_addr, &l);
    if(r < 0)
    {
        r = errno;
        fprintf(stderr, "%s: getsockname failed, r=%d (%s)\n", __FUNCTION__, r, strerror(r));
        return -r;
    };
    fprintf(stderr, "%s: local=%s:%d\n", __FUNCTION__,
        inet_ntoa(p_local_addr->sin_addr),
        (int) ntohs(p_local_addr->sin_port));

    return fd;
};

static int dmx_data(unsigned char* buf, int len, int idx)
{
    fprintf(stderr, "%s: buf=%p, len=%d, idx=%d\n", __FUNCTION__, buf, len, idx);
    fprintf(stderr, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n", __FUNCTION__,
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    return 0;
};

int main(int argc, char** argv)
{
    int r, fd;
    uint8_t buf[2048];
    struct sockaddr_in local_addr;
    char ip1[32], ip2[32], ip3[32], ip4[32];

    fd = open_socket(&local_addr);
    if(fd < 0)
        return -1;

    while(1)
    {
        int len;
        uint8_t control_buf[2048];
        struct cmsghdr *cmsg;
        struct in_pktinfo *pi;
        struct sockaddr_in remote_addr;

        struct iovec iov = {
            .iov_base = buf,
            .iov_len = sizeof(buf),
        };

        struct msghdr mh = {
            .msg_name = &remote_addr,
            .msg_namelen = sizeof(remote_addr),
            .msg_control = control_buf,
            .msg_controllen = sizeof(control_buf),
            .msg_iov = &iov,
            .msg_iovlen = 1,
        };

        /* something here */
        r = recvmsg(fd, &mh, 0);
        if(r < 0)
        {
            r = errno;
            fprintf(stderr, "%s: recvfrom failed, r=%d (%s)\n", __FUNCTION__, r, strerror(r));
            break;
        };
        len = r;

        // iterate through all the control headers
        for (pi = NULL, cmsg = CMSG_FIRSTHDR(&mh); cmsg && !pi; cmsg = CMSG_NXTHDR(&mh, cmsg))
        {
            // ignore the control headers that don't match what we want
            if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
                continue;

            // at this point, peeraddr is the source sockaddr
            // pi->ipi_spec_dst is the destination in_addr
            // pi->ipi_addr is the receiving interface in_addr
            pi = (struct in_pktinfo *)CMSG_DATA(cmsg);
        };

        if(1)
        {
            fprintf(stderr, "%s: %d bytes, remote_addr=%s:%d, local_addr=%s:%d, ipi_spec_dst=%s, ipi_addr=%s\n",
                __FUNCTION__, len,
                inet_ntop(AF_INET, &remote_addr.sin_addr, ip1, sizeof(ip1)),
                    (int) ntohs(remote_addr.sin_port),

                inet_ntop(AF_INET, &local_addr.sin_addr, ip2, sizeof(ip2)),
                    (int) ntohs(local_addr.sin_port),

                inet_ntop(AF_INET, &pi->ipi_spec_dst, ip3, sizeof(ip3)),

                inet_ntop(AF_INET, &pi->ipi_addr, ip4, sizeof(ip4))
            );

        };

        r = artnet_parse_opcode(buf, len);
        if(r < 0)
        {
            fprintf(stderr, "%s: artnet_parse_opcode failed, r=%d (%s)\n", __FUNCTION__, r, strerror(-r));
            continue;
        };
        fprintf(stderr, "%s: opcode=0x%04X\n", __FUNCTION__, r);

        if(r == artnet_OpPoll)
        {
            struct sockaddr_in addr;

            len = artnet_compose_OpPollReply(buf);

            addr.sin_family = AF_INET;
            addr.sin_addr = pi->ipi_addr;
            addr.sin_port = htons((unsigned short)0x1936);

            fprintf(stderr, "%s: sending %d bytes to %s:%d\n",
                __FUNCTION__, len,
                inet_ntop(AF_INET, &addr.sin_addr, ip1, sizeof(ip1)),
                    (int) ntohs(addr.sin_port));

            r = sendto(fd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
            if(r < 0)
            {
                r = errno;
                break;
            };

        }
        else if(r == artnet_OpDmx)
        {
            r = artnet_process_OpDmx(buf, dmx_data);
            if(r < 0)
                fprintf(stderr, "%s: artnet_process_OpDmx failed, r=%d (%s)\n", __FUNCTION__, r, strerror(-r));
            else
                fprintf(stderr, "%s: artnet_process_OpDmx=%d\n", __FUNCTION__, r);
        }
        else
            fprintf(stderr, "%s: NOT SUPPORTED opcode=0x%04X\n", __FUNCTION__, r);
    };

    close(fd);
    fprintf(stderr, "%s: bye\n", __FUNCTION__);

    return 0;
};

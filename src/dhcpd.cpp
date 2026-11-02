// Minimaler DHCP-Server fuer den PC-Modus (USB-Netzwerk): gibt dem PC sofort 192.168.77.10.
// Aufruf (als root): groovebox --dhcpd usb0
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int runDhcpd(const char* ifname) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("dhcpd socket"); return 1; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, (socklen_t)strlen(ifname) + 1) < 0) perror("dhcpd SO_BINDTODEVICE");
    sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(67); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (sockaddr*)&a, sizeof a) < 0) { perror("dhcpd bind"); return 1; }
    const uint8_t server[4] = {192, 168, 77, 1}, client[4] = {192, 168, 77, 10}, mask[4] = {255, 255, 255, 0};
    printf("DHCP-Server auf %s: vergebe 192.168.77.10\n", ifname);
    fflush(stdout);
    uint8_t b[1500];
    for (;;) {
        int n = (int)recv(fd, b, sizeof b, 0);
        if (n < 240 || b[0] != 1) continue;
        if (b[236] != 0x63 || b[237] != 0x82 || b[238] != 0x53 || b[239] != 0x63) continue;
        int type = 0;
        for (int i = 240; i < n && b[i] != 255;) {
            if (b[i] == 0) { i++; continue; }
            if (i + 1 >= n) break;
            int l = b[i + 1];
            if (b[i] == 53 && l >= 1 && i + 2 < n) type = b[i + 2];
            i += 2 + l;
        }
        int reply = type == 1 ? 2 : (type == 3 ? 5 : 0);   // DISCOVER->OFFER, REQUEST->ACK
        if (!reply) continue;
        uint8_t r[300];
        memset(r, 0, sizeof r);
        r[0] = 2; r[1] = 1; r[2] = 6;
        memcpy(r + 4, b + 4, 4);       // xid
        memcpy(r + 10, b + 10, 2);     // flags
        memcpy(r + 16, client, 4);     // yiaddr
        memcpy(r + 20, server, 4);     // siaddr
        memcpy(r + 28, b + 28, 16);    // chaddr
        r[236] = 0x63; r[237] = 0x82; r[238] = 0x53; r[239] = 0x63;
        int o = 240;
        r[o++] = 53; r[o++] = 1; r[o++] = (uint8_t)reply;
        r[o++] = 54; r[o++] = 4; memcpy(r + o, server, 4); o += 4;
        r[o++] = 51; r[o++] = 4; r[o++] = 0; r[o++] = 1; r[o++] = 0x51; r[o++] = 0x80;   // 86400 s
        r[o++] = 1; r[o++] = 4; memcpy(r + o, mask, 4); o += 4;                           // kein Gateway/DNS
        r[o++] = 255;
        sockaddr_in to;
        memset(&to, 0, sizeof to);
        to.sin_family = AF_INET; to.sin_port = htons(68); to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        sendto(fd, r, sizeof r, 0, (sockaddr*)&to, sizeof to);
        printf("DHCP: %s -> 192.168.77.10\n", reply == 2 ? "OFFER" : "ACK");
        fflush(stdout);
    }
    return 0;
}

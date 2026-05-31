#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "../link_layer/link_layer.h"

// wire format do Packet: [headerLength(4)][totalLength(4)][src(4)][dst(4)][payload...]
#define PKT_HDR_SIZE (4 * (int)sizeof(int))

struct Packet deserializePacket(void* pkgData){
    struct Packet packet;
    int i=0;
    memcpy(&packet.headerLength,      pkgData+i, sizeof(packet.headerLength));      i += sizeof(int);
    memcpy(&packet.totalLength,       pkgData+i, sizeof(packet.totalLength));       i += sizeof(int);
    memcpy(&packet.sourceAddress,     pkgData+i, sizeof(packet.sourceAddress));     i += sizeof(int);
    memcpy(&packet.destinationAddress,pkgData+i, sizeof(packet.destinationAddress));i += sizeof(int);

    int len = packet.totalLength - packet.headerLength;
    packet.data = malloc(len);
    memcpy(packet.data, pkgData+i, len);

    return packet;
}

void* serializePacket(struct Packet packet){
    void* pkgData = malloc(packet.totalLength);

    int i=0;
    memcpy(pkgData+i, &packet.headerLength,       sizeof(int)); i += sizeof(int);
    memcpy(pkgData+i, &packet.totalLength,        sizeof(int)); i += sizeof(int);
    memcpy(pkgData+i, &packet.sourceAddress,      sizeof(int)); i += sizeof(int);
    memcpy(pkgData+i, &packet.destinationAddress, sizeof(int)); i += sizeof(int);
    memcpy(pkgData+i, packet.data, packet.totalLength - packet.headerLength);

    return pkgData;
}

int resolveIPToMAC(int ip_adress){
    return ip_adress;
}
int resolveMACToIP(int mac_adress){
    return mac_adress;
}

int network_send(int ip_destination, int ip_source, void* data, int data_size){
    struct Packet packet;
    
    packet.destinationAddress = ip_destination;
    packet.sourceAddress      = ip_source;
    packet.headerLength       = PKT_HDR_SIZE;
    packet.totalLength        = PKT_HDR_SIZE + data_size;
    packet.data               = data;

    void* serialized = serializePacket(packet);
    int res = send_packet(resolveIPToMAC(ip_destination),
                          resolveIPToMAC(ip_source),
                          serialized,
                          packet.totalLength);
    free(serialized);
    return res;
}

int network_listen(int ip_address, struct Packet* packet, char* host, char* port,
                   int timeout, int *out_src_ip){
    struct Frame frame;
    frame.data = NULL;

    if (listen_addr(&frame, resolveIPToMAC(ip_address), host, port, timeout) != 0)
    {
        if (frame.data) free(frame.data);
        return 1;
    }

    struct Packet pkg = deserializePacket(frame.data);
    free(frame.data);

    if (out_src_ip) *out_src_ip = pkg.sourceAddress;

    memcpy(packet, &pkg, sizeof(struct Packet));
    return 0;
}
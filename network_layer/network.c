#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "../link_layer/link.c"
struct Packet deserializePacket(void* pkgData){
    struct Packet packet;
    int i=0;
    memcpy(&packet.headerLength,pkgData+i,sizeof(packet.headerLength));
    i += sizeof(packet.headerLength);
    memcpy(&packet.totalLength,pkgData+i,sizeof(packet.totalLength));
    i += sizeof(packet.totalLength);
    memcpy(&packet.sourceAddress,pkgData+i,sizeof(packet.sourceAddress));
    i += sizeof(packet.sourceAddress);
    memcpy(&packet.destinationAddress,pkgData+i,sizeof(packet.destinationAddress));
    i += sizeof(packet.destinationAddress);

    int len = packet.totalLength-packet.headerLength;
    packet.data  =  malloc(sizeof(len));
    memcpy(packet.data,pkgData+i,len);

    return packet;
}

void* serializePacket(struct Packet packet){
    void* pkgData = malloc(packet.totalLength);

    int i=0;
    memcpy(pkgData+i,&packet.headerLength,sizeof(packet.headerLength));
    i += sizeof(packet.headerLength);
    memcpy(pkgData+i,&packet.totalLength,sizeof(packet.totalLength));
    i += sizeof(packet.totalLength);
    memcpy(pkgData+i,&packet.sourceAddress,sizeof(packet.sourceAddress));
    i += sizeof(packet.sourceAddress);
    memcpy(pkgData+i,&packet.destinationAddress,sizeof(packet.destinationAddress));
    i += sizeof(packet.destinationAddress);

    int len = packet.totalLength-packet.headerLength;
    packet.data  =  malloc(len);
    memcpy(pkgData+i,packet.data,len);
    //free(packet.data);
    return pkgData;


}

int resolveIPToMAC(int ip_adress){
    return ip_adress;
}
int resolveMACToIP(int mac_adress){
    return mac_adress;
}

int network_send(int ip_destination, int ip_source, void* data,int size){
    struct Packet packet;
    
    packet.destinationAddress=ip_destination;
    packet.sourceAddress=ip_source;
    
    packet.headerLength = 4*sizeof(int);
    packet.totalLength = packet.headerLength + size;
    packet.data = data;

    int res = send_packet(resolveIPToMAC(ip_destination),
                resolveIPToMAC(ip_source),
                serializePacket(packet),
                packet.totalLength);
    //free(packet.data);
    return res;
}
int network_listen(int ip_adress, struct Packet* packet){
    struct Frame *frame = malloc(sizeof(struct Frame));
    
    if (listen_addr(frame,resolveIPToMAC(ip_adress),"0.0.0.0","7777")!=0)
    {
        free(frame);
        return 1;
    }
    

    struct Packet pkg = deserializePacket(frame->data);
    memcpy(packet,&pkg,sizeof(struct Packet));

    free(frame);
    return 0;

}
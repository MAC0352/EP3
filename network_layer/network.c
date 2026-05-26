#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "../link_layer/link.c"
struct Packet deserializePacket(void* pkgData){
    struct Packet packet;
    int i=0;
    unsigned char *pchar = (unsigned char *)pkgData;
    
    memcpy(&packet.headerLength,&pchar,sizeof(packet.headerLength));
    i += sizeof(packet.headerLength);
    memcpy(&packet.totalLength,&pchar+i,sizeof(packet.totalLength));
    i += sizeof(packet.totalLength);
    memcpy(&packet.sourceAddress,&pchar+i,sizeof(packet.sourceAddress));
    i += sizeof(packet.sourceAddress);
    memcpy(&packet.destinationAddress,&pchar+i,sizeof(packet.destinationAddress));
    i += sizeof(packet.destinationAddress);
    
    printf("%d\n",packet.headerLength);
    int len = packet.totalLength-packet.headerLength;
    packet.data  =  malloc(len);
    memcpy(packet.data,pkgData+i,len);

    return packet;
}

void* serializePacket(struct Packet packet){
    unsigned char* pkgData = malloc(packet.totalLength);

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
    memcpy(pkgData+i,packet.data,len);
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
int network_listen(int ip_adress, struct Packet* packet,int timeout){
    struct Frame *frame = malloc(sizeof(struct Frame));

    //printf("entra1");
    int res = listen_addr(frame,resolveIPToMAC(ip_adress),"0.0.0.0",ip_adress,timeout); 
    if (res!=0)
    {
        free(frame);
        return res;
    }
    
    //printf("entra2");
    //printf("dest: %d \nsource %d  \ndata %d\n",frame->destinationAdress,frame->sourceAdress,*(((int *)&frame->data)+2));
    struct Packet pkg = deserializePacket(frame->data);
    printf("%d",frame->data);
    //memcpy(packet,&pkg,sizeof(struct Packet));

    free(frame);
    return 0;

}
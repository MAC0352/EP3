#ifndef PACKET_H
#define PACKET_H

struct Packet
{
    int headerLength;
    int totalLength;
    int sourceAddress;
    int destinationAddress;
    void* data;

};

#endif
#ifndef SEGMENT_H
#define SEGMENT_H

#define SEG_MAX_DATA 7900

/* Flags do segmento */
#define FLAG_ACK  0x01
#define FLAG_SYN  0x02
#define FLAG_FIN  0x04

/*
* Segmento da camada de transporte.
*
* Cabeçalho serializado:
*   sourcePort(4) | destinationPort(4) | seqNumber(4) | ackNumber(4)
*   | checksum(4) | flags(4) | data_size(4)
* = 7 × 4 = 28 bytes de cabeçalho.
*/
struct Segment {
    int sourcePort;
    int destinationPort;
    int seqNumber;
    int ackNumber;
    // soma simples dos bytes do payload (módulo 2^32)
    int checksum;   
    int flags;
    int data_size;
    char data[SEG_MAX_DATA];
};

#endif

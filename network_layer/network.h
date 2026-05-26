#ifndef NETWORK_H
#define NETWORK_H

#include "packet.h"

// desencapsula pacote recebido
struct Packet deserializePacket(void* pkgData);

// encapsula pacote para o envio 
void* serializePacket(struct Packet packet);

/* Envia 'data' (data_size bytes) encapsulado em Packet de ip_source → ip_destination. */
int network_send(int ip_destination, int ip_source, void *data, int data_size);

/*
 * Escuta até receber um Packet destinado a ip_address.
 * timeout_us: timeout em microssegundos (0 = sem timeout).
 * Se src_ip não for NULL, preenchido com o sourceAddress do pacote.
 * Retorna 0 em sucesso, 1 em timeout/erro.
 */
int network_listen(int ip_address, struct Packet *packet,
                   char *host, char *port,
                   int timeout_us, int *src_ip);

// simula protocolo ARP
int resolveIPToMAC(int ip_adress);
int resolveMACToIP(int mac_adress);

#endif
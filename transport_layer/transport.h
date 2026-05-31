#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "segment.h"
#include "network_layer/network.h"

// Timeout de retransmissão em microssegundos
#define TRANSPORT_TIMEOUT_US  2000000  
#define TRANSPORT_MAX_RETRIES 3

extern int g_reliable;

/*
* Envia 'data' (data_size bytes) de forma confiável usando  rdt3.0
* (stop-and-wait com ACK + retransmissão por timeout).
*
*   ip_src / ip_dst   : endereços da camada de rede
*   port_src / port_dst: portas lógicas (multiplexação)
*   seq               : número de sequência inicial (0 ou 1)
*   listen_host/port  : onde este nó escuta para receber o ACK
*
* Retorna 0 em sucesso, -1 se excedeu MAX_RETRIES.
*/
int transport_send(int ip_dst, int ip_src,
                   int port_dst, int port_src,
                   int seq,
                   const char *listen_host, const char *listen_port,
                   void *data, int data_size);

/*
* Recebe um segmento de dados destinado a ip_address:port.
* Envia ACK automaticamente.
* Preenche buf (até buf_size bytes) com o payload.
* Retorna número de bytes recebidos, ou -1 em erro/timeout.
*
*   expected_seq : sequência esperada (0 ou 1); atualizada pelo chamador.
*/
int transport_recv(int ip_address, int port,
                   const char *listen_host, const char *listen_port,
                   int expected_seq, int timeout_us,
                   void *buf, int buf_size);

#endif
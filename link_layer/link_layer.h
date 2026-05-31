#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include "frame.h"
#include <pthread.h>

// #define LOSS_RATE .1
// #define AVARAGE_DELAY 100 //miliseconds
// #define OFSET_DELAY 70

#define MAXDATASIZE 8000

extern double g_loss_rate;
extern int g_average_delay;
extern int g_offset_delay;
extern int g_reliable;

int load_link_config(const char *filename);

/* Caminho para o arquivo de hosts. Pode ser sobrescrito chamando
 * link_set_hosts_file() antes de qualquer send_packet. */
extern const char *g_hosts_file;
void link_set_hosts_file(const char *path);

/*
 * Servidor TCP persistente: deve ser iniciado uma vez por processo
 * antes de qualquer chamada a listen_addr.
 * Retorna 0 em sucesso, -1 em erro.
 */
int link_server_start(char* host, char* port);

int send_packet(int mac_destination, int mac_source, void* data, int data_size);

/*
 * Aguarda o próximo Frame destinado a mac_adress no servidor persistente.
 * timeout: em microssegundos.
 * Retorna 0 em sucesso (frame->data alocado com malloc), 1 em timeout/erro.
 */
int listen_addr(struct Frame *frame, int mac_adress, char* host, char* port, int timeout);

#endif
#include "frame.h"

#define LOSS_RATE .1
#define AVARAGE_DELAY 100 //miliseconds
#define OFSET_DELAY 70
#define FILE_HOSTS "hosts"

#define MAXDATASIZE 8000

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

int validateChecksum(struct Frame frame);
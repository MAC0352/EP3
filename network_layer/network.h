#include "packet.h"

//desencapsula pacote recebido
struct Packet deserializePacket(void* packet);

//encapsula pacote para o envio 
void* serializePacket(struct Packet packet);

int send(int ip_destination, int ip_source, void* data);

int listen(int ip_adress, void* data);

//simula protocolo ARP
int resolveIPToMAC(int ip_adress);
int resolveMACToIP(int mac_adress);

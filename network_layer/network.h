#include "packet.h"

//desencapsula pacote recebido
struct Packet deserializePacket(void* pkgData);

//encapsula pacote para o envio 
void* serializePacket(struct Packet packet);

int network_send(int ip_destination, int ip_source, void* data,int size);

int network_listen(int ip_adress, struct Packet*,int timeout);

//simula protocolo ARP
int resolveIPToMAC(int ip_adress);
int resolveMACToIP(int mac_adress);

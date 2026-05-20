#include "segment.h"
#include "network_layer/network.h"

//desencapsula segmento recebido
struct Segment deserializeSegment(void* segment);

//encapsula segmento para o envio 
void* serializeSegment(struct Segment segment);

int send(int ip_destination, int ip_source, int port_destination, int port_source, void* data);

int listen(int ip_adress, int port, void* data);
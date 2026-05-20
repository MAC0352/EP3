#include "frame.h"

#define LOSS_RATE .1
#define AVARAGE_DELAY 100 //miliseconds
#define OFSET_DELAY 70

int send_packet(int mac_destination, int mac_source,void* data, int data_size);
int listen_addr(void* data, int mac_adress);

int validateChecksum(struct Frame frame);
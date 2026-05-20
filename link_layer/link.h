#include "frame.h"

#define LOSS_RATE .1
#define AVARAGE_DELAY 100 
#define OFSET_DELAY 50

int send(int mac_destination, int mac_source,void* data, int data_size);
int listen(void* data, int mac_adress);

int validateChecksum(struct Frame frame);
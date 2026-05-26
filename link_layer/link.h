#include "frame.h"

#define LOSS_RATE .1
#define AVARAGE_DELAY 100 //miliseconds
#define OFSET_DELAY 70
#define FILE_HOSTS "hosts"

#define MAXDATASIZE 8000

int send_packet(int mac_destination, int mac_source,void* data, int data_size);
int listen_addr(struct Frame *frame, int mac_adress,char* host,char* port, int timeout);

int validateChecksum(struct Frame frame);
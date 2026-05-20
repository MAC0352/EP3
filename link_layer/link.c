#include "link.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
int send_packet(int mac_destination, int mac_source, void* data, int data_size){
    //packet loss
    if ((float)rand()/(float)RAND_MAX <= LOSS_RATE)
    {
        return 0;
    }
    int delay = AVARAGE_DELAY - OFSET_DELAY + rand()%(2*OFSET_DELAY+1);
    delay *=1000;
    usleep(delay);

    return 1;
}
#include "link.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
int send_packet(int mac_destination, int mac_source, void* data, int data_size){
    //packet loss
    if ((float)rand()/(float)RAND_MAX <= LOSS_RATE)
    {
        return 0;
    }


    int delay = AVARAGE_DELAY - OFSET_DELAY + rand()%(2*OFSET_DELAY+1);
    delay *=1000;
    usleep(delay);

    //adicionar mac no pacote
    void* linkData;
    linkData = malloc(2*sizeof(int)+data_size);
    memcpy(linkData,&mac_destination,sizeof(mac_destination));
    memcpy(linkData+sizeof(mac_destination),&mac_source,sizeof(mac_source));
    memcpy(linkData+sizeof(mac_destination)+sizeof(mac_source),data,data_size);

    //ler arquivo hosts e enviar para os IPs reais 
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(FILE_HOSTS, "r");
    if (fp == NULL){
        printf("no hosts file");
        free(linkData);
        return 1;
    }
    

    while ((read = getline(&line, &len, fp)) != -1) {
        
        char *saveptr;
        char *host = strtok_r(line, ":", &saveptr);
        char *port = strtok_r(NULL, ":", &saveptr);
        if (host == NULL || port == NULL)
        {
            printf("hosts file must to have lines in the format: <ip>:<port>\n");
            free(linkData);
            return 1;
        }
        
        
        //enviando o pacote pelo soquete TCP

        struct addrinfo hints = {0}, *res = NULL;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {free(linkData); return -1;}

        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { freeaddrinfo(res); free(linkData); return -1; }


        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            freeaddrinfo(res); close(fd);free(linkData); return -1;
        }
        freeaddrinfo(res);
        

        write(fd,linkData,data_size+2*sizeof(int));

        free(linkData);

        close(fd);

    }

    fclose(fp);
    if (line)
        free(line);
    
    return 0;
}


int listen_addr(void* data, int mac_adress,char* host,char* port){
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { return 1; }

    //int yes = 1;
    //setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)atoi(port));

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv); return 1;
    }
    if (listen(srv, 16) < 0) {
        close(srv); return 1;
    }
}

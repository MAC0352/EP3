#include "link.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

int send_packet(int mac_destination, int mac_source, void* data, int data_size){
    //packet loss
    if ((float)rand()/(float)RAND_MAX <= LOSS_RATE)
    {
        return 0;
    }

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

        int resc = connect(fd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (resc < 0) {
            close(fd);free(linkData); continue; //se não conseguir conectar apenas ignora
        }
        

        write(fd,linkData,data_size+2*sizeof(int));

        free(linkData);

        close(fd);

    }

    fclose(fp);
    if (line)
        free(line);
    
    return 0;
}


int listen_addr(struct Frame *frame, int mac_adress,char* host,int port, int timeout){
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { return -1; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv); return -1;
    }
    if (listen(srv, 16) < 0) {
        close(srv); return -1;
    }

    int acp =  0;
    
    //variaveis para mudar o timeout cada vez que umpacote é recebido
    struct timeval tvbegin;
    struct timeval tvnow;
    struct timeval tvend;
    struct timeval tvout={.tv_sec=timeout/1000000,.tv_usec=timeout%1000000}; 
    gettimeofday(&tvbegin, NULL);
    timeradd(&tvbegin,&tvout,&tvend);

    //verifica se o pacote recebido tem o mac do host 
    while (acp==0){

        gettimeofday(&tvnow, NULL);
        if (timercmp(&tvnow,&tvend,>))
        {
            close(srv);
            return 1;
        }
        timersub(&tvend,&tvnow,&tvout);

        setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tvout, sizeof(struct timeval));
        int pkgFd = accept(srv, NULL, NULL);

        int rcv_mac[1];
        recv(pkgFd, rcv_mac,sizeof(int),0);
        if (rcv_mac[0]==mac_adress)
        {
            void* dados = malloc(MAXDATASIZE);
            frame->destinationAdress = mac_adress;
            recv(pkgFd, &frame->sourceAdress,sizeof(int),0);
            recv(pkgFd, &frame->data,MAXDATASIZE,0);
            
            acp=1;
        }

        
        
    }
    close(srv);

    int delay = AVARAGE_DELAY - OFSET_DELAY + rand()%(2*OFSET_DELAY+1);
    delay *=1000;
    usleep(delay);

    return 0;
    
}

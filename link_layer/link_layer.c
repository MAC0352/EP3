#include "link_layer.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

const char *g_hosts_file = "hosts";

double g_loss_rate   = 0.10;
int    g_average_delay = 100;
int    g_offset_delay  = 70;
int    g_reliable      = 1;

// Carregar configurações
int load_link_config(const char *filename)
{
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        perror("config");
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), fp)) {

        if (strncmp(line, "loss_rate=", 10) == 0) {
            g_loss_rate = atof(line + 10);
        }

        else if (strncmp(line, "average_delay=", 14) == 0) {
            g_average_delay = atoi(line + 14);
        }

        else if (strncmp(line, "offset_delay=", 13) == 0) {
            g_offset_delay = atoi(line + 13);
        }

        else if (strncmp(line, "reliable=", 9) == 0) {
            g_reliable = atoi(line + 9);
        }
    }

    fclose(fp);

    printf(
        "[config] loss=%.2f delay=%d offset=%d reliable=%d\n",
        g_loss_rate,
        g_average_delay,
        g_offset_delay,
        g_reliable
    );

    return 0;
}

void link_set_hosts_file(const char *path) { g_hosts_file = path; }

// Helpers

static ssize_t write_all(int fd, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = write(fd, (const char *)buf + sent, n - sent);
        if (r <= 0) return -1;
        sent += r;
    }
    return (ssize_t)sent;
}

static ssize_t read_all(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char *)buf + got, n - got);
        if (r <= 0) return -1;
        got += r;
    }
    return (ssize_t)got;
}

/* Servidor TCP persistente  
 * O design original abre um servidor TCP a cada chamada de listen_addr,
 * o que causa conflito quando múltiplas threads escutam ao mesmo tempo.
 * Solução: um único servidor que roda em background e deposita frames
 * numa fila interna; listen_addr consome dessa fila.
 */

#define QUEUE_CAP 32

typedef struct {
    struct Frame *frames[QUEUE_CAP];
    int head, tail, count;
    pthread_mutex_t mtx;
    pthread_cond_t  cond;
} FrameQueue;

static FrameQueue g_queue = {
    .head = 0, .tail = 0, .count = 0,
    .mtx  = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};

static void queue_push(struct Frame *f) {
    pthread_mutex_lock(&g_queue.mtx);
    if (g_queue.count < QUEUE_CAP) {
        g_queue.frames[g_queue.tail] = f;
        g_queue.tail = (g_queue.tail + 1) % QUEUE_CAP;
        g_queue.count++;
        pthread_cond_broadcast(&g_queue.cond);
    } else {
        free(f->data); free(f); /* descarta se fila cheia */
    }
    pthread_mutex_unlock(&g_queue.mtx);
}

/* Retira o primeiro frame com destinationAdress == mac, ou NULL em timeout. */
static struct Frame *queue_pop(int mac, int timeout_us) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_us / 1000000;
    ts.tv_nsec += (timeout_us % 1000000) * 1000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

    pthread_mutex_lock(&g_queue.mtx);
    while (1) {
        /* procura frame para este mac */
        for (int i = 0; i < g_queue.count; i++) {
            int idx = (g_queue.head + i) % QUEUE_CAP;
            struct Frame *f = g_queue.frames[idx];
            if (f->destinationAdress == mac) {
                /* remove do meio: shift */
                for (int j = i; j < g_queue.count - 1; j++) {
                    int a = (g_queue.head + j)     % QUEUE_CAP;
                    int b = (g_queue.head + j + 1) % QUEUE_CAP;
                    g_queue.frames[a] = g_queue.frames[b];
                }
                g_queue.tail = (g_queue.tail - 1 + QUEUE_CAP) % QUEUE_CAP;
                g_queue.count--;
                pthread_mutex_unlock(&g_queue.mtx);
                return f;
            }
        }
        /* aguarda com timeout */
        int r = pthread_cond_timedwait(&g_queue.cond, &g_queue.mtx, &ts);
        if (r != 0) { /* ETIMEDOUT */
            pthread_mutex_unlock(&g_queue.mtx);
            return NULL;
        }
    }
}

// Thread do servidor: aceita conexões e empurra frames na fila.
static void *server_thread(void *arg) {
    int srv = *(int *)arg;
    free(arg);
    while (1) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) continue;

        int hdr[3];
        if (read_all(cli, hdr, sizeof(hdr)) < 0) { close(cli); continue; }

        int dst  = hdr[0];
        int src  = hdr[1];
        int size = hdr[2];

        if (size <= 0 || size > MAXDATASIZE) { close(cli); continue; }

        struct Frame *f = malloc(sizeof(struct Frame));
        f->data = malloc(size);
        if (read_all(cli, f->data, size) < 0) {
            free(f->data); free(f); close(cli); continue;
        }
        f->destinationAdress = dst;
        f->sourceAdress      = src;
        close(cli);

        queue_push(f);
    }
    return NULL;
}

// link_server_start

int link_server_start(char* host, char* port) {
    (void)host;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("link_server_start: socket"); return -1; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)atoi(port));

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("link_server_start: bind"); close(srv); return -1;
    }
    if (listen(srv, 32) < 0) {
        perror("link_server_start: listen"); close(srv); return -1;
    }

    int *srv_ptr = malloc(sizeof(int));
    *srv_ptr = srv;
    pthread_t tid;
    pthread_create(&tid, NULL, server_thread, srv_ptr);
    pthread_detach(tid);
    return 0;
}

int send_packet(int mac_destination, int mac_source, void* data, int data_size){
    //packet loss
    if ((float)rand()/(float)RAND_MAX <= g_loss_rate)
    {
        return 0;
    }

    // wire format: [dst(4)][src(4)][size(4)][data]
    int total = 3*sizeof(int) + data_size;
    void* linkData = malloc(total);
    memcpy(linkData,             &mac_destination, sizeof(int));
    memcpy(linkData+sizeof(int), &mac_source,      sizeof(int));
    memcpy(linkData+2*sizeof(int), &data_size,     sizeof(int));
    memcpy(linkData+3*sizeof(int), data,           data_size);

    //ler arquivo hosts e enviar para os IPs reais 
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(g_hosts_file, "r");
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
            fclose(fp);
            if (line) free(line);
            return 1;
        }
        port[strcspn(port, "\r\n")] = '\0';

        //enviando o pacote pelo soquete TCP
        struct addrinfo hints = {0}, *res = NULL;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, port, &hints, &res) != 0 || !res) continue;

        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { freeaddrinfo(res); continue; }

        int resc = connect(fd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (resc < 0) {
            close(fd); continue; //se não conseguir conectar apenas ignora
        }

        write_all(fd, linkData, total);
        close(fd);
    }

    free(linkData);
    fclose(fp);
    if (line)
        free(line);

    //delay de envio
    int delay = g_average_delay - g_offset_delay + rand()%(2*g_offset_delay+1);
    usleep(delay * 1000);
    
    return 0;
}

int listen_addr(struct Frame *frame, int mac_adress, char* host, char* port, int timeout){
    (void)host; (void)port; // servidor já iniciado por link_server_start

    struct Frame *f = queue_pop(mac_adress, timeout);
    if (!f) return 1; // timeout

    frame->destinationAdress = f->destinationAdress;
    frame->sourceAdress      = f->sourceAdress;
    frame->data              = f->data; // transfere ownership
    free(f);
    return 0;
}
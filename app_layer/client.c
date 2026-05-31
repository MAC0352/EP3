/*
* client.c — Aplicação de chat sobre a mini-pilha de rede.
*
* Uso:
*   ./client <meu_ip_logico> <ip_destino> <porta_src> <porta_dst> \
*            <host_escuta> <porta_escuta>
*
* Exemplo (duas janelas de terminal):
*   ./client 1 2 1001 2001 0.0.0.0 7777   // nó Alice
*   ./client 2 1 2001 1001 0.0.0.0 7777   // nó Bob
*
* O arquivo 'hosts' deve conter o destino no formato  ip:porta.
* No smoke test ambos escutam na mesma porta (7777) porque link_layer
* filtra por MAC (IP lógico).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../transport_layer/transport.h"
#include "../link_layer/link_layer.h"

// Contexto compartilhado com a thread receptora
typedef struct {
    int         my_ip;
    int         port;
    const char *host;
    const char *listen_port;
    // último seq esperado (alterna 0|1)
    int         seq;     
} RecvCtx;

static void *recv_thread(void *arg) {
    RecvCtx *ctx = arg;
    char buf[SEG_MAX_DATA + 1];
    int expected_seq = 0;

    for (;;) {
        int n = transport_recv(ctx->my_ip, ctx->port,
                               ctx->host, ctx->listen_port,
                               expected_seq,
                               // 10 s de timeout
                               10000000, 
                               buf, SEG_MAX_DATA);
        if (n < 0) {
            // timeout -> tenta novamente
            continue;
        }
        buf[n] = '\0';
        printf("\n[recebido] %s\n> ", buf);
        fflush(stdout);
        // Alterna 0 e 1
        expected_seq ^= 1; 
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr,
            "Uso: %s <meu_ip> <ip_dst> <porta_src> <porta_dst> "
            "<host_escuta> <porta_escuta>\n", argv[0]);
        return 1;
    }

    int my_ip      = atoi(argv[1]);
    int dst_ip     = atoi(argv[2]);
    int port_src   = atoi(argv[3]);
    int port_dst   = atoi(argv[4]);
    const char *host        = argv[5];
    const char *listen_port = argv[6];

    printf("[chat] nó %d → %d  portas %d→%d\n",
           my_ip, dst_ip, port_src, port_dst);
    printf("[chat] escutando em %s:%s\n", host, listen_port);
    printf("Digite mensagens e pressione Enter. Ctrl-C para sair.\n\n");

    // inicia servidor TCP persistente da camada de enlace 
    if (link_server_start((char*)host, (char*)listen_port) != 0) {
        fprintf(stderr, "Erro ao iniciar servidor de enlace\n");
        return 1;
    }
    // 100 ms para o servidor chegar no accept() 
    usleep(100000); 

    // inicia thread receptora 
    RecvCtx ctx = {my_ip, port_src, host, listen_port, 0};
    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, &ctx);
    pthread_detach(tid);

    // loop de envio 
    char line[SEG_MAX_DATA];
    int seq = 0;
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len == 0) continue;

        int r = transport_send(dst_ip, my_ip,
                               port_dst, port_src,
                               seq,
                               host, listen_port,
                               line, len);
        if (r == 0) {
            printf("[enviado] seq=%d\n", seq);
            seq ^= 1;
        } else {
            printf("[erro] mensagem não entregue\n");
        }
    }
    return 0;
}
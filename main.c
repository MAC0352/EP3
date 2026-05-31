/*
 * main.c — Cliente de chat MiniIRC sobre mini-pilha de rede
 *
 * Uso:
 *   ./client <meu_ip> <ip_dst> <porta_src> <porta_dst> \
 *            <host_escuta> <porta_escuta> <username> [--passive]
 *
 * --passive : não envia CONNECT; espera o peer conectar primeiro.
 *             Use em um dos dois lados para evitar que ambos enviem
 *             CONNECT simultaneamente.
 *
 * Exemplos (duas janelas):
 *   ./client 1 2 1001 2001 0.0.0.0 7001 Alice
 *   ./client 2 1 2001 1001 0.0.0.0 7002 Bob --passive
 *
 * Comandos durante o chat:
 *   /quit  -> encerra a sessão
 *   /help  -> lista comandos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "app_layer/app_layer.h"
#include "app_layer/app_protocol.h"
#include "link_layer/link_layer.h"

// Thread receptora

static AppSession *g_sess = NULL; // acesso da thread ao contexto

static void *recv_thread_fn(void *arg) {
    app_recv_loop((AppSession *)arg, NULL);
    printf("[chat] sessão encerrada pelo peer.\n");
    return NULL;
}

// Exibição de ajuda

static void print_help(void) {
    printf("\n  Comandos disponíveis:\n");
    printf("    /quit  — encerra o chat\n");
    printf("    /help  — exibe esta ajuda\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 8) {
        fprintf(stderr,
            "Uso: %s <meu_ip> <ip_dst> <porta_src> <porta_dst>"
            " <host_escuta> <porta_escuta> <username> [--passive]\n",
            argv[0]);
        return 1;
    }

    srand(time(NULL));
    
    int   my_ip      = atoi(argv[1]);
    int   dst_ip     = atoi(argv[2]);
    int   port_src   = atoi(argv[3]);
    int   port_dst   = atoi(argv[4]);
    char *host       = argv[5];
    char *listen_port= argv[6];
    char *username   = argv[7];
    int   passive    = 0;
    const char *hosts_file = "hosts";
    
    for (int i = 8; i < argc; i++) {
        if (strcmp(argv[i], "--passive") == 0) passive = 1;
        else if (strcmp(argv[i], "--hosts") == 0 && i+1 < argc)
        hosts_file = argv[++i];
    }
    link_set_hosts_file(hosts_file);
    load_link_config("config/current.conf");

    // valida username
    if (strlen(username) == 0 || strlen(username) >= 32) {
        fprintf(stderr, "username deve ter entre 1 e 31 caracteres\n");
        return 1;
    }

    AppSession sess = {
        .my_ip       = my_ip,
        .dst_ip      = dst_ip,
        .port_src    = port_src,
        .port_dst    = port_dst,
        .username    = username,
        .listen_host = host,
        .listen_port = listen_port,
    };
    g_sess = &sess;

    // banner
    printf("╔══════════════════════════════════╗\n");
    printf("║        MiniIRC Chat Client       ║\n");
    printf("╚══════════════════════════════════╝\n");
    printf("  Usuário : %s\n", username);
    printf("  IP lógico: %d  →  %d\n", my_ip, dst_ip);
    printf("  Escutando: %s:%s\n\n", host, listen_port);

    // inicia servidor de enlace
    if (link_server_start(host, listen_port) != 0) {
        fprintf(stderr, "Erro ao iniciar servidor de enlace na porta %s\n",
                listen_port);
        return 1;
    }
    usleep(100000); // 100 ms para o accept() estar pronto

    // handshake de sessão
    if (passive) {
        // modo passivo: sobe thread receptora e espera o CONNECT chegar 
        printf("[sessão] aguardando conexão de %d...\n", dst_ip);
        pthread_t tid;
        pthread_create(&tid, NULL, recv_thread_fn, &sess);
        pthread_detach(tid);
        usleep(500000); // dá tempo do CONNECT chegar antes do loop de envio 
    } else {
        // modo ativo: envia CONNECT e espera CONNECTED 
        if (app_connect(&sess) != 0) {
            fprintf(stderr, "Falha ao conectar. O peer está rodando?\n");
            return 1;
        }
        // sobe thread receptora só depois do handshake 
        pthread_t tid;
        pthread_create(&tid, NULL, recv_thread_fn, &sess);
        pthread_detach(tid);
    }

    printf("\nDigite uma mensagem e pressione Enter. /help para ajuda.\n\n");

    // loop de envio (stdin)
    char line[APP_PAYLOAD_LEN + 1];
    while (app_is_running()) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break; /* EOF / Ctrl-D */

        /* remove \n */
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len == 0) continue;

        /* comandos */
        if (strcmp(line, "/quit") == 0) {
            app_disconnect(&sess);
            break;
        }
        if (strcmp(line, "/help") == 0) {
            print_help();
            continue;
        }
        if (line[0] == '/') {
            printf("  Comando desconhecido. /help para ajuda.\n");
            continue;
        }

        /* envia mensagem */
        if (app_send_msg(&sess, line) == 0) {
            // confirmação visual discreta — o peer verá a mensagem
            printf("\033[A\033[2K"); // sobe uma linha e apaga
            printf("\033[1;32m%s\033[0m: %s\n", username, line);
            fflush(stdout);
        } else {
            printf("  [!] mensagem não entregue (peer desconectado?)\n");
        }
    }

    return 0;
}
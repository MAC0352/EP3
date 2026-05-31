#include "app_layer.h"
#include "app_protocol.h"
#include "../transport_layer/transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// Serialização e desserialização 

int app_serialize(const struct AppMessage *msg, char *buf, int buf_size) {
    int total = APP_MSG_HDR_SIZE + msg->payload_len;
    if (total > buf_size) return -1;
    int off = 0;
    memcpy(buf + off, &msg->type,        1);                off += 1;
    memcpy(buf + off,  msg->username,    APP_USERNAME_LEN); off += APP_USERNAME_LEN;
    memcpy(buf + off, &msg->msg_id,      4);                off += 4;
    memcpy(buf + off, &msg->payload_len, 2);                off += 2;
    memcpy(buf + off,  msg->payload,     msg->payload_len);
    return total;
}

int app_deserialize(const char *buf, int buf_size, struct AppMessage *msg) {
    if (buf_size < APP_MSG_HDR_SIZE) return -1;
    int off = 0;
    memcpy(&msg->type,        buf + off, 1);                off += 1;
    memcpy( msg->username,    buf + off, APP_USERNAME_LEN); off += APP_USERNAME_LEN;
    memcpy(&msg->msg_id,      buf + off, 4);                off += 4;
    memcpy(&msg->payload_len, buf + off, 2);                off += 2;
    msg->username[APP_USERNAME_LEN - 1] = '\0';
    if (msg->payload_len > APP_PAYLOAD_LEN) return -1;
    if (buf_size < APP_MSG_HDR_SIZE + (int)msg->payload_len) return -1;
    memcpy(msg->payload, buf + off, msg->payload_len);
    msg->payload[msg->payload_len] = '\0';
    return 0;
}

/* Controle de ciclo de vida e sequência
 *
 * g_sending: quando 1, a recv_thread cede o passo ao remetente.
 * Isso evita que a recv_thread consuma o ACK de transporte que o
 * transport_send está aguardando.
 */
static volatile int g_running  = 1;
static volatile int g_sending  = 0;   // 1 = stdin está enviando agora
static int          g_send_seq = 0;
static int          g_recv_seq = 0;
static unsigned int g_msg_id   = 0;



// Helpers de envio/recepção

static void fill_msg(struct AppMessage *msg, uint8_t type,
                     const char *username, const char *payload) {
    memset(msg, 0, sizeof(*msg));
    msg->type = type;
    strncpy(msg->username, username, APP_USERNAME_LEN - 1);
    if (payload && payload[0]) {
        msg->payload_len = (uint16_t)strlen(payload);
        if (msg->payload_len > APP_PAYLOAD_LEN)
            msg->payload_len = APP_PAYLOAD_LEN;
        memcpy(msg->payload, payload, msg->payload_len);
    }
}

// Envia uma AppMessage. Só avança g_send_seq se o transporte confirmar entrega
static int send_app_msg(AppSession *sess, struct AppMessage *msg) {
    char buf[APP_MSG_MAX_SIZE];
    int n = app_serialize(msg, buf, sizeof(buf));
    if (n < 0) return -1;
    int r = transport_send(sess->dst_ip, sess->my_ip,
                           sess->port_dst, sess->port_src,
                           g_send_seq,
                           sess->listen_host, sess->listen_port,
                           buf, n);
    if (r == 0) g_send_seq ^= 1;   // avança só após confirmar
    return r;
}

// Recebe uma AppMessage. Só avança g_recv_seq se realmente receber algo.
static int recv_app_msg(AppSession *sess, struct AppMessage *msg, int timeout_us) {
    char buf[APP_MSG_MAX_SIZE];
    int n = transport_recv(sess->my_ip, sess->port_src,
                           sess->listen_host, sess->listen_port,
                           g_recv_seq, timeout_us,
                           buf, sizeof(buf));
    if (n < 0) return -1;           // timeout — g_recv_seq não muda
    g_recv_seq ^= 1;                // avança só após recepção real  
    return app_deserialize(buf, n, msg);
}

/*
 *
 * Handshake inspirado no IRC (NICK + USER => 001 Welcome):
 *   -> CONNECT  (username no payload)
 *   <- CONNECTED (username do peer no campo username)
*/
int app_connect(AppSession *sess) {
    struct AppMessage msg;
    fill_msg(&msg, APP_CONNECT, sess->username, sess->username);
    if (send_app_msg(sess, &msg) != 0) {
        fprintf(stderr, "[app] falha ao enviar CONNECT\n");
        return -1;
    }
    printf("[sessão] CONNECT enviado como '%s'\n", sess->username);

    struct AppMessage resp;
    if (recv_app_msg(sess, &resp, 10000000) != 0) {
        fprintf(stderr, "[app] timeout aguardando CONNECTED\n");
        return -1;
    }
    if (resp.type == APP_ERROR) {
        fprintf(stderr, "[app] peer recusou: %s\n", resp.payload);
        return -1;
    }
    if (resp.type != APP_CONNECTED) {
        fprintf(stderr, "[app] resposta inesperada (0x%02x)\n", resp.type);
        return -1;
    }
    printf("[sessão] conectado! Peer: '%s'\n", resp.username);
    sess->session_open = 1;   // lado ativo: sessão aberta após CONNECTED
    return 0;
}

/*
 * 
 * Roda em thread separada. Não usa ACK_MSG — o transporte já confirmou
 * a entrega de cada MSG antes de entregá-la aqui.
 */
void app_recv_loop(AppSession *sess, app_recv_callback cb) {
    struct AppMessage msg;
    char peer_username[APP_USERNAME_LEN] = {0};

    while (g_running) {
        if (g_sending) { usleep(10000); continue; }

        int r = recv_app_msg(sess, &msg, 2000000);
        if (r < 0) continue;

        switch (msg.type) {

        case APP_CONNECT: {
            strncpy(peer_username, msg.username, APP_USERNAME_LEN - 1);
            sess->session_open = 1;
            printf("\n*** '%s' conectou ***\n> ", peer_username);
            fflush(stdout);
            struct AppMessage resp;
            fill_msg(&resp, APP_CONNECTED, sess->username, NULL);
            send_app_msg(sess, &resp);
            break;
        }

        case APP_MSG: {
            if (!sess->session_open) break;
            printf("\n\033[1;36m%s\033[0m: %s\n> ", msg.username, msg.payload);
            fflush(stdout);
            if (cb && cb(msg.username, msg.payload, msg.msg_id) < 0)
                return;
            break;
        }

        case APP_DISCONNECT:
            printf("\n*** '%s' desconectou ***\n", msg.username);
            fflush(stdout);
            g_running = 0;
            return;

        case APP_ERROR:
            fprintf(stderr, "\n[app] erro do peer: %s\n", msg.payload);
            break;

        default:
            break;
        }
    }
}

/*
 * Envia MSG. A confirmação é garantida pelo transporte (ACK de segmento);
 * não há ACK_MSG separado na camada de aplicação — isso simplifica o
 * protocolo e evita conflito de seq entre as duas threads.
 */
int app_send_msg(AppSession *sess, const char *text) {
    struct AppMessage msg;
    fill_msg(&msg, APP_MSG, sess->username, text);
    msg.msg_id = ++g_msg_id;
    g_sending = 1;
    int r = send_app_msg(sess, &msg);
    g_sending = 0;
    if (r != 0) {
        fprintf(stderr, "[app] falha ao enviar MSG\n");
        return -1;
    }
    return 0;
}

void app_disconnect(AppSession *sess) {
    g_running = 0;
    g_sending = 1;
    struct AppMessage msg;
    fill_msg(&msg, APP_DISCONNECT, sess->username, "bye");
    send_app_msg(sess, &msg);
    g_sending = 0;
    printf("[sessão] desconectado.\n");
}

int app_is_running(void) { return g_running; }
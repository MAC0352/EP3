#ifndef APP_LAYER_H
#define APP_LAYER_H

/*
 * app_layer.h — interface da camada de aplicação de chat (MiniIRC)
 *
 * Controle de sessão:
 *   - app_connect()    : inicia sessão com o peer (envia CONNECT, aguarda CONNECTED)
 *   - app_disconnect() : encerra sessão (envia DISCONNECT)
 *
 * Troca de mensagens:
 *   - app_send_msg()   : envia MSG e aguarda ACK_MSG
 *   - app_recv_loop()  : roda na thread receptora; entrega mensagens ao callback
 */

/* Callback chamado quando uma MSG chega. Retorna 0 para continuar, -1 para parar. */
typedef int (*app_recv_callback)(const char *username, const char *text,
                                 unsigned int msg_id);

typedef struct {
    int         my_ip;
    int         dst_ip;
    int         port_src;   // porta local (minha)  
    int         port_dst;   // porta do peer        
    char       *username;   // meu nome de usuário  
    const char *listen_host;
    const char *listen_port;
    int         session_open;
} AppSession;

/*
 * Inicia sessão: envia CONNECT e espera CONNECTED do peer.
 * Retorna 0 em sucesso, -1 em falha.
 */
int app_connect(AppSession *sess);

/*
 * Loop receptor: roda indefinidamente (em thread separada).
 * Chama cb para cada MSG recebida; responde automaticamente com ACK_MSG.
 * Retorna quando recebe DISCONNECT ou cb retornar -1.
 */
void app_recv_loop(AppSession *sess, app_recv_callback cb);

/*
 * Envia uma mensagem de texto e aguarda ACK_MSG.
 * Retorna 0 em sucesso, -1 em falha.
 */
int app_send_msg(AppSession *sess, const char *text);

/*
 * Encerra sessão enviando DISCONNECT.
 */
void app_disconnect(AppSession *sess);

/* Retorna 1 enquanto a sessão estiver ativa, 0 após disconnect. */
int app_is_running(void);

#endif /* APP_LAYER_H */
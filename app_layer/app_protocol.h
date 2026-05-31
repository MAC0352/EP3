#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>

/* Tipos de mensagem (inspirado em IRC/XMPP simplificado)
 *
 * Fluxo de sessão:
 *
 *   Remetente            Destinatário
 *       │── CONNECT ────────►│   "quero entrar com username X"
 *       │◄─ CONNECTED ───────│   "ok, sessão aberta"
 *       │── MSG ─────────────►│   "mensagem de chat"
 *       │◄─ ACK_MSG ──────────│   "recebi"
 *       │── DISCONNECT ──────►│   "encerrando"
 *
 *   Em caso de erro (username já em uso, sessão inválida):
 *       │◄─ ERROR ───────────│
 */

#define APP_CONNECT     0x01   // solicitação de conexão (envia username) 
#define APP_CONNECTED   0x02   // confirmação de sessão pelo destinatário 
#define APP_MSG         0x03   // mensagem de chat                        
#define APP_ACK_MSG     0x04   // confirmação de recebimento de MSG       
#define APP_DISCONNECT  0x05   // encerramento limpo de sessão            
#define APP_ERROR       0x06   // erro de protocolo (payload = descrição) 

#define APP_USERNAME_LEN  32
#define APP_PAYLOAD_LEN  512

/*
 * Cabeçalho serializado na wire (dentro do payload do Segment):
 *
 *   type(1) | username(32) | msg_id(4) | payload_len(2) | payload(≤512)
 *   = 39 bytes de cabeçalho + payload variável
 *
 * Total máximo: 39 + 512 = 551 bytes — bem abaixo do SEG_MAX_DATA (7900).
 */

#pragma pack(push, 1)
struct AppMessage {
    uint8_t  type;
    char     username[APP_USERNAME_LEN];  // null-terminated 
    uint32_t msg_id;                      // id sequencial por remetente 
    uint16_t payload_len;                 // bytes válidos em payload[]  
    char     payload[APP_PAYLOAD_LEN];
};
#pragma pack(pop)

#define APP_MSG_HDR_SIZE  (1 + APP_USERNAME_LEN + 4 + 2)  /* 39 bytes */
#define APP_MSG_MAX_SIZE  (APP_MSG_HDR_SIZE + APP_PAYLOAD_LEN)

/* Serializa msg → buf (retorna bytes escritos, -1 se buf pequeno). */
int app_serialize(const struct AppMessage *msg, char *buf, int buf_size);

/* Desserializa buf → msg (retorna 0 em sucesso, -1 em erro). */
int app_deserialize(const char *buf, int buf_size, struct AppMessage *msg);

#endif /* APP_PROTOCOL_H */

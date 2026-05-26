#include "transport.h"
#include "../network_layer/network.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Serializa tamanho de cabeçalho 
#define SEG_HDR_INTS 7
// Esperado 28 bytes
#define SEG_HDR_SIZE (SEG_HDR_INTS * (int)sizeof(int)) 

// Serialização e desserialização

static void serialize_segment(const struct Segment *s, char *out) {
    int off = 0;
#define COPY_INT(field) \
    memcpy(out + off, &s->field, sizeof(int)); off += sizeof(int)
    COPY_INT(sourcePort);
    COPY_INT(destinationPort);
    COPY_INT(seqNumber);
    COPY_INT(ackNumber);
    COPY_INT(checksum);
    COPY_INT(flags);
    COPY_INT(data_size);
#undef COPY_INT
    memcpy(out + off, s->data, s->data_size);
}

static void deserialize_segment(const char *buf, struct Segment *s) {
    int off = 0;
#define COPY_INT(field) \
    memcpy(&s->field, buf + off, sizeof(int)); off += sizeof(int)
    COPY_INT(sourcePort);
    COPY_INT(destinationPort);
    COPY_INT(seqNumber);
    COPY_INT(ackNumber);
    COPY_INT(checksum);
    COPY_INT(flags);
    COPY_INT(data_size);
#undef COPY_INT
    if (s->data_size < 0 || s->data_size > SEG_MAX_DATA)
        s->data_size = 0;
    memcpy(s->data, buf + off, s->data_size);
}

// Checksum simples (soma de bytes mod 2^32)

static int compute_checksum(const char *data, int size) {
    unsigned int sum = 0;
    for (int i = 0; i < size; i++)
        sum += (unsigned char)data[i];
    return (int)sum;
}

// Helpers de envio/recepção de segmento via rede

static int send_segment(const struct Segment *seg,
                        int ip_dst, int ip_src) {
    char buf[SEG_HDR_SIZE + SEG_MAX_DATA];
    serialize_segment(seg, buf);
    int total = SEG_HDR_SIZE + seg->data_size;
    return network_send(ip_dst, ip_src, buf, total);
}

/*
 * Recebe um segmento da camada de rede. Filtra por ip_address e port.
* Retorna 0 se recebeu segmento válido destinado a nós, 1 em timeout/erro.
*/
static int recv_segment(struct Segment *seg,
                        int ip_address, int port,
                        const char *host, const char *listen_port,
                        int timeout_us, int *out_src_ip) {
    struct Packet pkt;
    int src_ip = 0;
    if (network_listen(ip_address, &pkt, (char*)host, (char*)listen_port, timeout_us, &src_ip) != 0)
        return 1;

    int pkt_payload_size = pkt.totalLength - pkt.headerLength;
    if (pkt_payload_size < SEG_HDR_SIZE) {
        fprintf(stderr, "[transport] pacote curto demais\n");
        free(pkt.data);
        return 1;
    }

    deserialize_segment(pkt.data, seg);
    free(pkt.data);

    if (seg->destinationPort != port) {
        fprintf(stderr, "[transport] segmento para porta %d ignorado (esperava %d)\n",
                seg->destinationPort, port);
        return 1;
    }

    if (out_src_ip) *out_src_ip = src_ip;
    return 0;
}

// transport_send com RDT 3.0 (stop-and-wait) / sem pipelining
int transport_send(int ip_dst, int ip_src,
                   int port_dst, int port_src,
                   int seq,
                   const char *listen_host, const char *listen_port,
                   void *data, int data_size) {
    if (data_size > SEG_MAX_DATA) {
        fprintf(stderr, "[transport] payload %d excede SEG_MAX_DATA\n", data_size);
        return -1;
    }

    struct Segment seg;
    seg.sourcePort      = port_src;
    seg.destinationPort = port_dst;
    seg.seqNumber       = seq;
    seg.ackNumber       = 0;
    seg.flags           = 0;
    seg.data_size       = data_size;
    memcpy(seg.data, data, data_size);
    seg.checksum        = compute_checksum(seg.data, seg.data_size);

    int attempts = 0;
    while (attempts < TRANSPORT_MAX_RETRIES) {
        attempts++;
        fprintf(stderr, "[transport] enviando seq=%d tentativa=%d\n",
                seq, attempts);

        if (send_segment(&seg, ip_dst, ip_src) != 0) {
            fprintf(stderr, "[transport] erro no envio\n");
            continue;
        }

        // aguarda ACK
        struct Segment ack_seg;
        int r = recv_segment(&ack_seg, ip_src, port_src,
                             listen_host, listen_port,
                             TRANSPORT_TIMEOUT_US, NULL);

        if (r != 0) {
            fprintf(stderr, "[transport] timeout aguardando ACK seq=%d\n", seq);
            // retransmissão
            continue;
        }

        // verifica se é ACK com ackNumber correto
        if (!(ack_seg.flags & FLAG_ACK)) {
            fprintf(stderr, "[transport] resposta sem FLAG_ACK\n");
            continue;
        }
        if (ack_seg.ackNumber != seq) {
            fprintf(stderr, "[transport] ACK com ackNumber=%d esperava=%d\n",
                    ack_seg.ackNumber, seq);
            // ACK duplicado / de rodada anterior; retransmite
            continue; 
        }

        fprintf(stderr, "[transport] ACK recebido para seq=%d\n", seq);
        return 0;
    }

    fprintf(stderr, "[transport] FALHA: máximo de tentativas atingido (seq=%d)\n", seq);
    return -1;
}

int transport_recv(int ip_address, int port,
                   const char *listen_host, const char *listen_port,
                   int expected_seq, int timeout_us,
                   void *buf, int buf_size) {
    struct Segment seg;

    // Loop para lidar com segmentos duplicados (ACK anterior pode ter se perdido)
    for (;;) {
        int src_ip = 0;
        int r = recv_segment(&seg, ip_address, port,
                             listen_host, listen_port, timeout_us, &src_ip);
        if (r != 0) return -1; // timeout ou erro

        // valida checksum
        int expected_cs = compute_checksum(seg.data, seg.data_size);
        if (seg.checksum != expected_cs) {
            fprintf(stderr, "[transport] checksum inválido, descartando\n");
            // remetente vai retransmitir
            continue; 
        }

        // monta ACK
        struct Segment ack;
        memset(&ack, 0, sizeof(ack));
        ack.sourcePort      = port;
        ack.destinationPort = seg.sourcePort;
        ack.seqNumber       = 0;
        ack.ackNumber       = seg.seqNumber;
        ack.flags           = FLAG_ACK;
        ack.data_size       = 0;
        ack.checksum        = 0;

        // envia ACK de volta para o IP de origem do segmento
        send_segment(&ack, src_ip, ip_address);

        if (seg.seqNumber != expected_seq) {
            fprintf(stderr, "[transport] seq=%d duplicado (esperava=%d), ACK reenviado\n",
                    seg.seqNumber, expected_seq);
            continue;
        }

        int copy = seg.data_size < buf_size ? seg.data_size : buf_size;
        memcpy(buf, seg.data, copy);
        fprintf(stderr, "[transport] segmento seq=%d recebido (%d bytes)\n",
                seg.seqNumber, copy);
        return copy;
    }
}
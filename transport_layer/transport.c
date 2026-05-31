#include "transport.h"
#include "../network_layer/network.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Tamanho do cabeçalho serializado
#define SEG_HDR_INTS 7
#define SEG_HDR_SIZE (SEG_HDR_INTS * (int)sizeof(int))  /* 28 bytes */

// Serialização e desserialização

static void serialize_segment(const struct Segment *s, char *out) {
    int off = 0;
#define COPY_INT(field) \
    memcpy(out + off, &s->field, sizeof(int)); off += sizeof(int)
    COPY_INT(sourcePort); COPY_INT(destinationPort); COPY_INT(seqNumber);
    COPY_INT(ackNumber);  COPY_INT(checksum);        COPY_INT(flags);
    COPY_INT(data_size);
#undef COPY_INT
    memcpy(out + off, s->data, s->data_size);
}

static void deserialize_segment(const char *buf, struct Segment *s) {
    int off = 0;
#define COPY_INT(field) \
    memcpy(&s->field, buf + off, sizeof(int)); off += sizeof(int)
    COPY_INT(sourcePort); COPY_INT(destinationPort); COPY_INT(seqNumber);
    COPY_INT(ackNumber);  COPY_INT(checksum);        COPY_INT(flags);
    COPY_INT(data_size);
#undef COPY_INT
    if (s->data_size < 0 || s->data_size > SEG_MAX_DATA) s->data_size = 0;
    memcpy(s->data, buf + off, s->data_size);
}

static int compute_checksum(const char *data, int size) {
    unsigned int sum = 0;
    for (int i = 0; i < size; i++) sum += (unsigned char)data[i];
    return (int)sum;
}

// Helpers de envio/recepção

static int send_segment(const struct Segment *seg, int ip_dst, int ip_src) {
    char buf[SEG_HDR_SIZE + SEG_MAX_DATA];
    serialize_segment(seg, buf);
    return network_send(ip_dst, ip_src, buf, SEG_HDR_SIZE + seg->data_size);
}

/*
 * Recebe um segmento com deadline absoluto.
 * remaining_us: ponteiro para microssegundos restantes — decrementado a cada
 * chamada. Retorna 0 se recebeu segmento válido para (ip_address, port).
 * Retorna 1 se o deadline expirou.
 */
static int recv_segment_deadline(struct Segment *seg,
                                 int ip_address, int port,
                                 const char *host, const char *listen_port,
                                 int *remaining_us, int *out_src_ip) {
    if (*remaining_us <= 0) return 1;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    struct Packet pkt;
    int src_ip = 0;
    int r = network_listen(ip_address, &pkt, (char*)host, (char*)listen_port,
                           *remaining_us, &src_ip);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    int elapsed = (int)((t1.tv_sec - t0.tv_sec) * 1000000
                        + (t1.tv_nsec - t0.tv_nsec) / 1000);
    *remaining_us -= elapsed;

    if (r != 0) return 1; /* timeout */

    int pkt_payload = pkt.totalLength - pkt.headerLength;
    if (pkt_payload < SEG_HDR_SIZE) { free(pkt.data); return 1; }

    deserialize_segment(pkt.data, seg);
    free(pkt.data);

    if (seg->destinationPort != port) return 1; /* não é para esta porta */

    if (out_src_ip) *out_src_ip = src_ip;
    return 0;
}

// transport_send — RDT 3.0

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

    if (!g_reliable) {
        fprintf(stderr,
            "[transport] modo nao confiavel\n");

        return send_segment(&seg, ip_dst, ip_src);
    }

    for (int attempt = 1; attempt <= TRANSPORT_MAX_RETRIES; attempt++) {
        fprintf(stderr, "[transport] enviando seq=%d tentativa=%d\n", seq, attempt);

        if (send_segment(&seg, ip_dst, ip_src) != 0) {
            fprintf(stderr, "[transport] erro no envio\n");
            continue;
        }

        /* aguarda ACK com deadline absoluto para esta tentativa */
        int remaining = TRANSPORT_TIMEOUT_US;
        while (remaining > 0) {
            struct Segment ack;
            int r = recv_segment_deadline(&ack, ip_src, port_src,
                                          listen_host, listen_port,
                                          &remaining, NULL);
            if (r != 0) {
                fprintf(stderr, "[transport] timeout aguardando ACK seq=%d\n", seq);
                break; /* retransmite */
            }
            if (!(ack.flags & FLAG_ACK)) continue; /* segmento de dados — descarta */
            if (ack.ackNumber != seq)   continue;  /* ACK stale — descarta */

            fprintf(stderr, "[transport] ACK recebido para seq=%d\n", seq);
            return 0;
        }
    }

    fprintf(stderr, "[transport] FALHA: máximo de tentativas atingido (seq=%d)\n", seq);
    return -1;
}

// transport_recv

int transport_recv(int ip_address, int port,
                   const char *listen_host, const char *listen_port,
                   int expected_seq, int timeout_us,
                   void *buf, int buf_size) {
    int remaining = timeout_us;

    while (remaining > 0) {
        struct Segment seg;
        int src_ip = 0;
        int r = recv_segment_deadline(&seg, ip_address, port,
                                      listen_host, listen_port,
                                      &remaining, &src_ip);
        if (r != 0) return -1; /* deadline expirou */

        /* ignora ACKs puros — são respostas de transporte, não dados */
        if (seg.flags & FLAG_ACK) continue;

        /* valida checksum */
        if (seg.checksum != compute_checksum(seg.data, seg.data_size)) continue;

        /* monta e envia ACK — sempre, mesmo para duplicatas */
        struct Segment ack = {0};
        ack.sourcePort      = port;
        ack.destinationPort = seg.sourcePort;
        ack.ackNumber       = seg.seqNumber;
        ack.flags           = FLAG_ACK;
        send_segment(&ack, src_ip, ip_address);

        if (seg.seqNumber != expected_seq) continue; /* duplicata — ACK enviado, aguarda o correto */

        int copy = seg.data_size < buf_size ? seg.data_size : buf_size;
        memcpy(buf, seg.data, copy);
        fprintf(stderr, "[transport] segmento seq=%d recebido (%d bytes)\n",
                seg.seqNumber, copy);
        return copy;
    }
    return -1;
}
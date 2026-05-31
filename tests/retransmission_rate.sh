#!/bin/bash

LOG=$1

ATTEMPTS=$(grep -c "tentativa=" "$LOG")
ACKS=$(grep -c "ACK recebido" "$LOG")

RETRANS=$((ATTEMPTS - ACKS))

echo "Tentativas: $ATTEMPTS"
echo "Mensagens entregues: $ACKS"
echo "Retransmissões: $RETRANS"

awk -v r="$RETRANS" -v a="$ACKS" '
BEGIN{
if(a==0){print "Taxa: indefinida"; exit}
printf("Taxa de retransmissao: %.2f%%\n",(r/a)*100)
}'

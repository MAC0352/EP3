#!/bin/bash

LOSS_VALUES=("0.0" "0.1" "0.3" "0.5" "0.7")

for LOSS in "${LOSS_VALUES[@]}"
do
echo "====================================="
echo "LOSS_RATE=$LOSS"
echo "====================================="

```
cp config/base.conf config/current.conf

sed -i "s/^loss_rate=.*/loss_rate=$LOSS/" config/current.conf

./run_test.sh > logs/loss_${LOSS}.log 2>&1

ACKS=$(grep -c "ACK recebido" logs/loss_${LOSS}.log)
RETR=$(grep -c "tentativa=" logs/loss_${LOSS}.log)

echo "ACKs: $ACKS"
echo "Tentativas: $RETR"
```

done

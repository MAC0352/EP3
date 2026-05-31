#!/bin/bash

DELAYS=(50 100 250 500 1000)

for DELAY in "${DELAYS[@]}"
do
echo "====================================="
echo "DELAY=$DELAY ms"
echo "====================================="

```
cp config/base.conf config/current.conf

sed -i "s/^average_delay=.*/average_delay=$DELAY/" config/current.conf

START=$(date +%s%N)

./run_test.sh > logs/delay_${DELAY}.log 2>&1

END=$(date +%s%N)

ELAPSED=$(( (END-START)/1000000 ))

echo "Tempo total: $ELAPSED ms"
```

done

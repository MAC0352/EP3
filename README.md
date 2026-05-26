# EP3 MAC0352
## simulação da parte física
Sempre que um pacote é enviado ele é enviado para todos os endereços listados no arquivo hosts (que deve estar no mesmo diretório do executável) de forma que a rede funcione como se todos os hosts estivessem conectados em um mesmo hub e ao receberem um pacote devem primeiramente verificar se o MAC de destino é o seu. 
## cabeçalhos

### Camada de Transporte

|  |     |
| ----- | --- |
|   Porta de Destino   | Porta de Origem     |
|  Número da Sequencia| Número do Ack|
| Checksum| Flags| 
|Dados||

### Camada de Rede

|  |     |
| ----- | --- |
|Tamanho do Cabeçalho|Tamanho Total|
|   Endereço de Destino   | Endereço de Origem     |

### Camada de Rede

|  |     |
| ----- | --- |
|   Endereço de Destino   | Endereço de Origem     |
|Checksum||
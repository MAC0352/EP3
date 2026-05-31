# EP3 MAC0352

## Compilação

Para compilar o projeto:

```bash
make

````
## Compilação

Para compilar o projeto:

```bash
make
````

O executável gerado será:

```bash
./client
```

## Configuração

Cada nó utiliza um arquivo de hosts contendo os endereços reais dos outros participantes.

Exemplo:

**hosts_alice**

```text
127.0.0.1:7002
```

**hosts_bob**

```text
127.0.0.1:7001
```

## Execução do chat

A sintaxe geral é:

```bash
./client <meu_ip> <ip_dst> <porta_src> <porta_dst> \
         <host_escuta> <porta_escuta> <username> \
         [--passive] [--hosts arquivo_hosts]
```

### Parâmetros

| Parâmetro    | Descrição                                       |
| ------------ | ----------------------------------------------- |
| meu_ip       | Endereço lógico deste nó                        |
| ip_dst       | Endereço lógico do destinatário                 |
| porta_src    | Porta lógica local                              |
| porta_dst    | Porta lógica remota                             |
| host_escuta  | Endereço onde o servidor local escuta           |
| porta_escuta | Porta TCP local                                 |
| username     | Nome do usuário no chat                         |
| --passive    | Aguarda conexão ao invés de iniciar o handshake |
| --hosts      | Arquivo hosts a ser utilizado                   |

## Exemplo de execução

Abra dois terminais.

### Terminal 1 (Alice)

```bash
./client 1 2 1001 2001 0.0.0.0 7001 Alice \
         --hosts hosts_alice
```

### Terminal 2 (Bob)

```bash
./client 2 1 2001 1001 0.0.0.0 7002 Bob \
         --passive \
         --hosts hosts_bob
```

Após o estabelecimento da sessão, as mensagens digitadas em um terminal serão entregues ao outro participante.

## Comandos disponíveis

Durante a execução do chat:

```text
/help   Exibe ajuda
/quit   Encerra a sessão
```

## Observações

* Apenas um dos lados deve iniciar em modo ativo.
* O outro participante deve utilizar a opção `--passive`.
* Os IPs utilizados são endereços lógicos simulados pela camada de rede.
* A comunicação física é simulada pela camada de enlace utilizando conexões TCP locais.


## Simulação da parte física

Sempre que um pacote é enviado, ele é entregue para todos os endereços listados no arquivo `hosts`, que deve estar no mesmo diretório do executável. Dessa forma, a rede se comporta como se todos os hosts estivessem conectados ao mesmo hub físico. Ao receber um pacote, cada nó verifica primeiro se o MAC de destino corresponde ao seu próprio endereço; caso contrário, o pacote é descartado pela camada de enlace.

## Cabeçalhos

### Camada de Transporte

| Campo | Descrição |
| ----- | --------- |
| Porta de Destino | Identifica a porta lógica de recepção |
| Porta de Origem | Identifica a porta lógica de envio |
| Número de Sequência | Controla a alternância entre segmentos |
| Número de Ack | Confirma o segmento recebido |
| Checksum | Permite validação de integridade |
| Flags | Indica o tipo do segmento |
| Dados | Payload da aplicação |

### Camada de Rede

| Campo | Descrição |
| ----- | --------- |
| Tamanho do Cabeçalho | Quantidade de bytes do cabeçalho da camada de rede |
| Tamanho Total | Tamanho total do pacote encapsulado |
| Endereço de Destino | Endereço lógico de destino |
| Endereço de Origem | Endereço lógico de origem |

### Camada de Enlace

| Campo | Descrição |
| ----- | --------- |
| Endereço de Destino | MAC de destino do frame |
| Endereço de Origem | MAC de origem do frame |
| Checksum | Validação simples do frame |
# ES32Lab Arcade 1.4.0

## Novidades

- CONFIGURACOES passa a ser sempre a ultima opcao do hall, independentemente da quantidade de jogos.
- Credenciais Wi-Fi armazenadas no namespace NVS oficial `ES_Wifi`, versao 1.
- Suporte a ate 6 redes, com prioridade da rede usada com sucesso.
- O SSID/senha compilados funcionam somente como fallback quando a NVS ainda nao possui redes.
- Scanner de redes usando `ES_WiFi::scanWifi(true)`.
- Cadastro de rede protegida pelo teclado analogico da ES32Lab.
- Exclusao de redes cadastradas.
- Se nenhuma rede conectar na inicializacao, o scanner e oferecido e o usuario pode escolher jogar offline.
- OTA local continua disponivel enquanto o Arcade esta no hall/configuracoes.
- Verificacao automatica de nova versao pelo manifesto.
- CONFIGURACOES > ATUALIZACAO permite verificacao manual.
- Atualizacao por `firmware_url`, com validacao de `firmware_sha256` quando o manifesto fornecer o hash.
- Wi-Fi e OTA sao desligados ao iniciar um jogo, como nas versoes anteriores.

## platformio.ini

O `platformio.ini` NAO faz parte deste pacote por solicitacao do projeto.

O codigo espera os valores ja usados pelo Arcade:
- `ES32LAB_ARCADE_VERSION`
- `ES32LAB_ARCADE_WIFI_SSID`
- `ES32LAB_ARCADE_WIFI_PASSWORD`
- `ES32LAB_ARCADE_OTA_HOSTNAME`
- `ES32LAB_ARCADE_OTA_PASSWORD`

Para a URL do manifesto, o codigo aceita:
- `OTA_MANIFEST_URL`, ou
- `ES32LAB_ARCADE_MANIFEST_URL`

A versao oficial da release deve ser ajustada para `1.4.0` no `platformio.ini`.

Os novos modulos utilizam os mesmos componentes usados pelo firmware de treinamento:
- `ES_Wifi.h`
- `ArduinoJson.h`
- `Preferences.h`
- bibliotecas de rede/Update fornecidas pelo Arduino-ESP32.

## Arquitetura

- `ArcadeWiFi`: armazenamento ES_Wifi, conexao, prioridade e scanner.
- `ArcadeFirmwareUpdate`: manifesto, comparacao de versao e instalacao.
- `ArcadeSettings`: telas de Wi-Fi e Atualizacao.
- `ArcadeOTA`: OTA local, agora reutilizando a conexao administrada pelo ArcadeWiFi.
- `ArcadeMenu`: adiciona CONFIGURACOES dinamicamente depois do ultimo jogo.
- `ArcadeController`: coordena hall, configuracoes, provisionamento inicial e jogos.

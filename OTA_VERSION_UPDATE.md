# Atualização OTA e controle de versão

## Arquivos novos

- `src/ArcadeConfig.h`: Wi-Fi, senha, hostname e timeout.
- `src/ArcadeVersion.h`: versão semântica e identificação de build.
- `src/ArcadeOTA.h`
- `src/ArcadeOTA.cpp`

## Arquivos alterados

- `src/ArcadeController.h`
- `src/ArcadeController.cpp`
- `src/ArcadeMenu.h`
- `src/ArcadeMenu.cpp`
- `platformio.ini`
- `README.md`

## Comportamento

1. O menu aparece imediatamente.
2. O ESP32 tenta conectar em `homeF` sem bloquear o menu.
3. Quando pronto, aparece `OTA:OK` discretamente no cabeçalho.
4. Se a rede não existir, o indicador desaparece e o arcade continua.
5. Ao abrir um jogo, o Wi-Fi é desligado.
6. O reset físico retorna ao menu e reativa a tentativa OTA.

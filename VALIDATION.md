# Validação realizada

- Confirmado que existe apenas um `setup()` e um `loop()` no diretório `src`, ambos em `src/main.cpp`.
- Confirmado que os seis jogos permanecem em namespaces próprios.
- Confirmado que o registro central referencia Tetris, Space Invader, Minhoquinha, Enduro Lab, Frog Adventure e ES32 Lab Box.
- Confirmado que o menu mantém rolagem e aceita o sexto jogo sem alteração de layout.
- Confirmado que o nome `ES32 LAB BOX` cabe no espaço reservado do menu.
- Confirmado que o jogo de boxe usa `ES_TFT`, `ES_AnalogKeyboard`, `ES_Buzzer`, `ES_TimeInterval` e `TFT_eSprite`.
- Confirmado que o framebuffer do jogo de boxe é criado apenas quando o jogo é iniciado.
- Confirmado que versão, build, Wi-Fi e hostname OTA continuam centralizados.
- Confirmado que o OTA é processado somente no menu e desligado antes de iniciar um jogo.
- O novo módulo foi verificado sintaticamente em C++11 com interfaces simuladas das bibliotecas.

A compilação final com as bibliotecas reais e o teste no display devem ser realizados no PlatformIO e na ES32Lab.

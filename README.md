# ES32Lab Arcade

Projeto PlatformIO que reúne seis jogos funcionais em um único firmware para a ES32Lab:

- Tetris
- Space Invader
- Minhoquinha
- Enduro Lab
- Frog Adventure
- ES32 Lab Box

## Uso

1. Abra esta pasta no Visual Studio Code com PlatformIO.
2. Aguarde a instalação das dependências.
3. Compile e grave na ES32Lab.
4. No menu, use `CIMA` e `BAIXO`.
5. Pressione e solte `CENTER` para iniciar o jogo selecionado.
6. Para sair do jogo e voltar ao menu, reinicie fisicamente o ESP32.

## Estrutura

```text
ES32Lab_Arcade/
├── platformio.ini
├── README.md
├── original_games/
│   ├── Tetris.ino
│   ├── SpaceInvader.ino
│   ├── SnakeGame.ino
│   ├── EnduroLab.cpp
│   ├── FrogAdventure.cpp
│   └── ES32LabBox.cpp
└── src/
    ├── main.cpp
    ├── ArcadeConfig.h
    ├── ArcadeVersion.h
    ├── ArcadeController.*
    ├── ArcadeMenu.*
    ├── ArcadeOTA.*
    └── games/
        ├── GameDefinition.h
        ├── GameRegistry.*
        ├── TetrisGame.*
        ├── SpaceInvaderGame.*
        ├── SnakeGame.*
        ├── EnduroGame.*
        ├── FrogAdventureGame.*
        └── ES32LabBoxGame.*
```

Os arquivos de `original_games/` são cópias de referência. Eles não são compilados.

## Arquitetura

O arquivo `main.cpp` possui o único `setup()` e o único `loop()` do firmware.

Cada jogo expõe uma definição comum:

```cpp
struct GameDefinition {
  const char* name;
  const char* description;
  void (*begin)();
  void (*update)();
};
```

O menu escolhe um item do `GameRegistry`. Depois da escolha:

1. o controlador chama `begin()` uma única vez;
2. o controlador chama `update()` continuamente;
3. o jogo permanece ativo até o reset físico do ESP32.

Essa estratégia evita alterar a jogabilidade para criar um comando de saída.

## Como adicionar um novo jogo

1. Crie `src/games/NovoJogoGame.h`.
2. Crie `src/games/NovoJogoGame.cpp`.
3. Coloque toda a implementação em um namespace próprio.
4. Converta o `setup()` original em `begin()`.
5. Converta o `loop()` original em `update()`.
6. Exponha um objeto `Arcade::GameDefinition`.
7. Inclua o cabeçalho e adicione o objeto ao vetor `GAMES` em `GameRegistry.cpp`.

Modelo de cabeçalho:

```cpp
#pragma once

#include "GameDefinition.h"

namespace ArcadeGames {
extern const Arcade::GameDefinition NovoJogoGame;
}
```

Modelo de implementação:

```cpp
#include <Arduino.h>
#include <ES32Lab.h>

#include "NovoJogoGame.h"

namespace ArcadeGames {
namespace NovoJogoModule {

ES_TFT display;
ES_AnalogKeyboard keyboard(P_KEYBOARD);

void begin() {
  display.init();
  display.setRotation(3);
  display.fillScreen(TFT_BLACK);
}

void update() {
  // Logica original do loop.
}

}  // namespace NovoJogoModule

const Arcade::GameDefinition NovoJogoGame = {
  "NOVO JOGO",
  "Descricao curta",
  NovoJogoModule::begin,
  NovoJogoModule::update
};

}  // namespace ArcadeGames
```

## Manutenção dos jogos existentes

Cada jogo está isolado em seu próprio namespace e arquivo. Isso evita conflitos entre nomes comuns como `display`, `keyboard`, `pontos`, `gameOver`, `setup` e `loop`.

Para atualizar um jogo:

1. edite apenas o respectivo arquivo `*Game.cpp`;
2. preserve as funções públicas `begin()` e `update()`;
3. não crie outro `setup()` ou `loop()`;
4. mantenha os includes obrigatórios da ES32Lab;
5. compile o projeto completo após a alteração.

## Dependências

```ini
lib_deps =
    esdeveloper/ES32Lab
    esdeveloper/TFT_eSPI_ES32Lab
```

O Enduro utiliza `TFT_eSprite`, disponibilizado pela versão da TFT_eSPI preparada para a ES32Lab.


## Versão do projeto

A versão atual está centralizada em:

```text
src/ArcadeVersion.h
```

Versão desta entrega: `1.2.0`.

Ao preparar uma nova entrega, altere somente os três números definidos no
início desse arquivo. O texto da versão é montado automaticamente.

## Atualização OTA

O Wi-Fi e o OTA funcionam somente enquanto o menu principal estiver aberto.

- Rede: `homeF`
- Hostname OTA: `es32lab-arcade`
- Endereço mDNS: `es32lab-arcade.local`
- Tempo máximo de tentativa: 6 segundos
- Se a rede não for encontrada, o arcade segue offline normalmente.
- Ao iniciar um jogo, o Wi-Fi é desligado.
- Reinicie fisicamente o ESP32 para voltar ao menu e reativar o OTA.

A primeira gravação desta versão precisa ser feita por USB usando o ambiente:

```text
nodemcu-32s
```

Depois, para atualizar pela rede, use:

```text
nodemcu-32s-ota
```

As credenciais e o nome do dispositivo ficam centralizados em:

```text
src/ArcadeConfig.h
```

O ambiente OTA do `platformio.ini` usa autenticação. Se a senha for alterada em
`ArcadeConfig.h`, atualize também a opção `--auth` no `platformio.ini`.


## ES32 Lab Box

O jogo de boxe foi integrado como módulo independente, com `setup()` convertido em `begin()` e `loop()` convertido em `update()`. O jogo utiliza framebuffer `TFT_eSprite`, teclado analógico, buzzer e temporizadores da LIB ES32Lab.

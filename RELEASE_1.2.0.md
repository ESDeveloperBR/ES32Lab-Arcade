# ES32Lab Arcade 1.2.0

## Alterações

- Adicionado o jogo `ES32 LAB BOX` ao registro central.
- Criados `ES32LabBoxGame.cpp` e `ES32LabBoxGame.h`.
- Preservado o código original em `original_games/ES32LabBox.cpp`.
- Convertido o `setup()` original em `ES32LabBoxModule::begin()`.
- Convertido o `loop()` original em `ES32LabBoxModule::update()`.
- Mantido o OTA ativo somente no menu.
- Atualizada a versão do Arcade de `1.1.0` para `1.2.0`.

## Controles do jogo

### Menu do boxe

- CIMA/BAIXO: dificuldade.
- ESQUERDA/DIREITA: som.
- CENTER: iniciar.

### Luta

- ESQUERDA/DIREITA: movimentar e esquivar.
- BAIXO: defender.
- CENTER: soco normal.
- CIMA: gancho quando o medidor especial estiver cheio.

Para retornar ao menu principal do Arcade, reinicie fisicamente o ESP32.

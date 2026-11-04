# MaizoWii — configurações individuais por jogo (candidato)

O botão **START** na tela de detalhes abre as opções do jogo selecionado.
O frontend carrega os valores globais como base e somente os campos que o
módulo aceita são sobrepostos por um perfil. Ao apertar **B** ou selecionar
**VOLTAR**, o perfil individual é gravado e a tela de detalhes retorna.

Os arquivos individuais são armazenados no **mesmo dispositivo do jogo**:

- `sd:/maizo/game-profiles/<hash>.ini`
- `usb:/maizo/game-profiles/<hash>.ini`

O nome usa hash determinístico FNV-1a de 64 bits do caminho e sistema;
evita colisão de nomes e inserção de caracteres perigosos como `../`.
Não grava o caminho original no perfil. Não sobrescreve
`maizo/settings.ini` para salvar alterações individuais.

**Ajustes sobrepostos por módulo:**

- Wii / USB Loader GX: IOS e porta USB.
- GameCube / Nintendont: memcard, widescreen, cheats, skip IPL,
  modo de vídeo, MC multi, controle nativo e remoção de limite.
- PS1 / WiiSXRX: interpretador, frameskip e áudio.
- Mídia / WiiMC: volume, condicionado a suporte no DOL adaptado.

SNES, NES, GBA e GB permanecem com opções avançadas no respectivo módulo
até ser implementado um contrato de configuração comprovado. Tema e modo
de biblioteca continuam **globais**, mesmo ao editar um perfil de jogo.

Teste de integridade: `python3 -m unittest discover -s tools -p test_game_profiles.py`.
É verificação host do código real de geração de nomes, não prova uso no Wii.

### Recuperação em FAT
Quando a renomeação precisa substituir o perfil existente, o gravador guarda `.bak` até concluir o novo arquivo; em caso de reinicialização no meio da troca, o carregador procura o backup se o perfil principal desapareceu. A gravação não é uma transação de sistema de arquivos real e o resultado depende da mídia.

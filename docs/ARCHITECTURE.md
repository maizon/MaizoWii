# Arquitetura e telas MAIZO Wii

## Camadas
UI GX / navegação -> biblioteca e configurações -> contrato de launch -> executáveis externos.
O MAIZO não liga símbolos ou carregadores próprios dos emuladores ao frontend, e o carregamento DOL troca o processo ativo.

## Telas de produto (conceito NeoStation adaptado a CRT)
1. **Home:** três cards por sistema (GameCube, PlayStation 1, Filmes/Música), contagem de jogos e dicas de botões.
2. **Biblioteca:** aba de plataforma, títulos, quantidade, favoritos, varredura incremental e busca.
3. **Detalhes:** capa gráfica futura, nome, sistema, ID de disco, caminho, módulo e ação Iniciar.
4. **Busca:** teclado virtual acessível por D-pad, filtro instantâneo por título.
5. **Configurações:** tema, visualização, Nintendont [memory card, widescreen, cheats, skip IPL, vídeo auto/MPAL/NTSC/PAL50]; WiiSXRX [interpreter, frameskip, audio]; mídia [volume reservado no contrato].
6. **Mensagens:** falhas de módulo, caminho inválido ou parâmetros não suportados (sem forçar execução).
7. **Futuras:** favoritos dedicados, baixar/gerenciar capas, mapeador de controles, gerenciamento de caminhos, salvar perfis por jogo, diagnóstico de rede.

## Perfis por sistema
Nintendont: config binária `nincfg.bin` baseada no layout NIN_CFG v0x0B. Nintendont preserva o seu HID. Checar o comportamento de flags em console real, especialmente PAL-M.
WiiSXRX: argumentos `argv[1]` pasta e `argv[2]` nome de arquivo, seguidos das opções `Core`, `Audio`, `SkipFrames`; confira GamecubeMain.cpp.
WiiMC: source/wiimc.cpp aceita argv[1] com caminho iniciado por u/s/h, define loadedFile e AutobootExit. O MAIZO envia o caminho selecionado e registra maizo-launch.txt para diagnóstico.

## Segurança/estabilidade
- Limites fixos de caminhos e biblioteca.
- Varredura incremental sem loop de leitura de múltiplos diretórios na thread de GX.
- Leitura HID assíncrona; transferência de USB mediante mudança de DOL.
- Validação do tamanho/cabeçalho das seções DOL; ainda requer testes físicos.
- Preferências armazenadas em `maizo/settings.ini` e favoritos em `maizo/favorites.txt`.
- Nenhuma ROM, BIOS ou executável proprietário é incluído.
- Operações de rede para capas são externas ao Wii nesta alpha; o conversor gera cache nativo GX RGBA8, exibido em Detalhes e na Home.

## Responsabilidades não resolvidas
- Garantir ABI de arguments e mapeamento USB 0810:0001 por captura de relatório.
- Tratamento seguro de DOL que se sobrepõe ao booter na MEM1 (verificar por teste físico).
- Restabelecer interface após saída dos apps sem acesso Homebrew Channel.
- Padronizar ajustes em perfis por jogo e transições com capa.
- Validar licença de cada artefato antes de distribuição de motores.

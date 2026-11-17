# MAIZOWII — reparos de 17/11/2026 (candidato, nao validado em hardware)

## Relatos e intervencoes

- **GameCube com desenho de portatil:** removida dependencia ambigua `NGC` do
  pacote de icones. O gerador cria um cubo com quatro entradas de controle no
  proprio CI, tanto para a imagem principal quanto para a aba transparente.
  Os `.gxt` ja presentes no USB/SD prevalecem: apagar
  `/maizo/covers/IGC.gxt` e `TGC.gxt` antigos para usar os novos do pacote.
- **Nintendont nao inicia jogo, configuracoes nao persistem:** mantido o
  argumento binario NIN_CFG v0x0B e a verificacao de autoboot. Arquivos
  `/nincfg.bin` sao gravados atomicamente em SD e USB, verificando leitura
  de volta e limpando apenas backup Maizo obsoleto. Se a estrutura de
  autoboot nao foi construida corretamente, o launcher cancela antes de
  executar o DOL. O fluxo ainda precisa de logs e teste fisico.
- **WiiMC sem PAL-M:** selecionar PAL-M/NTSC/PAL50/PAL60 no modulo MEDIA
  passa a exigir a versao WiiMC compilada pelo Maizo; o binario oficial nao
  recebe essa adaptacao. Em transicoes de 240p, 480i e retorno do modo
  progressivo, o adapter reaplica o padrao em vez de voltar silenciosamente
  para NTSC. A reproduçao do codec/ISO original nao e intencionalmente alterada.
- **USB desconectar/reconectar:** a cada 120 frames, o frontend verifica presenca
  fisica do dispositivo. Remove mount FAT obsoleto, reinicializa USB IOS,
  reindexa biblioteca, invalida capas e enumera executaveis. A inicializacao
  independente de SD/USB tambem se aplica ao Priiloader. Testes de cold
  autoboot com Priiloader e FAT32 permanecem obrigatorios.
- **Botoes quadrados:** substituido o preenchimento de 3 retangulos por
  faixas com raio arredondado para foco, menus e carrossel.
- **Capas e downloader:** busca solicitada antes do fim da indexacao agora
  aguarda, tenta rede em intervalos limitados e apresenta erros de HTTP/TLS
  ou escrita FAT. PNGs continuam com verificacao TLS e limite de bytes;
  frontais PS1 usam 128x128, GB/GBC 128x160, demais 128x176.
  O cliente do Wii preserva proporcao real de imagens com letterbox;
  sem esticar a capa. O conversor offline segue as mesmas dimensoes.

## Requisitos de validacao real

1. Arrancar pelo Priiloader so com USB, desmontar e reconectar, validar
   `USB OK` e reindexacao de jogos sem reiniciar.
2. GameCube: iniciar `/games/Jogo/game.iso` diretamente, sem navegador
   Nintendont; testar opcoes por jogo e retorno a frontend. Se abrir menu
   Nintendont, registrar versao do boot.dol, caminho, GameID e modo IOS.
3. Reproduzir DVD-Video ISO com WiiMC mod e escolher PAL-M em TV composta;
   confirmar sinal colorido apos abrir video e voltar ao navegador.
4. Downloader: usar Wi-Fi online, disco com ID6, SNES/PS1, informar texto
   de erro exato e counters `BAIXADAS/ERROS`, confirmar arquivos em
   `/maizo/covers/<sistema>/`.
5. Verificar dimensoes de capa na tela sem distorcao PS1 quadrada;
   substituir cache de sistema GameCube antigo pelo arquivo atualizado.
6. Confirmar codigo de barras, cores e geometria em 480i e PAL-M
   usando CRT; HDMI/component podem nao suportar PAL-M.

Compilacao na CI e testes estaticos nao comprovam hardware, rede HTTPS
no Wii real, ou inicializacao de jogo. Nunca declarar esses resultados sem
log ou confirmacao de teste fisico.

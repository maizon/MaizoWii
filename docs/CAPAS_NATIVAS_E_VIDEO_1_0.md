# MaizoWii — capas no Wii e calibracao por modo (candidato interno)

## Alteracoes desta revisao

- **Capas retangulares 8:11**, bordas retas e proporcao consistente entre
  centro e laterais do carrossel. O tamanho maximo continua 160 x 220 na UI.
  As texturas permanecem 128 x 176, sem esticar capas quadradas.
- **Baixar capas faltantes**: em *Ajustes do Maizo*, escolha **BAIXAR CAPAS
  FALTANTES**. O Wii tenta conectar por HTTPS, consultar GameTDB para Wii e
  GameCube e Libretro para PS1, SNES, NES, GBA, GB e GBC, convertendo PNG em
  `MZG1` no proprio Wii (USB ou SD). Capas prontas sao ignoradas. Uma
  biblioteca grande ou rede lenta pode demorar: cada tentativa tem limite,
  mas o download ainda e sincrono no loop do frontend.
- **Sem download automatico de filmes/musicas**: nao ha um provedor universal
  de posters para arquivos locais e nao se devem usar URLs inventadas.
- **Modo de video do frontend**: na tela *Calibracao do frontend*,
  `AUTO`, `480I/576I`, `240P/288P`. Este seletor configura APENAS
  o MaizoWii, **nao modifica os modos nativos dos jogos nem altera o WiiMC**.
  Um modo novo exige confirmacao, e reverte apos cerca de dez segundos se
  nao for confirmado. O retorno automatico precisa ser testado no console.
- **Jogos: calibracao herdada por sistema e perfil**: centro, largura e
  altura do frontend sao o ponto de partida. PS1 e SNES oferecem
  ajustes horizontais/largura **separados para 240p/288p e 480i/576i**
  nas configuracoes globais e nas preferencias individuais por jogo.
  O jogo prevalece sobre o sistema e o sistema sobre os valores iniciais.
  As correcoes de quad/VI estao nos adaptadores de codigo-fonte de SNES9x GX,
  WiiSXRX e WiiSX 4.2.2, sem tocar o desenho dos menus.
- **Preservar WiiMC**: este commit NAO modifica a fonte do WiiMC nem
  suas configuracoes de video. O cache de compilacao do WiiMC permanece
  independente do novo adaptador de video.

## Arquitetura e riscos

- Arquivos de capas: `usb:/maizo/covers/{wii,gc,ps1,snes,nes,gba,gb}/`.
  O frontend continua reconhecendo capas antigas e o dispositivo alternativo.
- O downloader usa libcurl/mbedTLS/libpng embarcados. HTTPS requer internet
  no Wii, DNS, relogio e certificados CA corretamente configurados. O sucesso
  dos testes da CI nao comprova negociacao TLS na rede do Wii.
- O adaptador dos jogos le uma pequena gravacao temporaria
  `{usb,sd}:/maizo/maizo-launch.txt` (`MAIZO-LAUNCH/3`) ao iniciar;
  os jogos e ROMs nao sao modificados.
- Alguns jogos mudam a resolucao durante o gameplay; a geometria precisa
  ser novamente validada nessas transicoes. Os ajustes finos por modo de
  video devem ser feitos **na TV real** para nao cortar imagem.
- O frontend usa rasterizador software/YUYV por compatibilidade. A troca
  de resolucao do VI e condicional, com temporizador de reversao;
  a CI nao testa a saida analogica de uma TV.
- Os 63 requisitos nao estao declarados como completos nesta revisao.
  Modos e downloads funcionais exigem teste no hardware real, incluindo
  USB HID, WiiMC, SNES e PS1.

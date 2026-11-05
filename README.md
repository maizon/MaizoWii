# MAIZO Wii — frontend multi-sistema (alpha de integração)

Interface **original em libogc/GX para Nintendo Wii**, inspirada na organização
visual do [NeoStation](https://github.com/misobadev/neostation-frontend). Não
é um port de Flutter. Projeto GPL-3.0-or-later; não inclui jogos, BIOS nem
binários de emuladores.

## Sistemas e executáveis detectados

| Aba | Conteúdo reconhecido | Módulo externo |
|---|---|---|
| Wii | `wbfs/*.wbfs`, `wbfs/*.iso` | USB Loader GX Enhanced |
| GameCube | `games/*/game.iso`, ISO/GCM/CISO | Nintendont |
| PlayStation 1 | ISO/BIN/CUE/IMG/PBP (+ CHD/M3U sinalizados) | WiiSXRX ou WiiSX 4.2.2 |
| Super Nintendo | SFC/SMC/FIG/ZIP | Snes9x GX |
| NES | NES/FDS/UNF/ZIP | FCE Ultra GX |
| Game Boy Advance | GBA | VBA GX |
| Game Boy / Color | GB/GBC/SGB | VBA GX |
| Filmes e música | MP4/MKV/AVI/MP3/FLAC etc. | WiiMC-SSLC |

Busca incremental em USB/SD, subpastas, filtros, favoritos persistentes,
catálogo em grade/lista, detalhes, teclado virtual em grade 8 colunas,
pesquisa em títulos e capas GX (cache LRU).

**Importante:** o MAIZO só executa diretamente **jogo** nas integrações
que possuem contrato de autoboot conhecido: Wii (ID6 no USB Loader GX
Enhanced), GameCube (config Nintendont) e PS1 (argv do WiiSXRX, formatos
aceitos). SNES/NES/GB/GBA usam autoboot já suportado nos emuladores GX via argv[1] (pasta) e argv[2] (arquivo);
WiiMC-SSLC recebe caminho em argv[1] e tenta reproduzir diretamente; o arquivo `maizo-launch.txt` é uma proposta de
contrato para versões adaptadas, **não é consumido** por binários originais. O WiiMC usa `usb1:/`/`sd1:/` e rejeitamos nomes com `%` por segurança na versão original.
Não há compatibilidade HID universal entre os módulos.

## Correcoes de integracao: USB WAD, video e WiiSX 4.2.2

### Diagnose USB e WiiMC: hotfix v6

O forwarder v6 usa a sequencia de passagem do FIX94: transmite um bloco
argv com o caminho real, libera FAT **e** o dispositivo USB, e tenta o
handoff LWP original quando essa funcao existe na libogc instalada.
O executavel USB tem prioridade sobre uma copia potencialmente antiga
no SD. O frontend tenta montagens independentes e nao procura arquivos
quando nenhum dispositivo estiver montado. Na home, Z reindexa a biblioteca.

A compilacao WiiMC foi alterada para utilizar as bibliotecas libogc e
portlibs empacotadas pelo proprio projeto WiiMC-SSLC, pois os codecs
originais foram compilados contra essa ABI. Esta e uma tentativa de
corrigir incompatibilidades da recompilacao do GUI no ambiente 2026;
a execucao no Wii real precisa de verificacao. O WiiMC oficial intacto
continua sendo a referencia de reproduçao funcional, sem HID adaptado.
O funcionamento HID **nao esta confirmado em hardware**.


**Estado:** alteracoes de codigo, CI e contrato; teste com Wii real segue necessario.
Uma compilacao bem-sucedida nao comprova a reproducao de videos ou a
montagem de armazenamento no console.

- **WAD/USB:** forwarder v6 passa argv, prioriza USB, fecha a interface USB antes do handoff e tenta IOS58 antes de montar; o frontend remonta
  SD e USB independentemente, inclusive com repeticoes de montagem. Se o
  IOS58 nao estiver instalado ou o dispositivo nao responder, o problema
  continua sendo diagnosticavel e nao se afirma suporte universal.
- **Tela MAIZO:** em Configuracoes > Calibrar tela MAIZO, ajuste posicao X/Y,
  largura e altura (70-130%). Os valores sao persistidos no settings.ini.
  Sao transformacoes apenas da UI deste frontend, nao alteram jogos externos.
- **GameCube:** Nintendont usa as opcoes reais VideoScale 40-120 (ou AUTO)
  e VideoOffset -20 a +20, aplicadas ao nincfg.bin, globalmente ou por jogo.
  O VideoOffset altera posicao vertical; nao e um esticamento vertical.
- **PS1:** selecione WiiSXRX ou WiiSX Beta 4.2.2 globalmente ou por jogo.
  WiiSX 4.2.2 recebe no argv: Core, Audio, SkipFrames, NativeOutput
  (240p/264p), VideoMode, ScreenMode, Deflicker, Dithering, FPS e LimitFrames.
  O pacote inclui o modulo oficial separado, sem substituir WiiSXRX.
  Resolucao nao significa aumentar renderizacao interna do PS1.
- **WiiMC:** escolha versao adaptada ou copia oficial (quando disponivel)
  e modo direto ou navegador original. A adaptacao inclui limite de
  espera ao carregar um arquivo. Nenhuma dessas mudancas comprova que o
  codec funcione no Wii real; o modo navegador permite diagnostico.

Para testar, comece com WiiMC oficial em modo navegador e um AVI pequeno;
depois compare o adaptado e o modo direto. Preserve sua instalacao estavel
de PS1/SNES ate verificar a nova compilacao em hardware.

## Instalar

1. Compile com devkitPro/devkitPPC e libogc ou baixe o artefato gerado pelo CI
   se a compilação passar. Copie `apps/maizo/boot.dol`, `meta.xml` e os
   perfis distribuídos no pacote para USB/SD FAT32.
2. Em `apps/`, coloque os módulos que possuir: `usbloader_gx`,
   `Nintendont`, `WiiSXRX`, `WiiSX`, `snes9xgx`, `fceugx`, `vbagx`, `WiiMC`,
   cada qual com `boot.dol`. São reconhecidas instalações em SD e USB.
3. ROMs em `wbfs`, `games`, `ps1`, `snes9xgx/roms`, `fceugx/roms`,
   `vbagx/roms`, `videos`, `music` ou caminhos alternativos indicados
   em `docs/MODULE_COMPATIBILITY.md`.
4. Inicie `MAIZO Wii` no Homebrew Channel.

## Controles e navegação

D-pad ou analógico do GameCube, Wii Remote ou **HID reconhecido**;
A confirma, B volta, L/R troca a aba, X busca, Y favorita, START/HOME abre
configurações. Na Home, Y abre favoritos e Z reindexa; na biblioteca,
Z alterna entre todos e favoritos. Na busca, setas selecionam a letra no
teclado, A digita, B apaga e START mostra resultados.

Existe decodificação para o controle **Microntek 0079:0006** e suporte
de **perfis por VID/PID**. Para 0810:0001 é necessário medir o relatório
real do Wii antes de criar o respectivo `.ini`. Consulte
`docs/HID_PROFILES.md`. Ao iniciar outro DOL, o suporte HID passa a ser
de responsabilidade do emulador. O controle USB usado no frontend
**não é automaticamente convertido em Wiimote/GC pad nos jogos**.

Na página AJUSTES, o rodapé exibe o último VID/PID USB HID visto, status
(conectado, desconectado ou sem perfil), número de relatórios e erros. A
presença de um receptor Twin 0810:0001 **não** significa que exista um
mapeamento funcional. A versão atual continua lendo um controle HID por vez.

Os argumentos usados no autoboot são montados por `source/launch_args.c` e
verificados em teste C nativo no CI. Jogos Wii sem ID6 válido geram erro
explícito em vez de abrir silenciosamente o menu do loader.

## Configurações

- Gerais: tema violeta/turquesa/coral, lista/grade.
- Wii: IOS inicial e porta USB (parâmetros do USB Loader GX Enhanced).
- GameCube: Memory Card, widescreen, cheats, Skip IPL, vídeo Auto/PAL-M/
  NTSC/PAL50, MC Multi, controle GC nativo, Remove Limit.
- PS1: dynarec/interpretador, frameskip, áudio (argumentos WiiSXRX).
- Vídeo: volume para **módulo adaptado**, não para WiiMC original.
- SNES/NES/GBA/GB: configurações avançadas permanecem nos menus originais.

Configuração em `maizo/settings.ini`, favoritos em
`maizo/favorites.txt`, perfis em `maizo/controllers/VID_PID.ini`.
A gravação da seleção de GC produz `nincfg.bin` na unidade do jogo.

## Capas de todos os videogames

`tools/cover_sync.py` baixa imagens oficiais ou preservadas pela comunidade:
GameCube e Wii por ID6/GameTDB; PS1, SNES, NES, GBA, Game Boy e
Game Boy Color por nome/Libretro Thumbnails (quando houver correspondência).
São baixadas **capas, não jogos**, em PC/Android com Internet.
`tools/build_cover_cache.py` converte as imagens PNG para texturas GX_RGBA8.

```sh
python3 tools/cover_sync.py /CAMINHO/DO/USB --systems all --limit 150
python3 tools/build_cover_cache.py /CAMINHO/DO/USB/maizo/covers
```

`--systems` aceita all, wii, gc, ps1, snes, nes, gba, gb, gbc.
Os scripts estarão disponíveis no ZIP em `maizo/tools/`.
O Wii **ainda não baixa capas diretamente da Internet**; o cache
é carregado localmente. Alguns jogos podem não ter arte correspondente.

## Compilar e validar

```sh
python3 tools/test_contract.py
python3 -m unittest discover -s tools -p 'test_cover_sync.py'
make -j2    # necessita DEVKITPPC/devkitPro
```

Pipeline CI: `.github/workflows/build.yml`. É indispensável testar
DOL/IOS/USB em **hardware Wii real** antes de anunciar estabilidade.
A troca de executáveis é **experimental** e não há retorno automático
garantido ao MAIZO após sair dos módulos.

## Pacote ALL-IN-ONE

O workflow `.github/workflows/build.yml` monta um ZIP com os oito programas:
frontend MAIZO, Nintendont, USB Loader GX Enhanced, WiiSXRX, Snes9x GX,
FCE Ultra GX, VBA GX e WiiMC-SSLC. Para instalar basta extrair o artefato
`MAIZOWII-ALL-IN-ONE-USB-SD` na raiz USB/SD FAT32 e adicionar seus próprios jogos.

**Qual binário vai para o ZIP:** o pipeline compila o source dos seis
módulos com a adaptação HID (`integrations/maizo_hid.inc` +
`tools/patch_upstreams.py`). Se determinado módulo compilar e seu DOL passar
pela validação, ele substitui a versão original **somente daquele módulo**.
Se a compilação falhar, o empacotador conserva a versão oficial sem a adaptação
HID. Consulte `maizo/MODULE_MANIFEST.json` e `HID-COMPATIBILITY.txt` dentro
do ZIP para verificar o resultado módulo por módulo. Nintendont usa seu próprio
driver HID e inclui a base de perfis `controllers/*.ini`.

**Compatibilidade:** ter o mesmo controle HID no MAIZO, Nintendont e na
interface do USB Loader GX **não garante controle no jogo Wii**: jogos Wii
possuem seu próprio sistema de entradas, geralmente esperando Wii Remote/
Classic. Para converter USB HID em controle de jogos Wii é necessário
suporte persistente via IOS/cIOS ou adaptação dos jogos; este projeto não o
implementa. Receptores 0810:0001 e XInput não devem ser declarados
compatíveis até serem testados com o relatório de dispositivo real.
Toda mudança de DOL e IOS necessita de testes físicos de estabilidade.
O jogo só abre se o formato, o armazenamento e o módulo forem compatíveis.

## Direitos e referências

Nintendont (GPL-2.0), WiiMC-SSLC (GPL-3.0), WiiSXRX (licença de upstream),
NeoStation (GPL-3.0, inspiração, não copiado), Flat Gamepad Icons
(CC BY-SA 4.0), Tabler (MIT), Font8x8 (domínio público).
Veja `THIRD_PARTY.md`.


## Forwarder e instalador
O ZIP inclui um DOL de forwarder para HBC em apps/maizo-forwarder/boot.dol,
que tenta iniciar sd:/apps/maizo/boot.dol ou usb:/apps/maizo/boot.dol.
Nao e canal WAD. Inclui YAWM ModMii Edition em apps/maizo-installer/boot.dol,
instalador **interativo**, que solicita a escolha/confirmacao do WAD.
O WAD MaizoWii ainda NAO foi produzido nem validado para instalacao na NAND.
Consulte docs/FORWARDER_INSTALADOR.md.


## HID compartilhado e compilação do instalador (nova rodada)
O frontend e os módulos adaptados compartilham o MESMO código fonte
`integrations/maizo_hid.inc` + `integrations/maizo_report.h`.
O parser permite reconhecer **gamepads HID de relatório padrão** por descritor
(USB HID Report Descriptor, report IDs e campos de botões/eixos/hat), sem
cadastrar cada VID/PID. Dispositivos com protocolo proprietário ainda podem
requerer `controllers/VID_PID.ini`, exatamente como nos perfis do Nintendont.
O driver atual aceita um único dispositivo USB físico ativo por DOL, embora
interprete até quatro portas lógicas de receptores `MultiIn` suportados.

O workflow também tenta recompilar o YAWM ModMii Edition com o mesmo driver,
integrando gamepad na rotina de `WaitButtons` e reinicialização após IOS
reload. O binário adaptado **só substitui** o oficial se a compilação e a
validação DOL passarem. O campo `installer.hid_adapted` do manifesto confirma
qual executável efetivamente foi distribuído. A instalação de WAD continua
**interativa, nunca automática**. Sem WAD de canal próprio validado, não existe
instalação do canal MaizoWii pronta. Teste real no console continua obrigatório.

## Candidato: gamepads USB HID MultiIn e perfis por jogo
Foi implementado roteamento de pacotes MultiIn 0–4 com convenções reais
do Nintendont, inclusive modo 3 (múltiplos pads em um relatório) e
modo 4 (relatório individual por pad), e testes C de limites dos pacotes.
O frontend abre configurações individuais com START na tela do jogo e
salva somente campos com contrato do módulo em
`sd:/maizo/game-profiles/` ou `usb:/maizo/game-profiles/`.
Veja `docs/GAME_PROFILES.md`. Tudo isso exige CI recompilado e teste Wii
real antes de ser anunciado como compatibilidade de hardware.

**Diagnóstico pelo controle:** Configurações → Select abre uma tela de informações USB HID com VID/PID, origem do perfil, endpoint, quantidade de relatórios, falhas e botões/eixos de até quatro portas lógicas. Detalhes em `docs/HID_DIAGNOSTICS.md`. Suporte real aos dispositivos depende do IOS e do Wii físico.

**Tema grafite (NeoStation/CRT):** o seletor global agora oferece quatro paletas, incluindo GRAFITE com cartões cinza e contraste moderado. É apenas opção visual; não altera modo de vídeo PAL-M/NTSC nem garante overscan corrigido em TV de tubo. Configurações individuais usam fallback `.bak` ao ler perfis se uma gravação FAT foi interrompida.

## Compilação incremental por componente

O GitHub Actions recompila **somente o componente cujo código ou dependência mudou**.
O frontend e o forwarder usam caches DOL independentes: alterar apenas
`source/**` não recompila o forwarder, e alterar apenas `forwarder/main.c`
não recompila o frontend. Cada módulo adaptado possui uma assinatura própria,
calculada de seu código de patch, dos três cabeçalhos HID compartilhados,
da revisão fixa do upstream e da imagem da ferramenta. Assim, alterações no
patch de WiiMC não recompilam WiiSXRX, SNES ou outros módulos.

Nas execuções seguintes, **caches com build bem-sucedido publicam novamente
os DOLs** no ZIP final (não são confundidos com versões oficiais). Falhas de
compilação também são registradas no cache como *fallback*, impedindo que o
mesmo módulo com código inalterado tente compilar de novo indefinidamente.
Enquanto o patch permanecer falhando, o ZIP usa a versão oficial e deixa isso
explícito no manifesto. O cache não substitui os arquivos do source.

**Exceções importantes:** uma mudança em cabeçalhos HID comuns afeta vários
módulos e os recompila; um cache ausente ou expirado exige nova compilação;
`workflow_dispatch → full_rebuild: true` força a recompilação, incluindo
adaptações com falha anteriormente armazenada. O empacotamento e testes podem
rodar a cada commit sem recompilar módulos preservados. A cache de GitHub
Actions não é armazenamento permanente e não garante reuso indefinido.


### Correcao de carregamento no Wii real — 04/11/2026
- A build #166 produz DOLs cujo ponto de entrada e **0x80003F00**, perfeitamente
  valido para devkitPro; o verificador anterior exigia 0x80004000 e rejeitava
  frontend e SNES9x GX. Frontend e forwarder agora aceitam 0x80003F00.
- O forwarder exibe detalhes de abertura, tamanho, leitura e validacao do DOL,
  e tenta montagem SD/USB explicita apos a tentativa FAT padrao.
- O bootstub limpa a area BSS declarada pelo DOL antes da transferencia.
- WiiMC: patch para nao sair/reiniciar automaticamente ao terminar um video
  enviado por argv; permanece no ambiente do WiiMC. Adaptacao HID depende de
  compilacao bem-sucedida e **teste real do controle**, nao apenas de cache.
- WiiSXRX: selecao por nome exato e menu de erro quando nao encontra o jogo,
  em vez de iniciar o ultimo arquivo do diretorio.
- O WAD de exemplo Nintendont (Title ID LXVS) foi analisado apenas como
  referencia de estrutura. **Nenhum WAD foi alterado ou instalado na NAND**;
  compilacao de um forwarder DOL nao prova seguranca de um canal WAD.
- A execucao de jogos de PS1 e compatibilidade USB HID precisam de teste
  em Wii real; compilacao verde nao garante compatibilidade.

- Seguranca de build WiiMC: o snapshot antigo traz um DOL precompilado.
  As builds #166 e #167 produziram WiiMC com o **mesmo SHA-256** apesar
  de patches diferentes: isso nao comprova integracao HID. A recompilacao
  agora exige novo DOL, propagacao dos erros do MPlayer e mudanca de hash.
  Se falhar, o ALL-IN-ONE mantem o WiiMC oficial que ja reproduz videos.

## Estabilidade: pacote de regressao e protecao
- A leitura de jogos e argv rejeitam caminhos SD/USB com traversal e bytes de controle.
- Configuracoes globais e favoritos passam a usar backup ao salvar em FAT32.
- Um erro no forwarder pode deixar um log em `sd:/maizo/forwarder.log`
  ou `usb:/maizo/forwarder.log` quando o dispositivo estiver montado.
- Cada DOL adaptado e distinguido do fallback oficial no resumo do CI.
- WiiMC, PS1, HID e WAD **exigem testes reais** antes de qualquer alegacao de
  compatibilidade completa. Nao instalar WAD experimental sem backup da NAND.

### Pacote de estabilidade: WiiMC original preservado e modulos testaveis
Se `maizo/MODULE_MANIFEST.json` indicar `WiiMC` em
`adapted_modules`, o empacotamento preserva automaticamente o player
anterior em `apps/WiiMC-official/boot.dol` com SHA-256 no manifesto.
**No Wii real**, caso a nova versao HID tenha problema, copie esse executavel
para `apps/WiiMC/boot.dol` no USB/SD (mantendo um backup do adaptado).
O novo executavel passou por compilacao no CI, mas **compatibilidade
HID, reproducao em CRT PAL-M e retorno ao Maizo exigem teste fisico**.
O WiiMC original que ja reproduzia videos continua recuperavel.
O frontend tambem procura PS1 em `roms/ps1` e `PSXISOS` e midia em `media/`.

### Canal Wii — forwarder OSC/FIX94 v4 (substitui o teste anterior)
A imagem antiga que exibe **MAIZO WII FORWARDER (sem instalacao NAND)** sem a
linha de versao OSC/FIX94 pertence a um forwarder anterior, mesmo quando o
executavel do frontend no USB ja foi atualizado. O codigo novo e baseado no
forwarder de Open Shop Channel/FIX94, com bootloader original de 872 bytes,
montagem independente SD/USB e tentativas USB.

O novo WAD experimental usa o **mesmo Title ID MZIO** (nao instala outro
titulo), mas **o WAD novo precisa substituir o instalado** para que seu codigo
tenha efeito. Atualizar `apps/maizo/boot.dol` sozinho nao atualiza o WAD.
Teste primeiro `apps/maizo-forwarder/boot.dol` pelo Homebrew Channel sem
instalar nada na NAND; se ele abrir o Maizo, a falha anterior estava no
forwarder/canal e nao no caminho do arquivo. No canal novo, verifique a linha
**MAIZO OSC/FIX94 FORWARDER v4** e os resultados `startup`/`FAT` na tela.
Caminho exigido em SD/USB: `apps/maizo/boot.dol` na **raiz FAT32**, nao em
`apps/maizo-forwarder/`. Logs em `maizo/forwarder.log`.

O WAD tem risco de brick em caso de defeito de banner/titulo. Valide no
Dolphin com NAND emulada antes de instalar e disponha de backup de NAND.
O resultado do CI **nao demonstra funcionamento no Wii real**.

### WiiMC 3.0 — correção de tela preta (versão para validação no Wii real)
A compilação modernizada do núcleo MPlayer/FFmpeg funcionou no CI, mas travou
na tela preta no Wii real. Em vez de compilar novamente o codec histórico,
o adaptador agora **preserva exatamente os cinco arquivos de bibliotecas
`.a` originais** do WiiMC 3.0, conferidos por SHA-1 do Git antes e depois
da compilação, e recompila apenas a integração GUI/HID, mantendo o decoder
estabelecido pela versão original. Os patches de HID continuam no código de
entrada e a interface de reprodução original é preservada.

O adaptador também verifica o caminho do vídeo após montar o armazenamento
(pesquisando até `usb5:` quando necessário), evita tela intencionalmente
preta durante a transição, dá retorno à GUI quando a reprodução não começa,
evita enumerar dispositivos HID repetidamente durante reprodução e adiciona
limites às esperas do MPlayer que anteriormente poderiam travar para sempre.

**Ainda não é garantia de execução no console:** A compatibilidade real
de vídeo, reprodução com áudio, controle USB HID e comportamento ao sair
dependem de confirmação no Wii. A versão oficial permanece guardada em
`apps/WiiMC-official/boot.dol`. Não altere PS1, SNES nem os outros núcleos
para investigar uma falha exclusiva do WiiMC.

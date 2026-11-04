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
| PlayStation 1 | ISO/BIN/CUE/IMG/PBP (+ CHD/M3U sinalizados) | WiiSXRX |
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

## Instalar

1. Compile com devkitPro/devkitPPC e libogc ou baixe o artefato gerado pelo CI
   se a compilação passar. Copie `apps/maizo/boot.dol`, `meta.xml` e os
   perfis distribuídos no pacote para USB/SD FAT32.
2. Em `apps/`, coloque os módulos que possuir: `usbloader_gx`,
   `Nintendont`, `WiiSXRX`, `snes9xgx`, `fceugx`, `vbagx`, `WiiMC`,
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

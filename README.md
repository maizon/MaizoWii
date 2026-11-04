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
aceitos). SNES/NES/GB/GBA abrem seus emuladores com o menu original;
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

## Capas

`tools/cover_sync.py` baixa **somente arte** de jogos Wii e GameCube
identificados por ID6 via GameTDB **em PC/Android com Internet**; não
baixa ROMs. `tools/build_cover_cache.py` transforma PNG em texturas
GX_RGBA8 `.gxt` no USB/SD. Para capas de outros sistemas, coloque
`maizo/covers/NOME-NORMALIZADO.png` e rode o conversor.

```sh
python3 tools/cover_sync.py /CAMINHO/USB --region US
python3 tools/build_cover_cache.py /CAMINHO/USB/maizo/covers
```

O Wii **ainda não baixa capas da Internet**; imagens podem ser carregadas
de armazenamento. See: `docs/ARCHITECTURE.md`.

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

## Direitos e referências

Nintendont (GPL-2.0), WiiMC-SSLC (GPL-3.0), WiiSXRX (licença de upstream),
NeoStation (GPL-3.0, inspiração, não copiado), Flat Gamepad Icons
(CC BY-SA 4.0), Tabler (MIT), Font8x8 (domínio público).
Veja `THIRD_PARTY.md`.

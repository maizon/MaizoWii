# MAIZO Wii — frontend independente (alpha experimental)

Frontend modular para Nintendo Wii, inspirado na **linguagem visual** do [NeoStation](https://github.com/misobadev/neostation-frontend). O MAIZO roda em libogc/GX; não é um port do Flutter e não inclui ROMs, BIOS ou motores de terceiros.

## Arquitetura

- MAIZO: menus, indexação progressiva USB/SD, filtro por console, busca, favoritos, preferências e seletor de arquivos.
- [Nintendont](https://github.com/fix94/nintendont): GameCube, execução em seu próprio DOL e suporte HID original do emulador.
- [WiiSXRX](https://github.com/niuus/WiiSXRX): PlayStation 1, autoboot de mídia por argumentos já existente no código-fonte.
- [WiiMC-SSLC](https://github.com/SuperrSonic/WiiMC-SSLC): vídeo/música, motor independente.

**Estado real:** Este é um primeiro protótipo para compilar e testar. Não afirma 100% de compatibilidade. O WiiSXRX declara suporte CHD/ECM/PBP; o WiiMC original reconhece caminho de vídeo em argv[1]. A execução automática ainda depende de confirmar o booter MAIZO no Wii. Não existe download de capas diretamente no Wii nesta versão. Não há garantia de retorno automático do DOL original para o frontend. O loader DOL e os controles HID precisam de testes no hardware.

## Instalação

1. Baixe o artefato **MAIZOWII-ALPHA-TEST** de GitHub Actions, após um build bem-sucedido.
2. Extraia em um pendrive ou SD FAT32, mantendo `apps/maizo/boot.dol` e `apps/maizo/meta.xml`.
3. Instale seus próprios executáveis em `apps/Nintendont/boot.dol`, `apps/WiiSXRX/boot.dol`, `apps/WiiMC/boot.dol` (também aceitos em `maizo/modules/{nintendont,wiisxrx,wiimc}/boot.dol`).
4. Jogos GameCube em `games/Nome/game.iso` ou arquivos ISO/GCM/CISO; PS1 em `ps1` ou `wiisxrx/isos`; mídias em `videos`, `movies` ou `music`. Aceita SD e USB.
5. Abra **MAIZO Wii** pelo Homebrew Channel.

## Controle

- D-pad / analógico: navegar.
- **A**: selecionar, abrir detalhes ou executar.
- **B**: voltar.
- **L/R**: alternar GameCube / PlayStation 1 / Mídia.
- **X**: pesquisa (na biblioteca).
- **Y**: favorito.
- **Start/Home**: ajustes; na busca, mostra resultados.
- **Z**: reindexar (na biblioteca).

Entrada do frontend: GameCube, Wii Remote e um adaptador HID experimental com relatório do **Microntek 0079:0006**. O dispositivo 0810:0001 é detectado, mas ainda não há perfil específico de seu protocolo USB. Um gamepad funcionar no frontend não transfere controle ao WiiSXRX/WiiMC; cada motor mantém seus próprios drivers. O Nintendont preserva seu próprio suporte.

## Construção

Requer devkitPro, devkitPPC e Wii libogc + libfat.

```bash
make
```

GitHub Actions compila em `devkitpro/devkitppc:20261102` e disponibiliza o DOL de teste.

## Interface

A primeira interface tem tema escuro, destaque violeta, navegação por sistemas, catálogo em lista, detalhes, pesquisa por teclado virtual, favoritos e ajustes separados de GameCube/PS1/mídia. O catálogo usa varredura **incremental** durante a renderização, até 1500 arquivos. Os ícones SVG em `assets/icons/` são referências não rasterizadas; a interface usa formas GX e mostra capas com cache de texturas `.gxt` preparadas pelo utilitário incluído.

## Capas

O auxiliar `tools/cover_sync.py` baixa capas GameCube por Game ID para `maizo/covers` quando executado em um PC com acesso à internet. Execute depois `python tools/build_cover_cache.py /CAMINHO/DO/PENDRIVE/maizo/covers` (requer Pillow) para converter PNG em textura GX. O MAIZO exibe os arquivos .gxt na Home e em Detalhes. Não há download HTTP no Wii. O script não distribui jogos.

## Problemas conhecidos / validação

1. **Primeiro teste:** navegação pelo Microntek, leitura USB e SD, biblioteca e estabilidade da UI.
2. **Segundo:** iniciar um GameCube pelo Nintendont sem alterar seu HID.
3. **Terceiro:** autoboot WiiSXRX com ISO/CUE/BIN, verificar quais formatos são suportados.
4. **Quarto:** WiiMC recebe argv[1] com o caminho do vídeo; validar reprodução e saída no Wii físico.
5. **Último:** retorno ao MAIZO e scanner de capas no Wii.

Veja `docs/ARCHITECTURE.md` e `docs/MODULE_COMPATIBILITY.md`.

## Créditos e licenças

O frontend é código original sob GPL-3.0-or-later. Font8x8 de Daniel Hepper (domínio público). Ícones Tabler: MIT; flat-gamepad-icons: CC BY-SA 4.0 (ver `THIRD_PARTY.md`). NeoStation é referência visual e não foi copiado para o programa. Nintendont, WiiMC e WiiSXRX são aplicativos separados, cada um mantendo sua licença e código.

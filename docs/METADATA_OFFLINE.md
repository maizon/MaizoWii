# MaizoWii: database offline de nomes e metadados
O frontend inclui o índice ID6 de títulos oficiais GameTDB Wii/GameCube, distribuído também pelo projeto Dolphin.
Fonte: https://github.com/dolphin-emu/dolphin/blob/master/Data/Sys/wiitdb-en.txt .
Arquivo instalado: `usb:/maizo/database/wiitdb-en.txt` ou `sd:/maizo/database/wiitdb-en.txt`.
O índice é lido uma vez durante a indexação e liberado da memória ao término.

## Títulos adicionais (PS1, SNES, NES, GBA, GB/GBC)
Opcionalmente, crie `/maizo/database/titles.tsv`, UTF-8, colunas separadas por TAB:
`SYSTEM<TAB>ROM_STEM_OR_ID6<TAB>OFFICIAL_TITLE<TAB>YEAR<TAB>REGION`.
Sistemas aceitos: WII, GC, PS1, SNES, NES, GBA, GB.

Exemplo:
`SNES<TAB>Super Mario World (USA)<TAB>Super Mario World<TAB>1990<TAB>USA`
(Substitua <TAB> por tabulação literal.)

Para importar um DAT XML do No-Intro/Redump disponibilizado legalmente, use em PC/Termux:
`python3 tools/import_nointro_dat.py SNES seu-dat.xml --out titles.tsv`
E copie o arquivo ao armazenamento do Wii.

O matching é **exato** por ID6 ou nome original da ROM (sem extensão). Não há identificação adivinhada de ROM por similaridade, nem hash integral de jogos gigantes no Wii. Se o jogo não for encontrado, o nome limpo de ROM ou da pasta aparece normalmente; ano/região somente quando identificados. Não é necessário acesso à internet durante a navegação.

## Capas
As fontes reais verificadas são GameTDB (Wii/GC por ID) e thumbnails.libretro.com (jogos por nome exato).
Tipos GameTDB: frontal, completa HQ, 3D; Libretro: Named_Boxarts, Named_Snaps, Named_Titles. Nem todas as imagens existem em todas as regiões e sistemas. Os downloads requerem rede HTTPS e têm tratamento de cancelamento em passos pelo curl multi. Para segurança, o arquivo GXT existente só é substituído depois de a imagem inteira ter sido baixada, decodificada e escrita em um arquivo temporário.

**Nenhum dos testes de host ou CI substitui testes no console Wii real.**

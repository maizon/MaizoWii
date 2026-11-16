# Compatibilidade verificada no código de origem (não equivale a teste de hardware)

| Plataforma | Pastas de biblioteca | DOL esperado | Integração ao iniciar |
| --- | --- | --- | --- |
| Wii | `wbfs`, `roms/wii` | `apps/usbloader_gx/boot.dol` | ID6 (se detectado), `-bootios`, `-usbport` no USB Loader GX Enhanced |
| GameCube | `games`, `roms/gamecube` | `apps/Nintendont/boot.dol` | `nincfg.bin` e AUTO_BOOT, GC Video |
| PS1 | `ps1`, `wiisxrx/isos` | `apps/WiiSXRX/boot.dol` | `argv[1]` diretório, `argv[2]` arquivo; Core, Audio, SkipFrames |
| SNES | `snes9xgx/roms`, `roms/snes` | `apps/snes9xgx/boot.dol` | autoboot por argv[1] (pasta), argv[2] (arquivo) |
| NES | `fceugx/roms`, `roms/nes` | `apps/fceugx/boot.dol` | autoboot por argv[1] (pasta), argv[2] (arquivo) |
| GBA | `vbagx/roms`, `roms/gba` | `apps/vbagx/boot.dol` | autoboot por argv[1] (pasta), argv[2] (arquivo) |
| GB/GBC | `vbagx/roms`, `roms/gb` | `apps/vbagx/boot.dol` | autoboot por argv[1] (pasta), argv[2] (arquivo) |
| Vídeos e músicas | `videos`, `movies`, `music` | `apps/WiiMC/boot.dol` | reprodução direta por `argv[1]` convertido em `usb1:/`/`sd1:/` (partição 1 presumida) |

Todos os caminhos dos DOLs também podem ser `sd:/apps/...` e
`sd:/maizo/modules/<id>/boot.dol` / `usb:/maizo/modules/<id>/boot.dol`.
As linhas Wii e GC dependem de dispositivos/IOS preparados corretamente.

### Fontes dos contratos

- USB Loader GX Enhanced: `source/StartUpProcess.cpp` reconhece game ID
  de 6 caracteres e parâmetros `-bootios=` e `-usbport=`.
- Nintendont: `common/include/CommonConfig.h`, `loader/source/main.c`,
  lê estrutura `NIN_CFG` versão `0x0B`.
- WiiSXRX: `Gamecube/GamecubeMain.cpp`, lê argumentos e pares de opções.
- WiiMC-SSLC: `source/wiimc.cpp` aceita caminho `argv[1]` para autoboot; nomes com `%` são bloqueados por bug de `sprintf` original.
- Snes9x GX, FCE Ultra GX, VBA GX: menus originais; integração avançada
  ainda requer verificar e eventualmente modificar seus pontos de entrada.

**HID**: o frontend não transfere seu driver a outro DOL. O ZIP ALL-IN-ONE inclui binários oficiais, enquanto patches HID para cada código-fonte são entregues separadamente e não significam compatibilidade comprovada no Wii. Nintendont tem
`controller.ini` específico; os outros motores não herdam esse suporte.
O `maizo-launch.txt` é apenas registro para possíveis patches. Não
afirmar suporte a HID 0810:0001 ou a XInput sem teste físico.

**Vídeo PAL-M**: forçar o modo não garante sinal na TV CRT, e o Wii pode
ficar com tela preta. Sempre guardar saída AUTO como padrão. CHD/M3U são
listados, mas são bloqueados no lançamento PS1 original por ausência de
suporte comprovado.

### Contratos executáveis testados no host

`source/launch_args.c` monta os argumentos que o `source/launch.c` envia
para `__argv`. O CI compila esse mesmo C no host e verifica cada um dos oito
sistemas, contagem de argumentos, rejeições de ID6, CHD/M3U e nomes WiiMC
com `%`. Isso **não** confirma consumo correto por cada DOL nem execução no Wii.
Sem ID6 válido, Wii agora mostra erro antes da troca de DOL em vez de abrir
o menu USB Loader GX silenciosamente.

## Saída CRT por núcleo (integração experimental)

MENU GERAL: AUTO/NTSC/PAL50/PAL60/PAL-M. Cada módulo (menos homebrews
arbitrários) oferece HERDAR/AUTO/NTSC/PAL50/PAL60/PAL-M; perfis
individuais de jogos podem sobrescrever isso. AUTO não impõe sinal.
As preferências são persistidas em settings.ini e o padrão efetivo
via tv_standard no registro de lançamento. Nintendont recebe NIN_CFG
PAL50/PAL60/NTSC/MPAL. Módulos reconstruídos que aplicam o registro:
WiiMC-SSLC, WiiSXRX/WiiSX e núcleos GX SNES/NES/GBA/GB, além do menu
do USB Loader GX. Forçar PAL/NTSC em qualquer jogo Wii que troca VI
internamente não é garantido; binários originais/fallback ignoram esse
registro. Modo progressivo de componente é deliberadamente preservado.
Apenas o menu do USB Loader é alterado de forma verificável; jogos Wii
exigem intervenção específica do motor e validação física.

WiiMC: mantém o suporte original a *DVD-Video ISO* por dvdnav. Arquivo
ISO genérico de jogo ou dados não é vídeo reproduzível. O adaptador tenta
montar a partição antes do autoplay e valida acesso; protege caminho IFO
e transições da lista de dispositivos. A enumeração HID é bloqueada
enquanto USB mass-storage estiver montado, para não competir pelo mesmo
barramento. Nessa condição o controle HID genérico pode não funcionar
dentro do WiiMC; Wiimote e controle nativo permanecem. Não atribuir
um crash DSI específico a esse defeito sem stack trace e addr2line.

**Atenção:** resultados dependem de compilação real de cada DOL adaptado
(veja MODULE_MANIFEST.json). Até os testes em Wii físico com vídeo,
USB e ISO, isto é implementação experimental, não correção confirmada.

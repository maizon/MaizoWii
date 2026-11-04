# Compatibilidade verificada no código de origem (não equivale a teste de hardware)

| Plataforma | Pastas de biblioteca | DOL esperado | Integração ao iniciar |
| --- | --- | --- | --- |
| Wii | `wbfs`, `roms/wii` | `apps/usbloader_gx/boot.dol` | ID6 (se detectado), `-bootios`, `-usbport` no USB Loader GX Enhanced |
| GameCube | `games`, `roms/gamecube` | `apps/Nintendont/boot.dol` | `nincfg.bin` e AUTO_BOOT, GC Video |
| PS1 | `ps1`, `wiisxrx/isos` | `apps/WiiSXRX/boot.dol` | `argv[1]` diretório, `argv[2]` arquivo; Core, Audio, SkipFrames |
| SNES | `snes9xgx/roms`, `roms/snes` | `apps/snes9xgx/boot.dol` | abre menu original; sem autoboot direto confirmado |
| NES | `fceugx/roms`, `roms/nes` | `apps/fceugx/boot.dol` | abre menu original |
| GBA | `vbagx/roms`, `roms/gba` | `apps/vbagx/boot.dol` | abre menu original |
| GB/GBC | `vbagx/roms`, `roms/gb` | `apps/vbagx/boot.dol` | abre menu original |
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

**HID**: o frontend não transfere seu driver a outro DOL. Nintendont tem
`controller.ini` específico; os outros motores não herdam esse suporte.
O `maizo-launch.txt` é apenas registro para possíveis patches. Não
afirmar suporte a HID 0810:0001 ou a XInput sem teste físico.

**Vídeo PAL-M**: forçar o modo não garante sinal na TV CRT, e o Wii pode
ficar com tela preta. Sempre guardar saída AUTO como padrão. CHD/M3U são
listados, mas são bloqueados no lançamento PS1 original por ausência de
suporte comprovado.

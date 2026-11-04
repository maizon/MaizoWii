# Compatibilidade dos módulos: fontes verificadas

| Sistema | Fonte | Entrada atual | Limites |
|---|---|---|---|
| GameCube | https://github.com/fix94/nintendont | `nincfg.bin` NIN_CFG v0x0B, AUTO_BOOT, USB, caminho relativo, vídeo e ajustes GC | Deve validar retorno, argv e formatos no Wii real |
| PlayStation | https://github.com/niuus/WiiSXRX | pasta e arquivo em argv[1]/argv[2], `Core`, `Audio`, `SkipFrames` | README WiiSXRX declara CHD/ECM/PBP, mas controle USB genérico não presumido |
| Mídia | https://github.com/SuperrSonic/WiiMC-SSLC | recebe caminho em `argv[1]` com `usb1:/` ou `sd1:/` e grava `maizo-launch.txt` | WiiMC original aceita argv de arquivo; transição DOL e partição precisam validação no Wii |

Nintendont reporta flags de vídeo `NIN_VID_FORCE_MPAL`, NTSC e PAL50; não presume que uma alteração de PAL-M produza imagem válida em toda TV.

Caminhos buscados em ordem: `usb:/apps/<nome>/boot.dol`, `sd:/apps/<nome>/boot.dol`, `usb:/maizo/modules/<nome>/boot.dol`, `sd:/maizo/modules/<nome>/boot.dol`.

### Próximas adaptações isoladas
- validar o argv[1] WiiMC no DOL original e eventual suporte ampliado a opções de reprodução;
- estratégia de retorno sem congelar: sair do núcleo + recarregar DOL MAIZO;
- diagnóstico após travamento de USB e registro de ABI;
- melhorar texturas GX e cache de capas;
- perfil USB HID fora de Nintendont, sem afirmar suporte universal.

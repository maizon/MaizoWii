# Canal Maizo Wii (WAD) — cuidado com a NAND

O frontend principal fica em **sd:/apps/maizo/boot.dol** ou
**usb:/apps/maizo/boot.dol**. O canal forwarder apenas procura esse executável.
Este WAD destina-se ao **Wii original** e não ao vWii de Wii U.

O workflow de WAD experimental compila o forwarder, reutiliza layout de
banner/ícone e animação do HBC-Reborn (não altera as dimensões do BRLYT),
altera somente a textura do título para MAIZO, e empacota com WadPakk:
- Title ID: `00010001-4D5A494F` (`MZIO`)
- IOS de inicialização: 58
- Tipo: canal de usuário, não altera System Menu ou IOS
- Implementação: comex loader, depois DOL forwarder do Maizo
- Teste automático: estrutura, Title ID, IOS58, quantidade de conteúdos
- **Ainda não validado em Wii real ou em Dolphin com NAND do System Menu.**

**Nunca é possível garantir "sem risco de brick" com um WAD gerado
recentemente.** Banner inválido pode travar o Menu Wii permanentemente
até recuperação. O verificador do WAD não detecta todos os erros visuais.

**Antes de considerar instalação**, teste no Dolphin com NAND Wii Menu,
verifique que Priiloader inicia segurando RESET ao ligar o Wii, mantenha
backup da NAND feito pelo BootMii e um meio de executar o instalador pela
recuperação. Se não possui controle compatível para navegar no YAWM ou
no Priiloader, não instale. A ausência dessas condições bloqueia o ensaio
no aparelho real. Não rode WADs de procedência duvidosa.

Créditos: fonte base dos layouts do banner de
[Monsterray/hbc-reborn](https://github.com/Monsterray/hbc-reborn), derivado
do Homebrew Channel. Empacotador externo
[davi-x86/WadPakk](https://github.com/davi-x86/WadPakk).

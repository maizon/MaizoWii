# MaizoWii — Forwarder e instalador (alpha)

O novo forwarder em apps/maizo-forwarder/boot.dol inicia apps/maizo/boot.dol (procura primeiro SD, depois USB). Ele é um DOL para testes via Homebrew Channel, não um canal instalável no Wii Menu.

O instalador incluído em apps/maizo-installer/boot.dol é o YAWM ModMii Edition 1.1 (GPL-2.0). Ele permite instalar um WAD após seleção e confirmação do usuário. Nenhuma instalação ocorre automaticamente.

Ainda não há WAD MaizoWii neste pacote: faltam banner original, construção e validação de canal Wii e ensaio real da instalação. O instalador não deve ser tratado como comprovado para o canal Maizo.

O adaptador USB HID dos módulos está expandindo MultiIn 1/2/4 e perfis Nintendont. O suporte real depende dos relatórios do dispositivo, do IOS, da compilação de cada módulo e de testes em Wii físico.

Instalar um WAD grava na NAND e oferece risco de brick. Antes de instalar, faça backup da NAND, tenha um método de recuperação e revise o Title ID e a origem do arquivo.


## Tentativa de compilação de YAWM adaptado
`tools/patch_yawm.py` modifica o source da revisão fixa
`62205f9ab507cdceae94c77197b1b459f0276d39` do YAWM ModMii Edition.
O patch conecta botões HID ao fluxo que o instalador já usa para seus prompts,
incluindo a confirmação obrigatória de instalação. Quando o DOL adaptado for
compilado e validado pelo CI, ele substitui o oficial no ZIP; caso contrário
o oficial permanece e o manifesto acusa `hid_adapted: false`.

**Não existe** `MaizoWii-Forwarder.wad` validado neste pacote e não há
instalação automática em NAND. Um forwarder `.dol` não substitui o canal WAD.

## Critério rígido do ZIP final (YAWM HID)
A partir deste ciclo, o workflow **não publica o ZIP ALL-IN-ONE como aprovado** quando `installer.hid_adapted` não é verdadeiro. O binário YAWM oficial permanece apenas como fallback de diagnóstico, sem promessa de navegação USB HID. Uma release exige DOL adaptado compilado, manifesto e SHA-256 verificados. Isso não substitui o teste do instalador em Wii real nem produz um canal `.wad` validado.

# MaizoWii — Forwarder e instalador (alpha)

O novo forwarder em apps/maizo-forwarder/boot.dol inicia apps/maizo/boot.dol (procura primeiro SD, depois USB). Ele é um DOL para testes via Homebrew Channel, não um canal instalável no Wii Menu.

O instalador incluído em apps/maizo-installer/boot.dol é o YAWM ModMii Edition 1.1 (GPL-2.0). Ele permite instalar um WAD após seleção e confirmação do usuário. Nenhuma instalação ocorre automaticamente.

Ainda não há WAD MaizoWii neste pacote: faltam banner original, construção e validação de canal Wii e ensaio real da instalação. O instalador não deve ser tratado como comprovado para o canal Maizo.

O adaptador USB HID dos módulos está expandindo MultiIn 1/2/4 e perfis Nintendont. O suporte real depende dos relatórios do dispositivo, do IOS, da compilação de cada módulo e de testes em Wii físico.

Instalar um WAD grava na NAND e oferece risco de brick. Antes de instalar, faça backup da NAND, tenha um método de recuperação e revise o Title ID e a origem do arquivo.

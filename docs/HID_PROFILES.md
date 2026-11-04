# Perfis HID USB do MAIZO (Wii)

O MAIZO recebe eventos do **frontend** por GameCube, Wii Remote e USB HID. O perfil
de fábrica do **Microntek 0079:0006** foi baseado no relatório previamente testado.
O receptor **Twin 0810:0001** não tem layout confiável no Wii: **não se inventa
um mapa** para ele. Alguns receptores XInput nem aparecem como USB HID padrão.

## Como criar um perfil por VID/PID

Crie `usb:/maizo/controllers/0810_0001.ini` ou
`sd:/maizo/controllers/0810_0001.ini` após capturar o relatório bruto real.
Os bytes são índices **zero-based** de um relatório USB IN de até 64 bytes.

```ini
x_offset=0
y_offset=1
hat_offset=5
hat_mask=0x0f
hat_shift=0
a_offset=5
a_mask=0x40
b_offset=5
b_mask=0x80
x_button_offset=5
x_button_mask=0x20
y_button_offset=5
y_button_mask=0x10
l_offset=6
l_mask=0x01
r_offset=6
r_mask=0x02
start_offset=6
start_mask=0x20
select_offset=6
select_mask=0x10
```

Esse trecho é **apenas o exemplo Microntek**, NÃO o mapeamento real do Twin.
Um arquivo com a extensão `.example` não é carregado. A indicação de controle
USB só aparece depois de ler um relatório. Perfis inválidos/fora dos limites
não devem causar leitura fora do buffer. Ao desconectar, o leitor volta a
tentar localizar o controle; reconexão precisa ser validada em Wii real.

**Limitação do sistema:** Nintendont, WiiSXRX, SNES9x GX, FCE Ultra GX,
VBA GX e WiiMC são processos/motores independentes. O driver HID do MAIZO
**não atravessa** a troca de DOL/IOS. O Nintendont precisa do seu próprio
suporte HID e `controller.ini`; os demais precisam de patches individuais de
entrada ou de um serviço IOS persistente devidamente testado. Não confundir
navegar no frontend com controlar o jogo.

## Diagnóstico incorporado ao frontend

Em AJUSTES aparece o último VID/PID HID enumerado, se há perfil, conexão,
contador de relatórios e erros de leitura. Dispositivos sem perfil **não**
são abertos para tentativa de leitura, evitando inventar offsets do Twin.
O status não substitui dump de descritores e teste com dois gamepads no Wii;
a implementação atual atende somente uma conexão USB HID ativa por vez.


## MultiIn (experimental)
Os modulos adaptados suportam leitura de MultiIn=1 (filtro primeiro byte),
MultiIn=2 (porta em primeiro byte) e MultiIn=4 (relatorios separados,
ate quatro portas logicas, no maximo 32 bytes cada), conforme convencao
Nintendont. MultiIn=3 e outros formatos continuam limitados. Isto nao
comprova compatibilidade USB HID 0810:0001 no Wii.

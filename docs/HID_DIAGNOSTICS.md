# MAIZO — diagnóstico USB HID pelo próprio gamepad

No frontend MaizoWii, entre em **Configurações** e pressione **Select**
(mapeado como `refresh`) para abrir a tela **Diagnóstico USB HID**.
Use **B**, **A** ou **Start** para voltar. Funciona sem teclado ou Wii Remote
desde que o gamepad já esteja sendo reconhecido pelo frontend.

Exibe:
- Último VID:PID de dispositivo identificado pelo driver.
- Origem do mapeamento: `controller.ini` ou descritor USB HID.
- Número de portas lógicas com relatório recebido.
- Endpoint e tamanho máximo de pacote, ID de relatório e modo `MultiIn`.
- Quantidade de relatórios, falhas de leitura e estado dos botões/eixos por porta.

**Limitações:** detecção da classe USB HID é diferente de compatibilidade
universal. XInput e protocolos proprietários que não expõem interface HID
não são automaticamente reconhecidos. O modo `MultiIn=3` exige stride
em bytes correto e no máximo quatro pads, e `MultiIn=4` exige pacotes
indexados de no máximo 32 bytes, conforme Nintendont. A tela diagnostica
o último dispositivo enumerado quando não há um controle ativo.

Esta tela mostra dados do driver compilado; o fluxo e os botões precisam
de confirmação no Wii físico com adaptadores diferentes.

# MAIZO Wii v6.2 - recuperação após relatos de Wii real

Status: **prévia de testes; ainda NÃO é a primeira release estável**.

## Evidência do console

O canal forwarder v6 consegue passar execução ao frontend, mas o processo
seguinte perdeu o armazenamento USB e, por causa de um gate indevido no
`source/input.c`, também impediu a inicialização do HID. O frontend, quando
aberto por outro homebrew, montou e acessou o armazenamento. Isso delimita o
problema ao handoff/remontagem e não prova defeito físico no pendrive.

Foi relatada a tela **Exception (DSI) occurred** no WiiMC adaptado (modo
direto e navegador), enquanto o WiiMC original reproduz. Esse é um crash real
não resolvido só por compilar. A imagem contém os PCs / stack trace
`80551550`, `805A2310`, `805B7594`, `805B8050`,
`805B8680`, `805B9734` (transcrição aproximada; conferir no ELF com
símbolos correspondente).

## Mudanças aplicadas no frontend

- A enumeração HID ocorre sem depender de FAT/SD/USB montado. Controlador é
  USB HID; não exige uma partição FAT para funcionar.
- Montagem inicial tenta libfat e dispositivos independentes, com mais
  tentativas USB; reavalia periodicamente unidades ausentes, refaz cache de
  módulos e reindexa quando USB volta. NUNCA reinicia o canal sozinho.
- Scanner Wii analisa o ID6 real do disco, no início da ISO e no cabeçalho
  embutido do WBFS (0x200), quando o nome de arquivo não contém o ID6.
  Isso NÃO recupera imagem corrompida e NÃO converte ISO em WBFS.
- Interface sem barra lateral. Menu de sistemas e biblioteca compartilham
  três layouts: GRADE com cards do mesmo tamanho, LISTA com títulos à
  esquerda e capa à direita, CARROSSEL com capas centrais deslizantes.
  START abre Configurações; B volta; L/R alterna sistemas.
- Calibração de largura e deslocamento modifica agora também viWidth e
  viXOrigin **da saída VI do frontend MAIZO**, preservando parâmetros de
  modo/entrelaçamento, sem inventar resolução 480p. Os outros homebrews
  configuram sua própria saída VI: não herdam essa calibração.
- WiiMC ORIGINAL passa a ser a escolha padrão para instalação nova;
  HID ADAPTADO permanece opção **experimental e não confiável** enquanto
  o DSI não for localizado/corrigido em ELF real.
- WiiSX Beta 4.2.2: patch de fonte semelhante ao WiiSXRX para GUI,
  navegador e botões analógicos via controlador GC virtual. O build
  adaptado é selecionado apenas se compilar; na falha mantém-se o DOL
  oficial e o manifesto deve reportar ausência do HID adaptado.

## Passos de validação no Wii físico (USB FAT32)

1. Conservar backup do `usb:/apps/maizo/boot.dol`, `settings.ini`,
   saves e `nincfg.bin`. **Não apagar o WAD v5/v6 instalado às cegas**.
2. Extrair novo `MAIZOWII-PREVIEW-USB-SD` na raiz do pendrive, com
   `usb:/apps/maizo/boot.dol` e os demais `apps/...`.
3. Abrir MAIZO primeiro por **outro homebrew que já funciona** para medir
   separadamente frontend e canal. Ver indicadores USB e HID no canto,
   navegar com o controle e conferir módulos. Repetir pelo WAD v6.
4. Alterar largura VI na tela de calibração aos poucos; não aumentar
   exageradamente para além da área segura da CRT. Isso não afeta Nintendont
   nem WiiMC, e não substitui calibração própria do televisor.
5. Escolher **WIIMC ORIGINAL** e MODO NAVEGADOR para reprodução segura.
   Não insistir com a versão HID adaptada que provocou DSI.
6. Testar Wii com WBFS nomeado sem `[ID6]`, WiiSXRX e WiiSX Beta 4.2.2
   com o controle HID e conferência do `MODULE_MANIFEST.json`. Build verde
   **não** equivale a teste de hardware.

## Bloqueadores para uma release pública

- **WiiMC HID adaptado DSI**: exige mapear o stack trace no ELF exato da
  compilação e testar libogc/MPlayer/USB em aparelho real. O original
  é o fallback funcional.
- **WAD forwarder**: deve ser repetido no Wii real e nunca instalado sem
  plano de recuperação NAND/Priiloader. Não foi reconstruído nesta mudança;
  o v6 carrega diretamente apps/maizo/boot.dol, não outro forwarder.
- **Compatibilidade de controles**: drivers HID dependem do protocolo real.
  `0079:0006` possui perfil; receptor Twin `0810:0001` e XInput não
  são automaticamente suportados.
- **Geometria dos módulos**: cada DOL reconfigura VI. A calibração do
  frontend não viaja para o emulador. Ajuste por emulador é separado.
- **maizo-installer**: um build YAWM oficial sem HID não prova que instalação
  de canal via gamepad funcione. Não há instalação automática silenciosa.

A primeira release só deve ser anunciada quando cada bloqueador crítico for
aprovado em hardware e o manifesto corresponder aos executáveis entregues.

# MAIZO Wii — inicialização segura no console real (frontend)

Esta mudança afeta somente o frontend MAIZO: NÃO muda WiiMC,
WiiSXRX, Nintendont, USB Loader GX, emuladores nem instalador.

## Proteções implementadas no código

- A VI permanece preta até existir um framebuffer efetivamente renderizado,
  evitando expor memória de vídeo não inicializada (faixas rosa/preto).
- O vídeo/GX inicia antes de FAT, biblioteca, WPAD e sondagem de HID.
- Painel de etapas antes de operações que podem bloquear:
  VIDEO GX OK; MONTANDO SD E USB; CARREGANDO CONFIGURACOES;
  VERIFICANDO MODULOS; INDEXANDO BIBLIOTECA; INICIALIZANDO CONTROLES.
- Estados GX definidos explicitamente: matriz, canal de cor, TEV e texturas.
- Snapshot da presença dos módulos evita abrir dezenas de arquivos por segundo
  no FAT durante a Home. Pressione Z na Home para atualizar e reindexar.
- Evicção de capas aguarda a GPU antes de liberar buffers de textura.
- Detecção USB HID adiada até depois dos primeiros quadros renderizados.
- Rótulos maiores na Home; suporte básico a transliteração de acentos UTF-8.

## Teste no Wii físico

1. Faça backup do boot.dol atual.
2. Substitua SOMENTE apps/maizo/boot.dol pelo novo executável.
3. Inicie pelo Homebrew Channel e anote a última etapa de boot visível.
4. Confirme Home, Jogos, Busca, Ajustes e diagnósticos HID.
5. Teste no CRT PAL-M/4:3 e no Dolphin com o MESMO DOL.
6. Teste com o receptor USB conectado e desconectado.
7. Se travar após a Home e suspeitar de HID, crie um arquivo vazio em
   sd:/maizo/disable-hid.flag ou usb:/maizo/disable-hid.flag e reinicie.
   Neste modo USB HID fica desligado; Wii Remote e controle GC podem funcionar.
8. Remova o arquivo disable-hid.flag para reativar a detecção USB HID.

## Leitura do diagnóstico

Parou em MONTANDO SD E USB? Investigue armazenamento e IOS.
Parou em INICIALIZANDO CONTROLES? Investigue WPAD.
A Home aparece antes de travar? Investigue USB HID ou biblioteca.
Faixas rosas antes da primeira mensagem? Investigue modo de vídeo,
VI/GX, framebuffer e lançamento pelo Homebrew Channel.

Testes estáticos e compilação em CI não substituem execução no Wii real.
Este commit reduz causas possíveis e melhora o diagnóstico, mas não garante
compatibilidade com todos os IOS, discos e controles.

# MaizoWii — sincronização Nintendont, verificação das capas e WiiMC HID

## Nintendont
O frontend salva os ajustes GameCube em settings.ini (globais) e em
game-profiles (por título). A partir desta integração, também escreve
`/nincfg.bin` em SD e USB quando os ajustes são **confirmados** no
MaizoWii, e não só quando um jogo é iniciado. É a estrutura NIN_CFG
original versão 0x0B do Nintendont. Modificações aplicadas:
Memory Card Emulation, widescreen, cheats, Skip IPL, MC Multi,
Native SI, Remove Limit, vídeo NTSC/PAL/PAL-M, VideoScale e VideoOffset.
Outras opções não expostas pelo Maizo são preservadas do NIN_CFG
válido já existente, incluindo idioma e número de controles.

Como `/nincfg.bin` é **um único conjunto de opções** (não uma base
de perfis), ao editar um jogo seu perfil Maizo fica salvo separadamente;
o último perfil aplicado atualiza o Nintendont. Abrir o próximo jogo
reaplica o perfil daquele jogo, sem perder os demais. A gravação
manual de opções NUNCA ativa auto-boot de um jogo antigo. Apenas lançar
um jogo no Maizo envia NIN_CFG com AUTO_BOOT e o caminho selecionado.
Uma falha em gravar no SD/USB é explicitada na interface. Não existe
correspondente Nintendont para tema, menu e configurações WiiMC.

## Capas — identificar jogo antes de baixar
* Wii: extrai ID6 da imagem WBFS em 0x200 ou ISO em 0.
* GameCube: extrai ID6 de ISO, GCM ou do primeiro bloco de CISO,
  verificando a palavra-mágica do console (não usa `CISO00` como ID).
* GBA: código de 4 bytes e título da ROM do cabeçalho nativo.
* GB/GBC e SNES: título interno validado como alternativa.
* NES/PS1: sem ID global confiável disponível nesses formatos; a
  identificação usa o arquivo e, quando presente, metadados externos.

O GameTDB usa o ID6 confirmado, evitando capa errada por diretório
renomeado. Para PS1/SNES/NES/GBA/GB, o Libretro tenta nome original
da ROM, título do catálogo e título interno, primeiro no CDN e depois
no espelho oficial no GitHub, nas regiões suportadas. Nome idêntico
não prova identidade binária. O aplicativo NÃO promete correspondência
criptográfica sem base No-Intro/Redump de hashes CRC/SHA.

Falhas de HTTP do GameTDB não interrompem toda a fila: seguem para
outros idiomas e fontes Libretro. A última falha e o código do jogo
ficam visíveis na tela. Para erros TLS, conferir data/hora do Wii e
CA oficial; conexões HTTPS não desabilitam a verificação de certificados.
Não há download de jogos.

## WiiMC HID
Antes o código ignorava **qualquer novo controle HID** sempre que um
dispositivo USB de armazenamento estava montado. Agora, durante os
menus e navegador, o adaptador tenta detectar controles de classe HID
mesmo com a unidade USB presente. Durante reprodução, não enumera
novos dispositivos; controles já abertos continuam recebendo relatórios
do endpoint interrupt, para preservar a leitura do vídeo.
O controle pode precisar estar conectado ao abrir o WiiMC, pois a
enumeração durante reprodução permanece restrita. HID universal
(XInput puro, receptores proprietários) não é garantido por libogc.

## Interface
Para instalações sem settings.ini, o tema inicial é **CLÁSSICO**.
O WiiMC-SSLC com HID é a versão padrão nas novas instalações; o WiiMC
original continua selecionável sem HID. 
Um tema já salvo pelo usuário continua sendo respeitado.

A CI valida código/compilação mas não substitui teste no Wii real,
com controle e pendrive conectado, internet e diferentes discos.

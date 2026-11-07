# MAIZO — Forçar largura VI 704 (CRT)

O controle fica em **HOME -> START (Configurações gerais) -> FORCAR VI 704**.
Padrão **DESLIGADO**, para não modificar TVs/configurações existentes.
Liga/desliga sem mexer na NAND, Priiloader, cIOS ou resolução vertical.

**704 significa largura da saída de vídeo VI em amostras horizontais.**
Não obriga o framebuffer interno a ter 704 pixels e não transforma
240p em 480i: 240p continua progressivo com altura original, mas a
janela horizontal VI passa a 704 amostras. O 480i mantém sua cadência.

## Aplicação de código

- Maizo frontend: viWidth=704, viXOrigin centrado em 8; modos AUTO,
  480i/576i e 240p/288p mantêm sua configuração vertical.
- Nintendont: NIN_CFG.VideoScale=104 -> largura 600+104=704 em escala
  Nintendont. O Nintendont pode aplicar outras regras de vídeo próprias.
- SNES9x GX, FCE Ultra GX (NES), VBA GX (GBA/GB): módulo **adaptado**
  lê `force_vi704=1` do registro de lançamento; menus e renderização do
  jogo ajustam a VI horizontal. O 240p usa seu modo próprio e não muda
  a altura. Compilações oficiais não modificadas ignoram essa configuração.
- WiiSXRX/WiiSX: o ajuste de VI já presente lê o novo flag de largura
  do registro; algumas rotas antigas do motor podem ignorar ajustes.
- WiiMC-SSLC adaptado: aplica 704 após montar armazenamento e ler perfil,
  inclusive após mudanças internas de modo de vídeo. WiiMC oficial não.
- USB Loader GX adaptado: faz patch **best effort** das estruturas
  GXRModeObjRVL reconhecidas no DOL dos jogos Wii, somente quando o
  usuário habilita a opção. Jogos que geram modos dinamicamente ou não
  usam essas estruturas podem ignorar; patch em memória ainda exige
  teste físico. Nunca escreve nos jogos originais em disco.

## Limites técnicos

**Não é possível impor largura de vídeo em todos os homebrews e jogos
simplesmente pelo frontend.** Ao iniciar outro DOL, ele pode alterar
seus próprios registradores VI. O Menu do Wii, Priiloader, HBC,
homebrews de terceiros não adaptados e jogos não reconhecidos não são
modificados. Para cada aplicativo adicional é necessário adaptar seu
renderizador/loader, quando o código ou API permitir. Não substituir
IOS, inserir patch residente na NAND ou alterar hardcode dos jogos
às cegas para tentar cobrir essas lacunas.

## Testes no Wii CRT

Salvar a versão anterior. Comparar 480i e 240p com opção desligada e
ligada, com o mesmo cabo e fonte. Testar especialmente SNES (224p,
239p, 480i), NES (240p/480i), GB/GBA, GameCube, Wii e mídia. Conferir
centragem com padrão em quadrantes e corte em ambas as laterais. No
caso de tela preta, iniciar pelo Priiloader/HBC anterior e restaurar
`force_vi704=0` no `/maizo/settings.ini`.

**Status:** implementação para revisão; sem validação em Wii físico,
não garante qualidade perfeita em toda TV nem compatibilidade universal.

# Maizo Wii — Super Interface v6.1 (2026-11-05)

## Implementado nesta revisão

Interface preserva o backend VI por software (YUYV) conhecido como
funcional no hardware. **Não introduz GX, Vulkan, alteração de IOS nem
mudança de formato de vídeo do console.**

- Layout visual retrabalhado: barra lateral, cartões de plataforma,
  estado SD e USB legível, foco destacado e contraste de CRT.
- Tela de boas-vindas na primeira utilização, exclusivamente por controle.
  Tema grafite como padrão, alternância por esquerda/direita.
  A/START/B concluem; o assistente não altera a NAND.
- Biblioteca com capas em cache, modos grade e lista, busca com contador
  real de resultados e detalhes com indicação do módulo selecionado.
- Movimento de navegação extraído para `source/nav.c`, compilável
  diretamente no host, com testes de zero itens, uma entrada, grade
  incompleta, bordas e rolagem alinhada. Evita divisão por zero e
  seleção de entradas fora do limite, sem teleporte incorreto por módulo.
- Diagnóstico de USB/SD apresentado na barra superior sem acesso ao
  armazenamento durante desenho de cada frame.
- Avisos de erro retornam para o contexto correto (detalhes/ajustes),
  em vez de descartar a navegação do usuário.
- Falha ao salvar opções globais passa a ser explícita.
- Menu distingue engine PS1 WiiSXRX/WiiSX 4.2.2 e WiiMC com HID/WiiMC
  original; catálogo geral reconhece ambos, mas detalhes conferem
  a variante escolhida, sem tentar abrir módulos errados.
- Leitura dos ajustes do jogo ao entrar em detalhes, sem `fopen`
  a cada quadro de vídeo.
- Módulos e arquivos dos emuladores previamente estáveis permanecem
  inalterados; o WAD v6 e a adaptação WiiMC de reprodução também.

## Controles do frontend

- Direcional / analógico: navegar
- A: selecionar/abrir
- B: voltar
- L/R: trocar sistema
- X: pesquisar
- Y: favorito
- START: ajustes; na pesquisa, mostrar resultados
- Z / Select (quando o perfil HID envia Z): atualizar a biblioteca
  na tela inicial ou abrir os diagnósticos nos ajustes.

## Verificações e limitações

O GitHub Actions valida o contrato da aplicação, executa testes
de navegação C com compilador host, compila frontend e forwarder, e
empacota os módulos. **Os resultados de CI não provam comportamento
em um Wii real, nem compatibilidade de receptores USB específicos.**
Para isso usar `docs/TESTES_WII.md`.

O WiiMC oficial continua sendo uma alternativa necessária para
isolamento de regressões de áudio/vídeo; ele não ganha suporte HID
automaticamente. Se a versão HID causar tela preta, selecionar
**WIIMC ORIGINAL** nas opções do sistema e registrar o resultado.

## Pendências não resolvidas automaticamente

- Confirmar conectividade de Twin USB 0810:0001 e receptor Xbox 360
  em Wii real; não presumir que um `.ini` fornece driver XInput.
- Testar WAD no console com boot e USB após IOS58, sem instalar canais
  perigosos ou mudar configurações da NAND.
- Validar áudio/vídeo, legendas e HID simultaneamente no WiiMC
  recompilado, inclusive retorno ao frontend e vídeos AVI/MP4.
- Testar Wii, GameCube, PS1, SNES, NES, GBA e GB no aparelho.
- Trabalhos futuros de UX: fotos e metadados automáticos e
  traduções completas; não são anunciados como concluídos.

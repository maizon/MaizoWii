# MAIZOWII ULTIMATE v1.0.0-rc.1 — PRÉ-RELEASE EXPERIMENTAL

Esta versão **não é a release estável v1.0.0**. O pacote foi preparado por CI e **não é prova de funcionamento em um Wii físico**. O WAD de instalação de canal foi **excluído** desta distribuição por não ter sido validado na NAND.

## Alterações deste candidato
- Barra superior com ícones dos nove sistemas, destaque circular suave e cache residente separado das capas dos jogos.
- Tema OLED como padrão em instalação nova, preservando temas previamente salvos.
- WiiMC: correções direcionadas a entrada nula da lista de dispositivos, pasta pai vazia e nomes vazios na navegação FAT. O motor original de áudio/vídeo permanece; **o travamento real ainda precisa de reteste**.
- GameCube: validação contra truncamento de GamePath[255], versão e tamanho de NIN_CFG; **autoboot ainda requer verificação no Wii real**.
- Perfis de vídeo dos jogos deixam de herdar automaticamente os ajustes da interface.
- Tela de detalhes com metadados conhecidos ou indicador desconhecido, acesso Y a opções individuais e descrição de homebrew quando presente em meta.xml.

## Instalação
Extraia a raiz do ZIP em um dispositivo FAT32 e inicie apps/maizo/boot.dol pelo Homebrew Channel. Não remova os diretórios maizo, controllers e apps. Nenhuma instalação de canal NAND é necessária para testar esta versão.

## O que continua PENDENTE
Testes em hardware real da navegação SD/USB e reprodução no WiiMC, USB HID genérico, boot direto de GameCube, demais emuladores, retorno ao frontend, arquivos problemáticos, saves, modos CRT (240p/288p/480i/576i) e compatibilidade com seu controle. Emuladores com fallback oficial não ganham HID apenas por estarem no pacote. Arquivos ELF de homebrew não estão habilitados sem carregador verificado.

Os hashes dos executáveis e a origem de cada módulo aparecem em maizo/MODULE_MANIFEST.json. Use os logs do GitHub Actions para distinguir compilação e testes de contrato de testes no console. Relate falhas com a versão e o módulo exatos.

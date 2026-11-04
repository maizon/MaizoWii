# MAIZO Wii — teste físico de integração (NÃO executado automaticamente)

Execute em Wii real com backup da NAND e configurações de Priiloader adequadas.
Use inicialmente vídeo AUTO, sem forçar PAL-M, e evite alterar o IOS do System Menu.
Registre modelo do console, versão do Homebrew Channel, IOS em uso, porta USB,
VID/PID e mensagem exata de erro. Se algum teste travar, marque FALHA, não PASS.

| ID | Passo reproduzível | Evidência mínima | Estado |
| --- | --- | --- | --- |
| R04 | Partindo do HBC, abrir cada módulo pelo MAIZO e sair | Tela de inicialização, retorno ou modo de recuperação por módulo | PENDENTE |
| R05 | Selecionar WBFS identificado por ID6 com porta/IOS configurados | Jogo Wii efetivamente executado | PENDENTE |
| R06 | Selecionar games/Titulo/game.iso no mesmo dispositivo do Nintendont | Jogo GameCube inicia e saída retorna adequadamente | PENDENTE |
| R07 | Selecionar CUE+BIN compatíveis com WiiSXRX | Jogo PS1 com imagem, som e controles | PENDENTE |
| R08 | Abrir SFC, NES, GBA e GBC em seus motores GX | Quatro ROMs efetivamente em execução | PENDENTE |
| R09 | Abrir AVI e MP4 suportados no WiiMC-SSLC | Dois vídeos com imagem e som | PENDENTE |
| R10 | Microntek 0079:0006 no USB | A/B/direções/troca de abas, desconectar/reconectar | PENDENTE |
| R11 | Twin Wireless 0810:0001 no USB | VID/PID reais, relatório HID dos 2 pads e ausência de congelamento | PENDENTE |
| R12 | Entrar em cada DOL adaptado com controle USB | Resposta dos botões no módulo, não apenas no MAIZO | PENDENTE |
| R13 | Usar TV CRT 4:3, composto e componente | Sem texto cortado, pesquisa e todos menus navegáveis | PENDENTE |
| R14 | Ajustar opções, reiniciar console e relançar jogo | Preferências por jogo preservadas e aplicadas | PENDENTE |
| R15 | Biblioteca grande, capas ausentes, remoção/reinserção da mídia | Sem crash e uso estável de memória | PENDENTE |
| R16 | Extrair ZIP na raiz FAT32 e conferir manifest/hashes | Oito DOLs válidos e licenças incluídas | PENDENTE |
| R17 | Abrir AJUSTES com Microntek e Twin plugados separadamente | VID/PID/status/relatórios/erros legíveis; Twin sem perfil | PENDENTE |
| R18 | Executar job CI launch-contracts | GitHub Actions sucesso com C real do frontend | PENDENTE CI |
| R20 | Reexecutar todos testes com ZIP completo | Build, hashes e lista de resultados assinada | PENDENTE |

**Atenção:** HID no menu não atravessa a troca do DOL; Nintendont, emuladores GX,
WiiSXRX e WiiMC exigem mecanismos próprios. O receptor Twin não possui mapa
comprovado no Wii e o código deliberadamente não inventa bytes de controle.

Nenhuma linha acima constitui alegação de teste já realizado no console.

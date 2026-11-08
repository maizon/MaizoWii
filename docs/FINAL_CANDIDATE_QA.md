# MaizoWii — candidate único de correções de inicialização e interface

> **Release candidate / teste obrigatório no Wii real.** Um CI verde demonstra compilação e testes automatizados. Não comprova o reconhecimento do USB na cIOS do usuário, tampouco autoboot real do Priiloader, HDMI/CRT ou compatibilidade de cada ISO.

## Falhas reais relatadas

- Wii: o USB Loader GX abre a própria tela em vez do jogo e não encontra o USB.
- GameCube: Nintendont abre a própria interface em vez da ISO selecionada.
- Priiloader: Installed File manual funcionou em versão anterior, mas autoboot USB de algumas versões falhou.

## Intervenções desta candidate

1. **USB Loader GX:** o frontend agora envia o ID6 do disco e **ambos** `-ios=N` (IOS do loader) e `-bootios=N` (IOS inicial), além da porta. O USB Loader GX realmente interpreta ambos separadamente. Há opção de porta 0, 1 e AMBAS/AUTO, que usa o modo 2 já aceito pelo loader. O padrão permanece porta 0 para não alterar configurações que já funcionem. Não instala nem altera cIOS.
2. **Nintendont:** mantém o argumento binário NIN_CFG v0x0B e o caminho da ISO relativo ao dispositivo, mas salva o `nincfg.bin` nos dois dispositivos FAT montados. O Nintendont pode escolher o SD como dispositivo primário mesmo ao executar um jogo no USB; antes o fallback só era gravado na raiz do dispositivo do jogo. Mantém a inicialização sem abortar por erro de escrita no cache. O backup usa sufixo `.maizobak` se FAT negar substituição direta. **Atenção: como o loader original, uma inicialização pode substituir `nincfg.bin` existente na raiz. Faça cópia das configurações pessoais antes.**
3. **Handoff:** espelha o NUL terminal extra do lançador upstream, sem modificar o tamanho do NIN_CFG, a ordem dos argumentos ou o `argc`.
4. **UI:** logo MAIZO fixa (posição 25,23) na camada final de todas as telas, inclusive carregamento; títulos e status USB/SD na home e na biblioteca; foco discreto na capa; seleção visível ao rolar opções longas; abertura do módulo com tela de estado.
5. **Vídeo:** preserva o Force 704 para módulos compatíveis, mas o **WiiMC continua explicitamente isento**.

## Matriz de aceitação no console (não marcada como concluída)

| Fluxo | Esperado | Estado |
| --- | --- | --- |
| Priiloader > Installed File, manual | Abre MaizoWii + navega com HID | A reconfirmar na candidate |
| Priiloader > Autoboot > Installed File, ligar a frio | Inicializa USB sem prender no boot | A reconfirmar |
| Wii > ISO/WBFS no USB, porta 0/1/AUTO | Vai direto ao título, não ao menu GX | A reconfirmar |
| GameCube > ISO no USB e SD presente | Vai direto ao título, não ao menu Nintendont | A reconfirmar |
| GameCube > ISO apenas no USB | Vai direto ao título | A reconfirmar |
| SNES/NES/GBA/PS1 | Executa ROM/ISO sem tela preta | A reconfirmar por módulo |
| CRT 240p/480i e 704 opcional | Centraliza, sem perder vídeo | A reconfirmar |
| WiiMC | Sem regressão no vídeo que já funcionava | A reconfirmar |
| Controle Microntek 0079:0006 e outros HID | Menu e módulos adaptados navegam | A reconfirmar |

## Se o Wii ainda não aparecer no USB Loader GX

Verificar a porta física, formato da partição e cIOS/IOS selecionada. O MaizoWii **não pode suprir uma cIOS ausente ou alterar o driver de armazenamento de todo jogo**, e o modo AMBAS/AUTO só funcionará quando suportado pela cIOS/loader. O arquivo `maizo/maizo-launch.txt` inclui o ID/caminho do jogo e, para Wii, o IOS e porta solicitados para comparação com o loader.

## Limitações explícitas

Retorno ao MaizoWii a partir de **qualquer** jogo Wii não pode ser garantido apenas alterando o frontend: cada loader/jogo controla o retorno. A simples presença de fontes adaptadas na CI não comprova que o binário adaptado foi incluído no ZIP: conferir o manifesto de módulos e eventuais fallbacks oficiais. O WAD MIZN previamente fornecido pelo usuário não deve ser substituído por WAD desconhecido.

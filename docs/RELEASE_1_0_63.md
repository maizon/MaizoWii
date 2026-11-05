# MaizoWii 1.0 — matriz de aceitação interna (63 requisitos)

> NÃO É UMA RELEASE. `implementado` significa que existe código/integração, não que todas as variantes foram aprovadas em hardware. Os 63 só estarão concluídos após testes físicos e funcionais. Este arquivo não autoriza publicar/taguear 1.0.

| Nº | Requisito | Estado atual |
|---:|---|---|
| 1 | Ícones visuais por plataforma | parcial |
| 2 | Capas retangulares sem distorção | implementado |
| 3 | Proporções próprias por console | parcial |
| 4 | Perspectiva do carrossel | parcial |
| 5 | Luz ambiente baseada nas capas | implementado |
| 6 | Degradês e transições visuais | implementado |
| 7 | Animação de seleção e execução | parcial |
| 8 | Destaque de capa selecionada | implementado |
| 9 | Tipografia adequada à CRT | implementado |
| 10 | Temas Graphite/OLED/Classic | implementado |
| 11 | Layout consistente em 4:3 e 16:9 | parcial |
| 12 | Otimização da UI visando 60 FPS | parcial |
| 13 | Baixar capas automaticamente no Wii | pendente |
| 14 | Resolver capas por ID, título e região | parcial |
| 15 | Cache persistente de capas | implementado |
| 16 | Baixar apenas capas ausentes | parcial |
| 17 | Metadados ano, empresa e gênero | pendente |
| 18 | Varredura USB e SD | implementado |
| 19 | Biblioteca unificada | implementado |
| 20 | Atualização de biblioteca em uso | parcial |
| 21 | Falha segura ao remover USB ou SD | parcial |
| 22 | Pastas e filtros por sistema | implementado |
| 23 | Índice de biblioteca para abrir rápido | parcial |
| 24 | Placeholder seguro quando faltar capa | implementado |
| 25 | Jogar diretamente pela capa | implementado |
| 26 | Tela de detalhes/opções | implementado |
| 27 | Informações individuais do jogo | parcial |
| 28 | Cheats gerenciáveis por jogo | pendente |
| 29 | Saves gerenciáveis por jogo | pendente |
| 30 | Configuração individual persistida | implementado |
| 31 | Configurações gerais | parcial |
| 32 | Configuração global/sistema/jogo | parcial |
| 33 | Categorias Vídeo/Áudio/Controles etc. | implementado |
| 34 | Exibir apenas opções realmente suportadas | implementado |
| 35 | Herança global→sistema→jogo | implementado |
| 36 | Remapeamento por sistema | pendente |
| 37 | Remapeamento por jogo | pendente |
| 38 | Perfis PlayStation/Xbox/GameCube | pendente |
| 39 | Sensibilidade de analógicos | pendente |
| 40 | Zonas mortas dos analógicos | pendente |
| 41 | Modo de vídeo automático | parcial |
| 42 | Modos 240p/288p/480i/576i/480p seguros | parcial |
| 43 | Padrões NTSC/PAL/PAL60/PAL-M compatíveis | parcial |
| 44 | Ajuste real de geometria de vídeo | implementado |
| 45 | Proporção 4:3/16:9 | parcial |
| 46 | Opção deflicker | parcial |
| 47 | Confirmar/reverter modo de vídeo | pendente |
| 48 | Modo de vídeo de recuperação | pendente |
| 49 | Transmitir parâmetros de vídeo aos núcleos | parcial |
| 50 | Prevenir promessas de vídeo universal | parcial |
| 51 | USB Loader GX abre jogo Wii diretamente | parcial |
| 52 | USB Loader GX navega com HID USB | parcial |
| 53 | Preservar WiiMC adaptado funcional | implementado |
| 54 | Preservar emuladores e Nintendont | implementado |
| 55 | Navegação global pelo gamepad | implementado |
| 56 | Falhas seguras dos módulos | implementado |
| 57 | Uso eficiente de texturas/MEM1/MEM2 | parcial |
| 58 | Pré-carregamento e animação leves | parcial |
| 59 | Compilação incremental com testes | implementado |
| 60 | Pacote USB/SD verificável | implementado |
| 61 | Tela de inicialização Maizo funcional | implementado |
| 62 | Carrossel sem tremor ao mudar de capa | implementado |
| 63 | Validação completa sem regressões no Wii físico | pendente |

## Condições obrigatórias para a primeira versão pública

1. Compilar frontend, módulo HID do USB Loader GX, WiiMC e demais núcleos sem regressões; preservar hashes/manifests e recuperação do instalador.
2. Testar Wii real com USB HID Microntek 0079:0006 e controle sem Wiimote, troca rápida de capas, reconexão de dispositivo e biblioteca vazia/cheia.
3. Comprovar abertura direta de Wii/GC/PS1/SNES/NES/GBA/GB e vídeo com áudio; testar modo SD e USB. Nenhum teste de host implica sucesso no console.
4. Aferir FPS reais e frametimes em NTSC/PAL-M/PAL; a meta de 60 FPS não é garantia de 60 em saída de vídeo a 50 Hz.
5. Confirmar modos de vídeo sem imagem preta e com retorno automático, especialmente CRT PAL-M; nunca aplicar forçadamente 480p em CRT 480i.
6. Concluir download e identificação de capas dentro do Wii; a ferramenta atual `tools/cover_sync.py --convert` é externa (Android/PC) e não cumpre o item 13.
7. Validar forwarder/WAD no aparelho antes de recomendá-lo como instalação segura; pacote atual permanece experimental.

## Mudanças do candidato de correção visual

Carrossel usa slots fixos de inteiros definidos em `source/carousel_layout.c`; nenhuma translação global de 9 px. Capa é carregada fora da rotina de desenho por `cover_prefetch`; ausência é armazenada em cache negativo e o botão de reindexação invalida caches. Vídeo otimizado com LUT horizontal e conversão única de cores sólidas.

**Atenção:** A matriz será atualizada apenas após evidência testável; não transformar o estado `parcial` em `implementado` para inflar a contagem.

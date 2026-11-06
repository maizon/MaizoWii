# MaizoWii v1.0.0 — matriz de aceitação da candidata

> **NÃO VALIDADO NO WII REAL.** Esta matriz registra implementação de código, não declara uma v1.0.0 estável. O commit ainda depende de CI devkitPPC, testes de rede reais e testes físicos com USB HID/WiiMC. A revisão deste documento não autoriza publicação de versão estável.

## Regras de leitura

- **IMPLEMENTADO (sem validação física)**: o fluxo principal foi implementado no código-fonte, mas sua confiabilidade em console real ainda não está comprovada; NÃO equivale a VALIDADO.
- **PENDENTE / PARCIAL**: falta funcionalidade, validação ou cobertura exigida no pedido.
- **Os testes de contrato do host não substituem testes de hardware.** O download externo depende das fontes públicas, das condições TLS e da rede real do Wii.

| ID | Requisito original resumido | Estado nesta candidata | Evidência / área de código |
|---|---|---|---|
| R001 | Download de capas dos sistemas | PENDENTE / PARCIAL | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R002 | Download de todos os sistemas | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R003 | Download filtrado por sistema | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R004 | Baixar apenas ausentes | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R005 | Atualização de existentes | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R006 | Repetir falhas da sessão | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R007 | Capa individual | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R008 | Múltiplas fontes públicas | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R009 | Fallback entre fontes | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R010 | Selecionar fonte preferida | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R011 | Tipos de imagem | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R012 | Preferência de região | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R013 | Progresso real | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R014 | Cancelamento cooperativo | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R015 | Retomada parcial de downloads | PENDENTE / PARCIAL | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R016 | Validação PNG e GXT | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R017 | Preservar capas existentes | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R018 | Capas offline | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R019 | Limpeza/reconstrução do cache | IMPLEMENTADO (sem validação física) | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R020 | Diagnóstico completo por sistema | PENDENTE / PARCIAL | `source/cover_download.c`; `source/main.c`; `source/ui.c` |
| R021 | START global na HOME | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R022 | START nas configurações do sistema | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R023 | Menus por sistema | PENDENTE / PARCIAL | `source/settings.c`; `source/config.c`; `source/main.c` |
| R024 | Opções exclusivas suportadas | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R025 | Três escopos de configurações | PENDENTE / PARCIAL | `source/settings.c`; `source/config.c`; `source/main.c` |
| R026 | Prioridade por jogo | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R027 | Vídeo frontend independente | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R028 | Capas dentro das gerais | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R029 | Capas dentro de cada sistema | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R030 | B volta ao menu anterior | IMPLEMENTADO (sem validação física) | `source/settings.c`; `source/config.c`; `source/main.c` |
| R031 | Topbar oculta na HOME | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R032 | Halo oculto na HOME | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R033 | Carrossel principal de consoles | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R034 | Nome do console abaixo da capa | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R035 | Jogos Disponiveis X | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R036 | Topbar somente dentro do sistema | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R037 | Remover fundo quadrado do ícone | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R038 | Alpha verdadeira nos sprites | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R039 | Preservar círculo seletor | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R040 | Círculo atrás do ícone | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R041 | L/R mudam sistema na biblioteca | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R042 | Círculo acompanha categoria | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R043 | B volta à HOME | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R044 | Transições suaves de entrada e saída | PENDENTE / PARCIAL | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R045 | Preservar geometria do carrossel | IMPLEMENTADO (sem validação física) | `source/ui.c`; `source/main.c`; `tools/prepare_system_icons.py` |
| R046 | Database local em todos sistemas | PENDENTE / PARCIAL | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R047 | Títulos oficiais preferidos | PENDENTE / PARCIAL | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R048 | ID/hash/metadados conforme sistema | PENDENTE / PARCIAL | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R049 | Fallback ROM | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R050 | Fallback nome da pasta | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R051 | Limpar extensão | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R052 | Nome sob a capa | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R053 | Região quando conhecida | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R054 | Ano de lançamento quando conhecido | PENDENTE / PARCIAL | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R055 | Cache de metadados | PENDENTE / PARCIAL | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R056 | Sem banco na renderização | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R057 | Biblioteca sem internet | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R058 | Atualizar dados na reindexação | IMPLEMENTADO (sem validação física) | `source/library.c`; `data/wiitdb-en.txt`; `source/ui.c` |
| R059 | Evitar reindexação ao trocar sistema | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R060 | Cache de imagens e ícones | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R061 | Sem FAT na renderização | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R062 | Animações leves | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R063 | Preservar modos VI | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R064 | Legibilidade em CRT | PENDENTE / PARCIAL | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R065 | Hotplug robusto | PENDENTE / PARCIAL | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R066 | Preservar HID | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R067 | Preservar emuladores sem mudanças | IMPLEMENTADO (sem validação física) | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R068 | Nenhuma regressão nos módulos | PENDENTE / PARCIAL | `source/covers.c`; `source/video_sw.c`; `source/main.c` |
| R069 | Testes para gerenciador de capas | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R070 | Testes de fontes e fallback | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R071 | Testes reais dos tipos de download | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R072 | Testes de cancelamento/erro | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R073 | Testar START contextual | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R074 | Testar L/R e B | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R075 | Testar database e fallback | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R076 | Compilar frontend com devkitPPC | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R077 | Reutilizar artefatos compatíveis | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R078 | Verificar ZIP final | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R079 | Publicar artefatos da CI | PENDENTE / PARCIAL | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |
| R080 | Registrar evidências por ambiente | IMPLEMENTADO (sem validação física) | `tools/test_v1_candidate.py`; `.github/workflows/build.yml` |

## Riscos de estabilidade conhecidos

- WiiMC original e adaptado não exibiram a GUI nas capturas automáticas do Dolphin headless. Resultado anterior inconclusivo; o Wii físico é indispensável para certificar reprodução real.
- A instalação do YAWM HID não foi comprovada; não publicar WAD da NAND sem validação independente.
- As fontes GameTDB e Libretro são HTTPS e podem mudar; requisições devem ser testadas em hardware real. Downloads utilizam libcurl multi para manter cancelamento entre passos, mas DNS da libc do Wii pode ainda bloquear momentaneamente.
- A database oficial empacotada é a GameTDB Wii/GameCube EN distribuída no projeto Dolphin (snapshot 2025); outras plataformas continuam com fallback para ROM/pasta. Identificação oficial ampla por CRC/serial e anos de lançamento NÃO foi concluída.
- O gerenciador oferece tipos, fontes e regiões, mas nem todas as combinações têm imagens disponíveis nas fontes públicas. Retomada byte a byte de PNG incompleto NÃO está implementada; arquivos íntegros são preservados.
- Contagem de capas disponíveis/ausentes é fornecida, mas detecção detalhada de imagem corrompida é parcial.

## Critério de promoção

**Não marcar v1.0.0 como estável nem publicar a tag v1.0.0 enquanto existirem itens PENDENTE / PARCIAL e bloqueadores críticos sem verificação.** Preserve a tag anterior v1.0.0-rc.1. Não instalar WAD experimental.

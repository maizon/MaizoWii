# Arquitetura do MAIZO Wii — integração modular

## Camadas

```
GameCube / WPAD / USB HID
          |
frontend em libogc (GX, teclado e páginas)
          |
índice incremental USB/SD + preferências/favoritos + capa GX
          |
adaptador por sistema: NIN_CFG / argv WiiSXRX / argv USBLoaderGX
          |
transferência de execução para o DOL externo (experimental)
```

## Telas

1. **Home:** grade 4x2 com oito abas, favoritos por aba e estado do módulo.
2. **Biblioteca:** grade com capas ou lista (oito itens por página),
   contagem filtrada por plataforma, indicadores de favorito e pesquisa.
3. **Detalhes:** capa, caminho, Game ID, motor e botão iniciar.
4. **Busca:** teclado 8 colunas, campo de texto e aplicação da pesquisa.
5. **Configurações:** tema, layout e parâmetros específicos do módulo
   selecionado. Somente ajustes aplicáveis pelo contrato são editáveis.
6. **Avisos:** módulo ausente, arquivo não suportado, falha de leitura.
7. **Evoluções:** downloads HTTP autenticados no console, gerenciador de
   unidades/pastas, perfis por jogo, diagnóstico integrado de USB, menu
   adaptado nos módulos, retorno da execução.

O frontend **não executa a ROM internamente**. Ao trocar de DOL, o
emulador assume a interface e os controladores. A ABI de passagem de
parâmetros é diferente em cada um.

## Organização

- `source/main.c`: estado, transições e callbacks de navegação.
- `source/ui.c`, `source/covers.c`: renderização GX/texturas.
- `source/library.c`: DFS limitado por profundidade, filtros e IDs;
  varredura incremental com orçamento por quadro.
- `source/input.c`: Wii Remote, até quatro GameCube pads, joystick e
  perfis HID com leitura USB assíncrona.
- `source/settings.c`, `source/config.c`: tabela de opções por sistema,
  gravação de preferências no FAT.
- `source/launch.c`: resolução de módulos, ABI NIN_CFG, USB Loader GX
  , WiiSXRX e WiiMC, carregador DOL ainda experimental.
- `tools/cover_sync.py`: download externo de artes por ID6; conversor
  `tools/build_cover_cache.py`: formato GX MZG1.

## Requisitos antes de lançamento estável

- Testar o carregador DOL contra sobreposição de MEM1/MEM2, IOS, memória
  de argumentos e retorno ao loader em Wii real.
- Confirmar USB Loader GX Enhanced identificando jogos pelos argumentos
  de ID6, e Nintendont lendo nincfg.bin de USB e SD.
- Implementar autoboot nos emuladores GX por patch de upstream;
  não inventar suporte CLI.
- Implementar suporte USB HID **em cada módulo**, compatível com
  0079:0006 e 0810:0001 após análise de descritores/relatórios no Wii.
- Inspecionar o suporte real de CHD/M3U no WiiSXRX antes de habilitar.
- Melhorar reconexão e tratamento de USB host cIOS.
- Implementar downloads de arte no Wii se houver TLS/HTTP confiável e
  fallback sem rede, respeitando limites de memória.
- Corrigir normalização e detecção de disco múltiplo, ZIP e região
  conforme formato de cada sistema.

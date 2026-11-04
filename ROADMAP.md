# MAIZO Wii — backlog verificável

Base: main d4f14cb (03/11/2026). Cada item é uma capacidade independente com critério de aceite. Itens com CI verde não substituem teste físico.

- [x] R01 — Frontend compila no CI com libogc/devkitPPC. Aceite: workflow build verde na main.
- [x] R02 — Catálogo de oito categorias e varredura de ROMs. Aceite: contrato estático e build CI existentes.
- [x] R03 — Empacotador valida executáveis DOL. Aceite: testes unitários sintéticos existentes.
- [ ] R04 — Troca de DOL confiável em Wii real. Aceite: lançar e voltar de cada módulo sem travamento.
- [ ] R05 — Wii USB Loader GX autoboot. Aceite: jogo Wii real inicia pelo ID6 com IOS selecionado.
- [ ] R06 — Nintendont autoboot e retorno. Aceite: GC inicia com config e retorna ao MAIZO.
- [ ] R07 — WiiSXRX autoboot, vídeo e áudio. Aceite: BIN/CUE de teste inicia sem tela preta.
- [ ] R08 — Emuladores SNES/NES/GB/GBA iniciam ROMs. Aceite: um jogo por sistema no Wii.
- [ ] R09 — WiiMC-SSLC abre e reproduz mídia. Aceite: AVI/MP4 compatível com áudio, sem travar.
- [ ] R10 — Microntek 0079:0006 funciona no frontend físico. Aceite: navegação completa e reconexão.
- [ ] R11 — Twin USB 0810:0001 autodetectado. Aceite: relatório HID capturado no Wii e dois pads testados.
- [ ] R12 — Suporte HID nos módulos adaptados. Aceite: cada DOL adaptado compilado e controle testado.
- [ ] R13 — UI NeoStation completa e calibrada CRT. Aceite: telas e navegação validadas em 4:3.
- [ ] R14 — Ajustes por jogo persistentes. Aceite: editar, reiniciar e relançar com mesmas opções.
- [ ] R15 — Capas e cache robustos. Aceite: biblioteca grande, imagens ausentes e mídia removida sem crash.
- [ ] R16 — Pacote integral reproduzível e licenciado. Aceite: manifest, hashes e todos módulos compatíveis.
- [ ] R17 — Diagnóstico USB/HID em tela. Aceite: VID/PID, estado e falhas visíveis no Wii. Código de status e contadores introduzido; testar visualização em Wii físico.
- [x] R18 — Testes automatizados para contratos reais de autoboot. Aceite: fixtures executando o C de produção e CI verde. Testes nativos de argv dos oito sistemas aprovados no workflow 41808890786 (job launch-contracts).
- [x] R19 — Cobertura de segurança do empacotador. Aceite: ZIP traversal, drives, symlinks e fallback DOL testados.
- [ ] R20 — Release validado no console. Aceite: smoke test completo e ZIP final documentado.

Nota: R01–R03 e R19 são evidências de CI/código, não garantia de funcionamento no Wii. R18 validado pelo job launch-contracts do workflow 41808890786; os demais requisitos de hardware continuam pendentes. Não marcar R04–R17/R20 por implementação parcial ou simulação host.


- [ ] R21 — Canal forwarder e instalador: fornecer WAD proprio verificavel,
      instalar via DOL com confirmacao e demonstrar funcionamento no Wii.
      O forwarder HBC e o YAWM interativo sao parciais e nao fecham R21.

# PROGRESSO — execução 2026-11-03

Base: `d4f14cb13961fd55a67c00ad57dd9638fff53588`.
Backlog inicial: 17 pendências (R04–R20). Meta aspiracional desta execução: ceil(17 × 0,30) = **6 itens**.
Concluídos com evidência nesta execução: **0/17 até confirmação do CI**. R19 recebe testes e correção propostos, mas permanece pendente até CI verde.

Mudanças propostas:
- Empacotador não falha imediatamente se o DOL preferido estiver corrompido e houver outro executável válido.
- ZIP com caminho absoluto de drive Windows é rejeitado antes da extração.
- Testes de regressão para fallback, forwarder e caminho com drive.
- Roadmap auditável e critérios de aceite.

Verificações anteriores: workflows build e upstream HID na main estavam verdes (GitHub Actions runs 41757066210 e 41757066264). **Essas execuções não validam esta PR**.
Pendências de maior risco: execução física do bootstub, compatibilidade HID em módulos e WiiMC/PS1.
Não foi alcançada a meta de 30%; continuar pelos contratos de autoboot e testes de hardware.

## Execução 2026-11-04
- CI da PR #1 (run 41763267253) falhou: `test_zip_windows_drive_blocked` não levantou ValueError; compilação do frontend foi pulada.
- Correção aplicada na validação de caminhos ZIP com drive Windows e regressão adicional para barras invertidas.
- 17 pendências iniciais; meta 6. Nenhum item novo considerado concluído antes de CI verde e aceites respectivos.

## Execução 2026-11-04 — verificação posterior
- GitHub Actions run 41769137203 da PR #1 concluiu com **success** para commit 8bde27b, validando o conjunto de regressões anterior e o build automatizado.
- R19 marcado concluído segundo seu critério de aceite de testes sintéticos e CI. O teste não substitui ensaio de arquivos maliciosos em ambiente isolado.
- Backlog no início desta execução: 17 itens (R04–R20); meta 6; itens concluídos confirmados: 1 (R19). Restam 16. Meta de 30% não atingida.
- Novos testes propostos para ZIP symlink Unix e caminho absoluto; verificar CI do novo commit antes de atribuir êxito aos novos testes.
- Próxima prioridade: R18 contratos de autoboot; R04–R17 e R20 dependem de integração ou ensaios reais no Wii.

## Execução 2026-11-04 — endurecimento adicional do ZIP
- Início: 16 pendências (R04–R18, R20); meta ceil(16 × 0,30) = 5 itens.
- CI do commit b500965d: workflow 41775432315 **success**.
- Correção proposta: normalização explícita de barras invertidas na extração ZIP, rejeição de caminhos duplicados sem distinção de caixa e tipos especiais Unix; testes sintéticos novos.
- A validação CI deste commit e testes no Wii ainda precisam ser observados. Nenhum item novo marcado concluído nesta execução; R19 permanece concluído com CI anterior.
- R18 permanece pendente: ainda faltam fixtures dos contratos reais de autoboot. Meta de 30% não atingida.

## Execução 2026-11-04 — colisões de diretórios ZIP
- Início: 16 pendências (R04–R18, R20); meta ceil(16 × 0,30) = 5 itens.
- CI do commit anterior 2c01de8: run 41782080860 concluído com success.
- Corrigida validação pré-extração de ZIP para rejeitar arquivo que se apresenta como diretório pai de outro membro, em qualquer ordem, inclusive com diferença de caixa.
- Adicionados testes sintéticos de regressão; aprovação de CI do novo commit ainda pendente. Nenhum novo item do roadmap marcado concluído: 4/20 no total, 16 pendentes.
- Próximo: R18 fixtures de autoboot; R04–R17/R20 exigem implementação e/ou testes físicos adicionais.

## Execução 2026-11-04 — nomes incompatíveis com FAT32
- Início: 16 pendências; meta ceil(16 × 0,30) = 5 itens.
- Detectado risco adicional: ZIPs com nomes reservados DOS, caracteres proibidos e espaços/pontos finais podem ser aceitos no host mas falhar ou colidir no FAT32 do Wii.
- Adicionada validação pré-extração e regressões sintéticas para nomes inválidos e caminhos válidos.
- Mudança proposta para CI; sem comprovação de execução no Wii. Nenhum novo item R04–R18/R20 marcado concluído. R18 continua pendente.

## Execução 2026-11-04 — componentes ambíguos ZIP
- Início: 16 pendências; meta ceil(16 × 0,30) = 5 itens.
- Revisão identificou que PurePosixPath normaliza componentes "." e separadores repetidos antes da checagem; a extração poderia aceitar aliases ambíguos.
- Rejeição explícita de componentes vazios, "." e ".." antes da normalização; preservadas entradas convencionais de diretório terminadas em "/".
- Acrescentadas duas regressões sintéticas. CI do novo commit ainda não confirmado; 0 itens novos concluídos, 16 pendências permanecem. R18 e ensaios físicos ainda bloqueiam o progresso maior.

## Execução 2026-11-04 — autoboot testável, diagnóstico e CI
- Ponto de partida: 16 itens pendentes (R04–R18, R20); meta ceil(16 × 0,30) = **5 itens**.
- Causa raiz apurada do CI anterior (run 41802871787): test_zip_backslash_normalized usava **dois** backslashes no nome ZIP e contradizia a nova regra de caminhos ambíguos; corrigido fixture de barra simples e criada regressão separada para barra duplicada.
- Extraído construtor de argumentos real para source/launch_args.c e conectado ao código de lançamento source/launch.c; a contagem argc agora deriva do bloco produzido, em vez de tabela manual independente.
- O autoboot Wii sem ID6 válido falha de forma explícita, evitando sucesso aparente que apenas abria o menu do USB Loader GX. Paths e opções inválidas agora produzem mensagens de erro antes de transferir controle.
- Teste de contrato **dinâmico** via ctypes: compila source/launch_args.c com GCC nativo e exercita os oito sistemas, NULs, argc, áudio, IOS, nomes de mídia e caminhos incorretos. launch-contracts adicionado ao workflow e como condição da geração do pacote completo.
- Diagnóstico de HID parcial em AJUSTES: VID/PID visto, estado, perfil, relatórios e erros. Twin 0810:0001 continua **sem perfil validado**, o código não tenta abrir dispositivo não suportado.
- Evidência de desenvolvimento local: **7 testes de unidade do construtor nativo PASS** com GCC; compilação devkitPPC, suíte ZIP completa e workflow GitHub Actions da alteração aguardam execução/resultado (não presumir verde).
- Formalmente novos itens fechados antes do CI novo: **0**. R18 é candidato de conclusão após CI; R17 requer testar no Wii. Mantidos 4/20 concluídos e 16 pendentes até confirmação adicional.
- Bloqueio incontornável nesta execução: não há Wii físico sob controle do agente, e os aceites R04–R13/R17/R20 exigem funcionamento real de DOL/IOS/USB, controles e vídeo. Progresso abaixo da meta de 30%; próximo passo é examinar CI deste commit e resolver eventual erro, depois coletar checklist de ensaio físico em docs/TESTES_WII.md.

## Execução 2026-11-04 — integridade dos DOLs finais
- Base da PR #1: a3e245e; R18 foi confirmado pelo job launch-contracts do workflow 41808890786, portanto 5/20 concluídos e **15 pendentes** (R04–R17 e R20).
- Meta inicial: ceil(15 × 0,30) = **5**. Não houve conclusão de cinco itens; manter a meta aberta.
- R16 parcial: overlay_adapted.py agora calcula SHA-256 de todos os oito DOLs **após** aplicar adaptações/fallbacks, e verify_bundle.py exige correspondência exata com manifest.
- Três regressões sintéticas em test_final_integrity.py: fallback íntegro, corrupção de DOL e hash ausente. A CI da PR deve confirmar a execução; nenhum teste físico Wii executado.
- Commits de implementação: 8277868, 539285f, 9c477e9, f87a6bf. R16 não é concluído: falta pacote integral reproduzível/licenciamento e teste dos módulos no console.
- Próximo: verificar CI deste HEAD e corrigir qualquer falha; investigar os erros upstream WiiMC/WiiSXRX e testar hardware HID real.

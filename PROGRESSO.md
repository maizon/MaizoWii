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

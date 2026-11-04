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

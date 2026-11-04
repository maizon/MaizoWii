# Compatibilidade USB HID universal — contrato de aceite

O alvo é a **classe de gamepads USB HID**, não um VID/PID específico. Microntek 0079:0006 e Twin 0810:0001 são fixtures de teste, não exceções codificadas. A referência comportamental é o Nintendont; sua implementação IOS/HID não pode ser transplantada automaticamente para qualquer homebrew.

## Requisitos verificáveis

1. Enumerar dispositivos HID, interfaces, endpoints e descritores; registrar VID/PID e interface, sem presumir que um VID/PID representa sempre um gamepad.
2. Interpretar report descriptors HID e report IDs quando suportados; perfis compatíveis com controller.ini do Nintendont como fallback explícito. Não declarar suporte automático a protocolos proprietários.
3. Tratar eixos, D-pad/hat, botões, deadzone, gatilhos, múltiplos report IDs, MultiIn, desconexão e reconexão sem bloquear o loop de interface.
4. Exibir diagnóstico: estado, VID/PID, interface, report ID, perfil ativo, erro de abertura/leitura e limites do IOS.
5. Frontend, WiiMC, WiiSXRX, SNES9x GX, FCE Ultra GX, VBA GX, USB Loader GX e instalador devem aceitar navegação HID no Wii real. Nintendont mantém sua implementação nativa, sem regressões.
6. Cada módulo exige: build DOL adaptado sem fallback, teste automatizado de parser/mapeamento com fixtures, teste de botões e reconexão no Wii, registro de evidência. CI verde com fallback **não** atende ao aceite HID.
7. O instalador nunca grava NAND sem confirmação e mecanismo documentado de recuperação. WAD requer validação independente do forwarder HBC DOL.

## Matriz de teste no console

Para cada módulo: registrar build SHA, IOS ativo, porta USB, controle, VID/PID, descriptor/report ID, entrada de todos os botões, D-pad, analógicos, unplug/replug, retorno ao frontend e resultado PASS/FAIL. Testar ao menos um gamepad HID padrão além dos dois dispositivos de referência. Se não houver Wii físico, marcar NOT TESTED.

## Estado observado em 04/11/2026

Workflow 41816282992: compilação de WiiSXRX e outros adapters com estágio DOL; WiiMC executou etapa de fallback (Stage patched DOL: skipped; Report fallback: success). Portanto o sucesso do workflow não prova WiiMC adaptado, compatibilidade HID física nem aceite universal. Nenhum item de compatibilidade física deve ser encerrado com base apenas nessa execução.


## Nova implementação de base (candidato de integração)
- `integrations/maizo_report.h`: parser C portátil de descritor
  gamepad/joystick padrão, com report ID, deslocamentos em bits, campos
  de botões, eixos e hat; limites de leitura e rejeição de mouse.
- `integrations/maizo_hid.inc`: prioriza perfis Nintendont/local quando
  presentes; sem perfil, consulta descritor USB HID e só aceita Gamepad
  convencional. O código passou a ser compartilhado pelo frontend e adapters.
- O instalador YAWM possui patch de navegação HID mantendo confirmação de
  WAD. A variante só poderá ser marcada adaptada após build sem fallback.
- Escopo NÃO fechado: periféricos de múltiplas interfaces, vários receptores
  físicos simultâneos, arrays complexos, layouts proprietários e retorno
  seguro após trocas de IOS exigem testes extras.

## Roteamento MultiIn 0–4
O roteador `integrations/maizo_multi.h` implementa exatamente os formatos
documentados no Nintendont: modo 1 filtra relatório por primeiro byte; modo
2 usa o primeiro byte como porta; modo 3 carrega N pads no mesmo relatório,
onde `MultiInValue` é o tamanho de cada bloco em BYTES (não número de
controles); modo 4 usa pacotes separados indexados de 1 até
`MultiInValue`. São validados número de portas, tamanho do buffer e
limites individuais. Isso não significa que receptores com protocolo
proprietário ou XInput passem a ser reconhecidos sem driver.

**Compatibilidade de splitters — correção posterior:** em MultiIn=2, valor mínimo 0 continua aceitando pacotes indexados de 1 a 4; em MultiIn=3, `MultiInValue` é o stride em bytes, blocos finais incompletos são ignorados e o recebimento nunca ultrapassa quatro portas. São limites de interpretação, não prova de compatibilidade com um receptor físico.

**Report IDs USB HID:** o interpretador de descritores aceita IDs de 1 a 255, conforme o campo de 8 bits do padrão HID, em vez de bloquear incorretamente valores acima de 15. O teste C exercita ID 0xFE e rejeita ID 0xFF quando ele não está no descritor. Isso melhora cobertura de dispositivos genéricos, mas não emula XInput.

**USB vendor-class fallback:** o driver consulta primeiro a interface USB HID e, em segunda tentativa, somente interfaces vendor-class de dispositivos com `controller.ini` coincidente. Isso pode ajudar adaptadores que expõem relatórios HID por endpoints de interrupt mesmo com classe de interface 0xFF, mas não emula comandos de inicialização XInput/proprietários. Largura wMaxPacketSize é validada antes da conversão para u8.

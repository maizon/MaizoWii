# MaizoNet SMB1 Direct — protótipo de GameCube (NÃO validado no Wii real)

**Estado:** o cliente SMB1 e um backend de leitura de ISO sob demanda para o kernel
ARM do Nintendont foram escritos e a compilação automatizada está sendo exercitada.
Não há confirmação de jogo iniciando em Wii físico. Nunca substitua o Nintendont normal
com esse binário experimental.

## Objetivo e isolamento

- Somente Nintendo Wii, roteador e servidor Samba SMB1 existente na rede local.
- Sem Raspberry Pi e sem servidor MaizoNet próprio.
- Nenhuma ISO integral é transferida: o frontend baixa **somente os primeiros
  128 KiB** da imagem para um stub local; o Nintendont modificado tenta ler
  setores restantes diretamente do SMB1 durante o jogo.
- O stub é armazenado em `sd:/maizo/maizonet/game.iso` ou equivalente USB.
  É um arquivo **INCOMPLETO**; NÃO inicie no Nintendont oficial.
- O Nintendont oficial e os demais módulos não devem ser modificados.
- O WiiMC e sua configuração de vídeo continuam intactos.
- Um SD ou pendrive FAT é necessário para executar os DOLs, config e armazenar o
  pequeno stub (não para guardar a ISO toda).

## Configuração

Crie `sd:/maizo/network.ini` ou `usb:/maizo/network.ini`:

```ini
host=192.168.1.100
port=1445
share=PS2
path=GameCube/Meu Jogo/game.iso
```

O servidor padrão é uma LAN isolada, convidado sem senha.
Se sua instância Samba usa porta TCP 445, altere `port=445`.
O diretório de `path` é **relativo à raiz** do compartilhamento.
Não encaminhe a porta SMB1 para a internet.

## Instalação experimental

1. Faça backup dos seus arquivos e configurações.
2. Instale o frontend compilado pela branch de testes.
3. Instale o Nintendont modificado em
   `sd:/apps/nintendont-maizonet/boot.dol`, ou em
   `usb:/maizo/modules/nintendont-maizonet/boot.dol`.
   **Não sobrescreva** `apps/Nintendont/boot.dol`.
4. Na tela inicial, pressione START → Configurações gerais.
5. Escolha **MAIZONET: TESTAR SMB1 CONVIDADO**; com `path` configurado,
   a leitura de 32 bytes deverá retornar o ID do jogo.
6. Escolha **MAIZONET: INICIAR GAMECUBE SMB1**. Somente com Nintendont
   MaizoNet instalado o frontend permitirá avançar.
7. Caso falhe, registre exatamente a tela/etapa; não suponha que teste de
   conexão garante um jogo jogável.

## Limitações atuais

- Ainda não há descoberta automática de todas as ISOs SMB na biblioteca.
  Apenas um `path` é configurado por vez; extensões são passadas como texto.
- Apenas SMB1 com acesso de convidado, sem senha/assinatura, em LAN.
  Servidores com NTLMSSP obrigatório podem rejeitar a conexão.
- CISO e troca automática de discos pela rede ainda não têm compatibilidade
  comprovada.
- O kernel agora tenta **uma única reconexão** por leitura com erro, voltando
  ao deslocamento original. Isso não garante recuperação de qualquer queda.
- ISOSeek remoto não tenta deslocar um descritor FAT inexistente; o caminho
  local permanece inalterado.
- O marcador exclusivo da sessão SMB vai via NIN_CFG/argv; o frontend não
  grava essa configuração remota em `nincfg.bin` global para proteger o Nintendont padrão.
- Os testes de protocolo e build de CI não substituem Wii físico.
- O código de rede no kernel ARM deve ser testado em Wii físico antes de uso
  prolongado. As estratégias de timeout podem variar entre versões de IOS.
- Meta de 3,2 MB/s **não medida**. Nenhuma promessa de jogo sem travamentos.
- Nintendont é software GPL-2.0; alterações são compiladas em artefato
  separado. A distribuição final deve manter os créditos/licenças.

## Desligamento do Wii e diagnóstico experimental

O Nintendont anterior executava `Shutdown()` quando uma leitura remota
falhava ou caía no fallback FST. A versão instrumentada deixa o Wii ligado,
interrompe o lançamento remoto sem tentar usar os dados incompletos, e grava
o erro **quando a unidade FAT estiver gravável** em
`usb:/maizo/maizonet/diagnostic.log` (ou a unidade de inicialização SD).

A linha `MZNET_FRONTEND` prova somente a leitura dos 128 KiB; a linha
`MZNET_FATAL` distingue `ISO_OPEN` e `ISO_READ`, código da fase, último
NTSTATUS, offset e tamanho da leitura. Códigos: 101 marcador; 110-117 rede IOS;
120 negociação SMB; 121 abertura SMB; 130-131 leitura. O código 199 representa
falha não classificada. Não são logs completos de todos os eventos ou falhas
imprevisíveis: podem faltar em travamentos anteriores à gravação, corrupção
de FAT, resets ou desligamentos externos. Não altere seu Samba do OPL.

Após teste que falhar, desligue o Wii manualmente se necessário, conecte o
pendrive ao Android/PC e envie `maizo/maizonet/diagnostic.log`. Em testes
iniciais desative a emulação de Memory Card.

## Histórico circular de diagnóstico

O kernel experimental mantém até **64 eventos recentes na RAM**, sem escrita
FAT durante cada leitura da ISO. Em falha terminal, grava eventos de socket IOS,
SMB1, abertura, leitura e reconexão, com temporizador, retorno e deslocamento,
no `maizo/maizonet/diagnostic.log` do dispositivo FAT selecionado.
A captura é de melhor esforço, não de 100% dos eventos; falhas anteriores à
montagem FAT e falhas abruptas podem não gerar arquivo.

## Diagnóstico físico 12/11/2026 e correção KD opcional

Log real do Wii (maizonet em ISO do XIII):
```
MZNET_FATAL step=ISO_OPEN code=112 nt=00000000 offset=00000000:00000000 bytes=0
MZNET_TRACE tick=049A76AB phase=110 result=0 offset=00000000:00000000 size=1445
MZNET_TRACE tick=049A775E phase=111 result=2 offset=00000000:00000000 size=0
```
O estágio **112** é a falha ao abrir `/dev/net/kd/request` (WiiConnect24),
não a abertura da ISO e nem um defeito demonstrado no Samba. O dispositivo
`/dev/net/ip/top` foi aberto com sucesso (fd 2), portanto o kernel agora
continua a inicialização SO quando KD estiver indisponível, deixando
observável o resultado real de `SO_STARTUP`, `SO_HOSTID`, `SO_SOCKET`
e conexão TCP. O retorno da função `SO_HOSTID` é um endereço IPv4 de
32 bits e não deve ser testado como inteiro positivo com sinal; o
endereço 192.168.0.8 seria incorretamente rejeitado pela versão anterior.

**A correção permite avançar além da fase 112, mas ainda não valida que a
reimplementação SMB1 no kernel execute um jogo real.** Capture novos logs
para diferenciar falta de IP (115), falha de socket (116), conexão (117),
negociação SMB (120), abertura (121) ou leitura de setores (130/131).

## Teste de console de 12/11 — falha 115 e investigação NCD

Novo log real da build 9a0cc2e1: `ISO_OPEN code=115`, fase 112 abre KD
(fd=3), fase 113 retorna -33 na inicialização NWC24, fase 114 retorna
0 no `SO_STARTUP`, fase 115 retorna **0 em 25 tentativas** de IP.
O erro ocorreu **antes** de qualquer abertura SMB1 do kernel.

A implementação anterior não consultava `/dev/net/ncd/manage` antes
dos ioctls KD e SO. A revisão adiciona consulta read-only de link via
NCD (fases 108/109) antes do restante do fluxo, semelhante à ordem de
inicialização de `net_init` no libogc. Se ainda ficar sem IPv4, registra
o retorno e parâmetros `SO_GETINTERFACEOPT` para IP/máscara (118/119).

Não altera NAND, a conexão definida no menu do Wii, Samba, nem os
jogos locais. Ainda é um experimento que exige outro teste físico;
consultar NCD não garante por si só que o IOS obtenha um endereço IP.

### Inicialização PPC antes do primeiro acesso DI

A investigação também mostrou que o Nintendont chama
`CheckForMultiGameAndRegion` antes da preparação posterior dos jogos, e
que a implementação de rede no frontend não necessariamente sobrevive ao
handoff de IOS. O loader experimental agora invoca `net_init()` do **libogc
no PowerPC**, antes de sua primeira leitura pelo kernel, exclusivamente
quando detecta `MZSMB1|` em CheatPath. O código oficial `net_init()` segue
NCD → KD → SO → obtenção de IP, e o código de retorno é gravado no log como:

`MZNET_LOADER_NET_INIT result=<código>`.

Isso NÃO modifica a NAND ou a configuração de conexão do Wii. Ainda é
uma tentativa de inicialização: em caso de falha na interface/IOS,
a ISO continuará indisponível e não se deve supor sucesso.

## Correção funcional posterior: inicialização NCD antes da leitura DI

A falha de IP no kernel ocorre durante ISO_OPEN, **antes** de a rotina
padrão do Nintendont chegar à configuração de perfil de rede ao final
do loader. O Nintendont original ativa perfis selecionados nessa etapa
posterior exclusivamente quando a opção de emulação BBA está ativada.
O frontend MaizoWii usa `NetworkProfile=0` e não habilita BBA para o XIII.

A nova rotina experimental ativa apenas em MaizoNet um perfil NCD
**já configurado no próprio Wii**, antes de `CheckForMultiGameAndRegion`,
seguindo os mesmos ioctls 3/4 e bits de perfil utilizados no loader
upstream. Prioriza o perfil ativo; caso não exista, um perfil já
habilitado. Se não houver perfil configurado, não inventa um. Não liga
o BBA dentro do jogo e não altera os caminhos locais de Nintendont.

Além disso, `SO_HOSTID=0` deixa de bloquear automaticamente o SMB:
após uma sondagem breve, o kernel tenta a operação real
`SO_SOCKET / SO_CONNECT`. Isso pode permitir conexão quando o getter
de IP retorna zero apesar de a pilha de rede estar utilizável.

Compilação automática não equivale a sucesso de boot no Wii real.
A autenticação SMB, leitura aleatória de setores, boot de apploader,
vídeo e controles ainda precisam de teste físico. Sem gravar a ISO
completa no USB.

## Handoff de rede permanente — não suspender WiiConnect24 no MaizoNet

No upstream Nintendont, já depois da inicialização da rede e quase no
salto para o jogo, o loader chama `ExecSuspendScheduler` no serviço
`/dev/net/kd/request` para parar a agenda do WiiConnect24. Essa decisão
é correta para jogos carregados de disco/USB, mas pode interromper o
serviço que nossa ISO de GameCube **precisa manter funcionando durante
todo o jogo**. A nova integração experimental preserva esse serviço
somente quando `CheatPath` contém o marcador `MZSMB1|`. Jogos locais
continuam com o comportamento original.

O kernel agora verifica, ao abrir `/dev/net/ip/top`, se já existe um
IP atribuído pelo loader e, nesse caso, evita iniciar novamente KD/SO.
Se ainda estiver sem IP, realiza a rotina existente de inicialização
e tenta TCP `SO_CONNECT` mesmo que `SO_GETHOSTID` retorne zero.

Essas são alterações de fluxo funcional, não apenas logs.
O Wii real ainda precisa validar GameCube rodando e leituras aleatórias
pelo SMB; não há garantia de funcionamento apenas pelo CI.

## Nova correção: eliminar bloqueio indefinido no SO_CONNECT/SMB1

Depois de `MZNET_LOADER_NET_INIT result=0` sem erro de kernel, a hipótese
mais crítica é a espera dentro de uma chamada IOS SO de rede: o loader
conclui seu trabalho, mas o kernel pode nunca chegar à rotina de erro,
deixando a TV preta permanentemente.

O kernel experimental agora utiliza o protocolo de socket não bloqueante
do libogc: `SO_FCNTL(F_GETFL/F_SETFL,IOS_O_NONBLOCK)` e `SO_POLL`,
com janela de **3500 ms** por operação de envio/recebimento/conexão.
Respostas SMB continuam processadas incrementalmente e erros acionam a
única reconexão existente. Sem alterações ao servidor Android.

Esse ajuste não prova que o jogo funciona; elimina uma causa plausível de
travamento indefinido e cria um desfecho controlado quando não há rede.

## Kernel ARM exigido antes da execução: leitura real além de 128 KiB

O frontend só transferia o cabeçalho local, e o loader registrava
`net_init=0` sem verificar que o **kernel** conseguia recuperar dados.
O binário experimental agora testa `/dev/mydi` e lê um setor em
`offset=0x20000` (além do stub local de 128 KiB), enquanto a interface
PPC ainda funciona, **antes de iniciar o jogo**. A leitura depende do
cliente SMB1 do kernel; em caso de erro a execução é bloqueada e o Wii
mostra mensagem de falha. Nunca inicia o stub incompleto como jogo local.

Para suportar essa validação, `maizonet_nin_fatal` não bloqueia
permanentemente a thread DI. Marca falha, registra o erro e devolve
`-70` via IPC, inclusive nos erros de leitura de setores. Isso evita
silenciar um erro de transporte e evita um loop preto infinito.

**Aprovado no CI não valida jogo em hardware**. A preflight é necessária,
mas áudio, controles e o jogo inteiro ainda dependem de validação real.

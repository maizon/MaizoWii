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

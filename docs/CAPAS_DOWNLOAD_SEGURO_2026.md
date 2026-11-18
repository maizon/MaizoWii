# Capas nativas no MaizoWii — estilo USB Loader GX / MaizoNintendont

Este fluxo foi inspirado na busca por **Game ID de 6 caracteres** e na
preferencia por 2D/3D/capa completa com fallback de idioma/regiao do
USB Loader GX, alem da organizacao GameTDB por jogo do MaizoNintendont.
Nao copia binarios de nenhum outro frontend nem modifica os jogos.

## Usar no Wii

1. Abrir **Configuracoes > Gerenciar capas > Central de capas**.
2. Marcar os sistemas; optar por **todas as capas ausentes** ou **atualizar
   existentes**. Escolher **frontal**, **completa/HQ** ou **3D**.
3. Selecionar GameTDB, Libretro ou AUTO; a fonte por ID funciona melhor
   em Wii e GameCube. A fonte por titulo cobre PS1, SNES, NES, GBA e GB.
4. A opcao **Rede / certificado SSL** oferece:
   - `AUTO (HTTPS + GX)`: tenta HTTPS validado; **somente se o certificado
     falhar** (erro curl 60), tenta HTTP nao criptografado em URLs de PNG
     estritamente limitadas ao GameTDB. A tela informa `HTTP GX` e quantas
     imagens foram baixadas sem TLS.
   - `HTTPS CERTIFICADO`: nao faz downgrade; informa erro 60 e interrompe,
     evitando repetir a mesma falha em centenas de capas.
   - `USB LOADER GX HTTP`: para imagens GameTDB usa HTTP legado de inicio.
     Libretro continua usando HTTPS validado.
5. Selecionar **Iniciar download**. A tela mostra progresso, contagens,
   falhas de certificado e origem da transferencia. **B cancela** entre
   iteracoes, mantendo capas ja gravadas.

As fontes online podem mudar; o endpoint HTTP pode redirecionar para HTTPS.
Nesse caso o MaizoWii **nao ignora** a verificacao TLS, interrompe e mostra
a necessidade de acertar o relogio/CA.

## Se aparecer "SSL peer certificate or SSH remote key was not OK"

A mensagem e o texto generico do **curl erro 60**, indicando falha de
validacao de **certificado SSL/TLS**: nao significa que um SSH do usuario
esteja com chave errada.

- Confira a **data e hora do Wii** nas configuracoes do sistema.
- Compilacoes novas tentam incorporar certificados raiz Mozilla atualizados
  pelo ambiente verificado do GitHub Actions. O certificado antigo permanece
  como fallback se nao houver acesso ao servidor confiavel durante a CI.
- Se necessario, baixe o PEM oficial `https://curl.se/ca/cacert.pem` em
  computador/celular, e copie para `SD:/maizo/cacert.pem` ou
  `USB:/maizo/cacert.pem`. O MaizoWii le este arquivo sem desabilitar a
  verificacao do nome do servidor e da cadeia do certificado.
- Para diagnosticar, selecione `HTTPS CERTIFICADO` e leia o codigo na
  Central de Capas. So use o modo legado se aceitar o risco de transferir
  **imagens sem autenticacao criptografica**. O app nao usa HTTP para
  DOL, WAD, BIOS, atualizacoes nem configuracoes privadas.
- Ausencia de certificado valido, imagens PNG maiores que 3 MB,
  resolucao DNS, rede indisponivel ou captchas do provedor nao podem ser
  garantidos por testes de compilacao.

## Armazenamento e compatibilidade

`usb:/maizo/covers/{wii,gc,ps1,snes,nes,gba,gb}/` e equivalente em SD.
Capas do Wii/GC por ID6; classicos por titulo normalizado. Imagem original
convertida para cache `MZG1`, com dimensoes internas por sistema e sem
esticar as proporcoes. O frontend nao grava sobre ROMs. Os .gxt existentes
sao preservados salvo opcao expressa de atualizar, com arquivos .part
e renomeacao atomica em FAT.

A verificacao real exige Wii conectado, internet, FAT, teste de GameTDB,
Libretro e gravacao no cartao ou pendrive. **CI verde nao prova rede no Wii**.

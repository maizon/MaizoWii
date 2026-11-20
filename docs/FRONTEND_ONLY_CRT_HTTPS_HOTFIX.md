# Hotfix FRONTEND ONLY — video PAL / NTSC e HTTPS de capas

## Selecao do modo de TV
No menu de padroes de video, ESQUERDA/DIREITA selecionam a opcao
atualmente visivel. A confirma a opcao atualmente exibida, SEM
avancar uma casa. Isso corrige o comportamento anterior em que o
comando de confirmacao A tambem equivalia a DIREITA.

No modo de video global, ha 10 segundos para confirmar com A apos
ESQUERDA/DIREITA. B ou navegar para outra linha cancela a alteracao
e restaura o sinal anterior. Os perfis individuais de nucleos
sao salvos em seus valores proprios. Evite PAL-M em HDMI/componente
quando sua TV nao suporta esse sistema.

## CURL 60: mbedTLS certificate expired / HTTP 0
Indica falha na negociacao de HTTPS; HTTP 0 significa que ainda
nao houve resposta HTTP, NAO significa "capa ausente".
Uma CA atualizada nao conserta um certificado leaf vencido nem
um relogio do Wii incorreto. A verificacao TLS permanece LIGADA.
Confira DATA/HORA no Wii, sistema da TV e hora real, e se necessario
atualize `usb:/maizo/cacert.pem` a partir de https://curl.se/ca/cacert.pem.
Se GameTDB tiver TLS invalido, o modo AUTO pula o host e tenta
Libretro CDN e GitHub com HTTPS verificado. HTTP inseguro continua
disponivel SOMENTE no modo USB LOADER GX HTTP, por opcao explicita.
O modo AUTO nunca reduz seguranca de HTTPS para HTTP.

## CURL 28: connection timed out after 8007ms
O limite de conexao passou de 8s para 15s, com timeout
total de 32s para liberar conexoes lentas. Depois de um
timeout sem resposta HTTP, a fonte e marcada temporariamente
como indisponivel durante o lote para nao travar o menu
em dezenas de conexoes iguais. O downloader tenta outras
fontes, sem afirmar que a capa nao existe.
Se todas as fontes HTTPS falharem, orienta a verificar
data/hora e internet, em vez de declarar que os jogos
estao sem capa. Cabo/roteador/DNS podem causar o erro 28.

## Compilacao
Este commit usa o marcador `[frontend-only]` na mensagem.
A pipeline principal compila e publica apenas `MAIZOWII-FRONTEND`.
Nao recompila WiiMC, Nintendont, emuladores, instalador ou
ALL-IN-ONE neste push. Instalacao: substituir somente
`sd:/apps/maizo/boot.dol` ou `usb:/apps/maizo/boot.dol`
pelo executavel novo. O frontend novo nao muda o WiiMC DOL
ja instalado. CI valida compilacao, NAO sinal analogico ou rede Wii.

# Capas do MaizoWii via Android / PC (candidato 1.0)

**Atenção:** o Wii ainda **não baixa capas da internet por conta própria**.
O sincronizador abaixo roda no Android/Termux ou computador e grava as capas
diretamente no pendrive/SD FAT32. Não usa Wiimote nem modifica os núcleos.

## Um único comando para baixar e converter

Aponte o caminho para **a raiz do pendrive ou SD**, onde existem
`games/`, `wbfs/`, `roms/` etc.:

```sh
python3 tools/cover_sync.py /caminho/para/pendrive --region BR --limit 500 --convert
```

Preparação em Termux: instale Python e Pillow (no Termux normalmente via
`pkg install python python-pillow`), obtenha o repositório MaizoWii,
conceda acesso à mídia e substitua o caminho pelo volume realmente montado.
Se o Android não conceder escrita no OTG, escolha outro caminho autorizado
e copie `maizo/covers/` posteriormente; não forçar /system nem alterar cIOS.

A opção `--convert` salva o formato binário `.gxt` de 128×176, mantendo
a proporção da imagem original. O Wii não precisa decodificar PNG.
Arquivos `.gxt` existentes são preservados e apenas capas ausentes são
baixadas quando possível.

Pastas de destino:

```
maizo/covers/
  wii/       ID6.gxt
  gc/        ID6.gxt
  ps1/       NOME-DO-JOGO.gxt
  snes/      NOME-DO-JOGO.gxt
  nes/       NOME-DO-JOGO.gxt
  gba/       NOME-DO-JOGO.gxt
  gb/        NOME-DO-JOGO.gxt
```

O sincronizador consulta GameTDB para Wii/GC e miniaturas Libretro para
sistemas retro. Download depende do arquivo estar disponível no catálogo,
conectividade e nomes/IDs reconhecidos. Ele **não garante todas as capas**.

No MaizoWii, após copiar capas com o frontend aberto, use **Z para
reindexar** (ou reinicie). O cache é invalidado quando se reindexa.

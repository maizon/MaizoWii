# MaizoWii — frontend sem dependência do GX para vídeo 2D

## Causa investigada
Versão anterior: imagem rosa/preta no Wii físico, Dolphin renderizava.
Após mudança na fila GX e no estado inicial da VI: tela preta em ambos.
A compilação em CI passou, mas isso não substituiu teste visual.

## Alteração
O frontend não utiliza mais GX_Init, GX_DrawDone nem GX_CopyDisp para
desenhar sua interface. Passou a usar um framebuffer YUYV 4:2:2 da VI,
gravado diretamente em memória não-cacheada pela CPU, com dupla memória
de vídeo. A mesma UI de Home, Biblioteca, Busca, Ajustes, Perfis e HID
continua ativa. As capas MZG1 (.gxt), armazenadas em blocos RGBA8 4x4,
são decodificadas e apresentadas no framebuffer sem usar a GPU.

Emuladores externos continuam com seus próprios renderizadores e o
carregamento DOL/IOS original foi mantido, sem alterar módulos.

## Testar no Dolphin e no Wii real
1. Faça backup do arquivo apps/maizo/boot.dol anterior.
2. Baixe somente o artifact MAIZOWII-FRONTEND da nova execução do CI.
3. Copie o boot.dol para apps/maizo/boot.dol no USB ou no SD.
4. Inicie e confira as mensagens de inicialização e a Home.
5. Navegue Home, Biblioteca, Busca e Ajustes, sem módulos instalados.
6. Teste capas, temas, GameCube, Wii, PS1, SNES e mídias.
7. No Wii real, teste em PAL-M com cabo composto ou componente.
8. Se congelar depois de abrir a Home, teste com receptor USB
   desconectado ou com um arquivo vazio em
   sd:/maizo/disable-hid.flag ou usb:/maizo/disable-hid.flag.

## Limitações
A renderização por CPU pode ser mais lenta que a por GX, especialmente
com várias capas grandes. A qualidade mantém cores e gradientes, mas
utiliza amostragem simples (nearest neighbor) para as capas.
O frontend não tem áudio de inicialização; silêncio não indica travamento.
Nenhuma garantia de 99,99% é possível antes do teste no hardware real.

## Regressões testáveis
Os testes do frontend verificam a presença da saída direta YUYV,
ordem de inicialização, ausência de GX na Home, alcance dos offsets
das capas, detecção HID tardia e código dos contratos de lançamentos.

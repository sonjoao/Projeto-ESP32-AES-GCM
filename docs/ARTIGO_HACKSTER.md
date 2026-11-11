# Artigo Hackster.io — rascunho

## Título
Duas ESP32, uma mensagem protegida: AES-GCM via ESP-NOW

## Resumo
Este projeto demonstra comunicação sem roteador entre duas placas ESP32. O remetente cifra uma mensagem curta com AES-256-GCM e envia o pacote por ESP-NOW. O receptor valida a etiqueta de autenticação antes de exibir o texto no Monitor Serial. Uma mensagem cuja autenticação falha é descartada.

## Problema
Dispositivos IoT podem transmitir comandos ou telemetria que ficam expostos à leitura e alteração. O protótipo mostra como integrar uma primitiva criptográfica autenticada ao fluxo de comunicação entre microcontroladores.

## Materiais
- 2 placas ESP32 compatíveis com o ambiente esp32dev.
- 2 cabos USB de dados.
- 1 computador com VS Code e PlatformIO.
- Nenhum periférico externo é necessário na versão Serial.

## Montagem
Não há fiação entre GPIOs: conecte cada placa ao computador via USB. O remetente e o receptor comunicam-se pelo rádio ESP-NOW no canal 1. Confirmar os modelos exatos das placas antes de publicar fotos e instruções finais.

## Firmware e execução
O repositório oferece os ambientes esp32_sender e esp32_receiver. Configure a mesma chave AES-256 nas duas placas por meio de include/secrets.h, que não deve ser publicada. Grave cada ambiente em uma placa e use o Monitor Serial a 115200 baud. Ao digitar uma mensagem no remetente, o receptor só apresenta o texto se a etiqueta GCM for válida.

## Segurança e limitações
A versão é didática. Ainda não implementa provisionamento de chaves, proteção contra replay, rotação de chave nem confirmação de entrega. Adicione fotos da montagem e um vídeo real de bancada antes de publicar o artigo.

## Evidências a inserir antes da publicação
- Fotos das placas e dos cabos USB.
- Captura do Monitor Serial do envio e recebimento.
- Vídeo curto com o fluxo funcionando.
- Modelos exatos das placas e revisão do pinout.

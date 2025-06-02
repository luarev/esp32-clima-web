# ESP32 Clima Web

Projeto de monitoramento de temperatura e umidade utilizando ESP32 com sensor DHT11.  
Interface web com autenticação, WebSocket em tempo real e armazenamento de arquivos HTML via SPIFFS.

---

## Visão Geral

Este projeto tem como objetivo monitorar a temperatura e umidade do ambiente utilizando o sensor DHT11 acoplado a um ESP32. A interface web, hospedada no próprio ESP32 via SPIFFS, permite visualizar os dados em tempo real e requer autenticação para acesso ao painel de controle.

---

## Tecnologias Utilizadas

- ESP32 Dev Module
- Sensor DHT11
- ESPAsyncWebServer
- AsyncTCP
- SPIFFS
- PlatformIO (VSCode)

---

## Montagem do Circuito

- DHT11 VCC conectado ao pino VIN do ESP32
- DHT11 GND conectado ao GND do ESP32
- DHT11 DATA conectado ao GPIO 15 (D15)

---

## Instruções de Instalação

1. Instale o [Visual Studio Code](https://code.visualstudio.com/) com a extensão [PlatformIO IDE](https://platformio.org/install).

2. Clone este repositório:

   ```bash
   git clone https://github.com/luarev/esp32-clima-web.git
   cd esp32-clima-web
   ```

3. Conecte o ESP32 via cabo USB ao computador.

4. Compile e envie os arquivos da pasta `data/` para o sistema de arquivos do ESP32 (SPIFFS):

   ```bash
   pio run --target uploadfs
   ```

5. Em seguida, envie o firmware para o ESP32:

   ```bash
   pio run --target upload
   ```

6. Inicie o monitor serial para obter o endereço IP gerado pelo ESP32:

   ```bash
   pio device monitor
   ```

7. Acesse o IP informado pelo monitor serial no navegador para abrir a interface web.

---

## Funcionalidades Extras

- Registro de todas as sessões de login no arquivo `sessao.txt`, com data e hora real (via NTP).
- Armazenamento contínuo dos dados de temperatura e umidade no arquivo `clima.csv`, com marcação de data/hora.
- Interface web com botões para exportar os dados em formato `.csv` e `.txt`.
- Todos os dados são armazenados localmente no sistema SPIFFS do ESP32, permanecendo mesmo após reinicializações.

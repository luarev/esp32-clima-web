#include <WiFi.h>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

const char* ssid = "Galaxy_S23";
const char* password = "luana123159";

#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float temperatura = 0.0;
float umidade = 0.0;

int clientesConectados = 0;
String usuarioLogado = "";

// Função para validar login
bool validarLogin(const String& user, const String& pass) {
  return (user == "admin1" && pass == "senha1") ||
         (user == "admin2" && pass == "senha2") ||
         (user == "admin3" && pass == "senha3");
}

void salvarSessao(String usuario) {
  File f = SPIFFS.open("/sessao.txt", FILE_WRITE);
  if (f) {
    f.print(usuario);
    f.close();
  }
}

String carregarSessao() {
  File f = SPIFFS.open("/sessao.txt", FILE_READ);
  if (f) {
    String usuario = f.readString();
    f.close();
    return usuario;
  }
  return "";
}

void limparSessao() {
  SPIFFS.remove("/sessao.txt");
  usuarioLogado = "";
}

void salvarDados(float temperatura, float umidade) {
  File arquivo = SPIFFS.open("/clima.csv", FILE_APPEND);
  if (!arquivo) return;
  unsigned long timestamp = millis();
  arquivo.printf("%lu,%.2f,%.2f\n", timestamp, temperatura, umidade);
  arquivo.close();
}

void notifyClients() {
  String json = "{\"temperatura\":" + String(temperatura) + ",\"umidade\":" + String(umidade) + "}";
  ws.textAll(json);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return;
  }

  usuarioLogado = carregarSessao();

  WiFi.begin(ssid, password);
  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  Serial.print("Conectado com IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Servidor ativo na porta 80");

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      clientesConectados++;
      Serial.printf("Novo cliente conectado (ID %u). Total: %d\n", client->id(), clientesConectados);
    } else if (type == WS_EVT_DISCONNECT) {
      clientesConectados--;
      Serial.printf("Cliente desconectado (ID %u). Total: %d\n", client->id(), clientesConectados);
    }
  });
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (usuarioLogado == "") {
      String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Login</title></head>
<body>
  <h2>Login</h2>
  <form action='/login' method='POST'>
    Usuário: <input type='text' name='username'><br>
    Senha: <input type='password' name='password'><br>
    <input type='submit' value='Entrar'>
  </form>
</body>
</html>
)rawliteral";
      request->send(200, "text/html", html);
    } else {
      request->redirect("/painel");
    }
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String user = request->getParam("username", true)->value();
      String pass = request->getParam("password", true)->value();
      if (validarLogin(user, pass)) {
        usuarioLogado = user;
        salvarSessao(user);
        request->redirect("/painel");
      } else {
        request->send(200, "text/plain", "Login inválido. Volte e tente novamente.");
      }
    }
  });

  server.on("/painel", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (usuarioLogado == "") {
      request->redirect("/");
      return;
    }

    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset='UTF-8'><title>Painel</title></head>
<body>
  <h1>Bem-vindo ao painel</h1>
  <p>Usuário: %USER%</p>
  <p>Temperatura: <span id='temp'>--</span> °C</p>
  <p>Umidade: <span id='umid'>--</span> %</p>
  <a href='/logout'>Logout</a>
  <script>
    const socket = new WebSocket("ws://" + window.location.hostname + "/ws");
    socket.onmessage = function(event) {
      const dados = JSON.parse(event.data);
      document.getElementById("temp").textContent = dados.temperatura;
      document.getElementById("umid").textContent = dados.umidade;
    };
  </script>
</body>
</html>
)rawliteral";
    html.replace("%USER%", usuarioLogado);
    request->send(200, "text/html", html);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    limparSessao();
    request->redirect("/");
  });

  server.begin();
}

void loop() {
  umidade = dht.readHumidity();
  temperatura = dht.readTemperature();

  if (!isnan(umidade) && !isnan(temperatura)) {
    notifyClients();
    salvarDados(temperatura, umidade);
  }
  delay(2000);
}

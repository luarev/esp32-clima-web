#include <WiFi.h>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include "time.h"

const char* ssid = "Galaxy_S23";
const char* password = "galaxys23luana";

#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float temperatura = 0.0;
float umidade = 0.0;

int clientesConectados = 0;
String usuarioLogado = "";

// validação de login
bool validarLogin(const String& user, const String& pass) {
  return (user == "admin1" && pass == "senha1") ||
         (user == "admin2" && pass == "senha2") ||
         (user == "admin3" && pass == "senha3");
}

// salva cada sessão com usuário + data/hora no arquivo
void salvarSessao(String usuario) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);

    File f = SPIFFS.open("/sessao.txt", FILE_APPEND);
    if (f) {
      f.printf("Usuário: %s | Login em: %s\n", usuario.c_str(), buffer);
      f.close();
    }
  }
  usuarioLogado = usuario;
}

String carregarSessao() {
  return "";
}

void limparSessao() {
  usuarioLogado = "";
}

// horario via NTP
void configurarRelogio() {
  configTime(-10800, 0, "pool.ntp.org");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Erro ao obter hora via NTP");
  }
}

// salva dados de temperatura/umidade com horário formatado
void salvarDados(float temperatura, float umidade) {
  File arquivo = SPIFFS.open("/clima.csv", FILE_APPEND);
  if (!arquivo) return;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    arquivo.printf("%s,%.2f,%.2f\n", buffer, temperatura, umidade);
  }

  arquivo.close();
}

// envia dados webSocket
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

  WiFi.begin(ssid, password);
  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  Serial.print("Conectado com IP: ");
  Serial.println(WiFi.localIP());

  configurarRelogio();

  // webSocket
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

  // rotas
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (usuarioLogado == "") {
      request->send(SPIFFS, "/login.html", "text/html");
    } else {
      request->redirect("/painel");
    }
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String user = request->getParam("username", true)->value();
      String pass = request->getParam("password", true)->value();
      if (validarLogin(user, pass)) {
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

    File htmlFile = SPIFFS.open("/painel.html", "r");
    if (htmlFile) {
      String html = htmlFile.readString();
      html.replace("%USER%", usuarioLogado);
      request->send(200, "text/html", html);
      htmlFile.close();
    } else {
      request->send(500, "text/plain", "Erro ao carregar o painel.");
    }
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    limparSessao();
    request->redirect("/");
  });

  server.on("/baixar-clima", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/clima.csv")) {
      request->send(SPIFFS, "/clima.csv", "text/csv");
    } else {
      request->send(404, "text/plain", "Arquivo clima.csv não encontrado");
    }
  });

  server.on("/baixar-sessao", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/sessao.txt")) {
      request->send(SPIFFS, "/sessao.txt", "text/plain");
    } else {
      request->send(404, "text/plain", "Arquivo sessao.txt não encontrado");
    }
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
  delay(1000);
}

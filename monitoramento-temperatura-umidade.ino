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
<head>
  <meta charset='UTF-8'>
  <title>Login</title>
  <style>
    * {
      box-sizing: border-box;
    }
    body {
      margin: 0;
      height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #56ccf2, #2f80ed);
    }
    .login-box {
      background: white;
      border-radius: 15px;
      box-shadow: 0 8px 15px rgba(0, 0, 0, 0.2);
      padding: 40px;
      text-align: center;
      width: 320px;
      transition: transform 0.3s ease, box-shadow 0.3s ease;
    }
    .login-box:hover {
      transform: scale(1.02);
      box-shadow: 0 12px 20px rgba(0, 0, 0, 0.25);
    }
    h2 {
      margin-bottom: 20px;
    }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #ccc;
      border-radius: 5px;
      transition: border 0.3s ease;
    }
    input[type="text"]:focus, input[type="password"]:focus {
      border-color: #3498db;
      outline: none;
    }
    input[type="submit"] {
      width: 100%;
      padding: 10px;
      background-color: #3498db;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      transition: background-color 0.3s ease;
    }
    input[type="submit"]:hover {
      background-color: #2980b9;
    }
  </style>
</head>
<body>
  <div class='login-box'>
    <h2>Login</h2>
    <form action='/login' method='POST'>
      <input type='text' name='username' placeholder='Usuário' required><br>
      <input type='password' name='password' placeholder='Senha' required><br>
      <input type='submit' value='Entrar'>
    </form>
  </div>
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
<head>
  <meta charset='UTF-8'>
  <title>Painel de Monitoramento</title>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #56ccf2, #2f80ed);
      color: #333;
      text-align: center;
      padding: 20px;
    }
    .card {
      background: white;
      border-radius: 15px;
      box-shadow: 0 8px 15px rgba(0, 0, 0, 0.2);
      max-width: 600px;
      margin: 20px auto;
      padding: 20px;
    }
    .value {
      font-size: 2em;
      font-weight: bold;
    }
    #logout {
      margin-top: 10px;
      display: inline-block;
      color: #fff;
      background: #e74c3c;
      padding: 10px 15px;
      border-radius: 8px;
      text-decoration: none;
    }
    .danger {
      color: red;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Painel de Monitoramento</h1>
    <p>Usuário: %USER%</p>
    <p>Temperatura: <span id='temp' class='value'>--</span> °C</p>
    <p>Umidade: <span id='umid' class='value'>--</span> %</p>
    <a id="logout" href='/logout'>Logout</a>
  </div>
  <div class="card">
    <canvas id="tempChart"></canvas>
  </div>
  <div class="card">
    <canvas id="umidChart"></canvas>
  </div>

  <script>
    const socket = new WebSocket("ws://" + window.location.hostname + "/ws");

    const temp = document.getElementById("temp");
    const umid = document.getElementById("umid");

    const tempCtx = document.getElementById("tempChart").getContext("2d");
    const umidCtx = document.getElementById("umidChart").getContext("2d");

    const tempData = {
      labels: [],
      datasets: [{ label: "Temperatura (°C)", data: [], borderColor: "red", fill: false }]
    };
    const umidData = {
      labels: [],
      datasets: [{ label: "Umidade (%)", data: [], borderColor: "blue", fill: false }]
    };

    const tempChart = new Chart(tempCtx, {
      type: 'line',
      data: tempData,
      options: { responsive: true, scales: { y: { min: 0, max: 50 } } }
    });
    const umidChart = new Chart(umidCtx, {
      type: 'line',
      data: umidData,
      options: { responsive: true, scales: { y: { min: 0, max: 100 } } }
    });

    let t = 0;

    socket.onmessage = function(event) {
      const dados = JSON.parse(event.data);
      temp.textContent = dados.temperatura;
      umid.textContent = dados.umidade;

      temp.className = 'value';
      umid.className = 'value';
      if (dados.temperatura >= 35 || dados.temperatura <= 15) temp.classList.add("danger");
      if (dados.umidade >= 80 || dados.umidade <= 30) umid.classList.add("danger");

      if (tempData.labels.length > 20) {
        tempData.labels.shift();
        tempData.datasets[0].data.shift();
        umidData.labels.shift();
        umidData.datasets[0].data.shift();
      }
      t++;
      tempData.labels.push(t);
      tempData.datasets[0].data.push(dados.temperatura);
      umidData.labels.push(t);
      umidData.datasets[0].data.push(dados.umidade);

      tempChart.update();
      umidChart.update();
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

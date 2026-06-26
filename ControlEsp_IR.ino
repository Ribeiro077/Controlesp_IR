#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <time.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Tcl.h>

// ==========================================
// CONFIGURAÇÕES DE REDE E HARDWARE
// ==========================================
const char* ssid_casa = "Ribeiro";
const char* senha_casa = "seilamano";
const char* ssid_ap = "ESP8266_Controle";
const char* password_ap = "12345678";

// --- SEGURANÇA WEB ---
const char* www_login = "admin";
const char* www_senha = "123";

// --- CONFIGURAÇÕES DE RELÓGIO (NTP) ---
const char* ntpServer = "pool.ntp.br";
const long  gmtOffset_sec = -3 * 3600; // Fuso horário cravado em GMT-3
const int   daylightOffset_sec = 0;

const uint16_t kIrSenderPin = 5;       // Emissor IR: D1 (GPIO5)

IRTcl112Ac ac(kIrSenderPin);
ESP8266WebServer server(80);

// Variáveis Globais de Controle de Estado
int temperatura_atual = 22;
bool estadoAr = false;

// ==========================================
// FUNÇÃO DE GERAÇÃO E ENVIO DE COMANDO
// ==========================================
void enviarComandoAr() {
  uint8_t comando[14] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00};

  // 1. Lógica de Estado
  if (estadoAr) {
    comando[5] = 0x24; // Power On, Mode Cool
    comando[6] = 0x13; // Extra On
  } else {
    comando[5] = 0x20; // Power Off
    comando[6] = 0x03; // Extra Off
  }

  // 2. Trava de Hardware (Limites do Ar-Condicionado)
  int tempCalc = temperatura_atual;
  if (tempCalc < 17) tempCalc = 17;
  if (tempCalc > 30) tempCalc = 30;
  comando[7] = 31 - tempCalc;

  // 3. Cálculo de Segurança (Checksum Módulo 256)
  uint8_t checksum = 0;
  for (int i = 0; i < 13; i++) {
    checksum += comando[i];
  }
  comando[13] = checksum;

  // 4. Disparo Físico
  ac.setRaw(comando);
  ac.send();
  
  Serial.printf("Comando Enviado - Status: %s | Temp: %d C | Checksum: %02X\n", estadoAr ? "ON" : "OFF", tempCalc, checksum);
}

// ==========================================
// RENDERIZAÇÃO DA INTERFACE GRÁFICA (HTML)
// ==========================================
void handleRoot() {
  // Barreira de Login
  if (!server.authenticate(www_login, www_senha)) {
    return server.requestAuthentication();
  }

  String html = "";
  html.reserve(4000);

  html += F("<!DOCTYPE html><html lang='pt-BR'><head>");
  html += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  html += F("<title>Comando Virtual Inteligente</title>");
  html += F("<style>");
  html += F("body { font-family: 'Segoe UI', Arial, sans-serif; background: #121214; color: #e1e1e6; text-align: center; margin: 0; padding: 20px; }");
  html += F(".container { max-width: 400px; margin: 0 auto; background: #202024; padding: 25px; border-radius: 15px; box-shadow: 0 8px 24px rgba(0,0,0,0.3); }");
  html += F("h1 { font-size: 22px; margin-bottom: 5px; color: #04d361; }");
  html += F(".relogio { font-size: 14px; color: #a8a8b3; margin-bottom: 25px; font-family: monospace; }");
  html += F(".btn-power { background: #e04343; color: white; border: none; padding: 15px 40px; font-size: 18px; font-weight: bold; border-radius: 30px; cursor: pointer; margin-bottom: 30px; width: 80%; transition: 0.2s; }");
  html += F(".btn-power:hover { background: #a83232; }");
  html += F(".display-container { display: flex; align-items: center; justify-content: center; margin-bottom: 35px; gap: 20px; }");
  html += F(".temp-display { font-size: 54px; font-weight: bold; min-width: 150px; color: #ffffff; }");
  html += F(".btn-nav { background: #29292e; color: #04d361; border: 2px solid #04d361; font-size: 28px; width: 60px; height: 60px; border-radius: 50%; cursor: pointer; display: flex; align-items: center; justify-content: center; transition: 0.2s; }");
  html += F(".btn-nav:hover { background: #04d361; color: #121214; }");
  html += F(".status { margin-bottom: 15px; font-size: 16px; color: #a8a8b3; }");
  html += F("</style></head><body>");

  html += F("<div class='container'>");
  html += F("<h1>Virtual IR Remote</h1>");
  html += F("<div class='relogio' id='relogioDisplay'>Sincronizando hora...</div>");
  
  String textoStatus = estadoAr ? "LIGADO" : "DESLIGADO";
  html += F("<div class='status' id='statusDisplay'>Status atual: "); html += textoStatus; html += F("</div>");

  html += F("<button class='btn-power' onclick='dispararPower()'>LIGAR / DESLIGAR</button>");

  html += F("<div class='display-container'>");
  html += F("<button class='btn-nav' onclick='ajustarTemp(-1)'>▼</button>");
  html += F("<div class='temp-display' id='tempDisplay'>"); html += String(temperatura_atual); html += F(" °C</div>");
  html += F("<button class='btn-nav' onclick='ajustarTemp(1)'>▲</button>");
  html += F("</div>");

  html += F("</div>");

  html += F("<script>");
  html += F("function dispararPower() {");
  html += F("  fetch('/api/power', { method: 'POST' })");
  html += F("  .then(res => res.json()).then(data => { ");
  html += F("     if(data.sucesso) {");
  html += F("       document.getElementById('statusDisplay').innerText = 'Status atual: ' + data.estado;");
  html += F("     }");
  html += F("  });");
  html += F("}");
  
  html += F("function ajustarTemp(modificador) {");
  html += F("  fetch('/api/temperatura?mod=' + modificador, { method: 'POST' })");
  html += F("  .then(res => res.json())");
  html += F("  .then(data => {");
  html += F("    if(data.sucesso) {");
  html += F("      document.getElementById('tempDisplay').innerText = data.nova_temp + ' °C';");
  html += F("    } else {");
  html += F("      alert(data.aviso);");
  html += F("    }");
  html += F("  });");
  html += F("}");
  
  html += F("function atualizarRelogio() {");
  html += F("  fetch('/api/hora').then(res => res.json()).then(data => {");
  html += F("    document.getElementById('relogioDisplay').innerText = 'Hora local: ' + data.hora;");
  html += F("  });");
  html += F("}");
  html += F("setInterval(atualizarRelogio, 1000);");
  html += F("atualizarRelogio();");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// ==========================================
// HANDLERS DA API ASSÍNCRONA (JSON Backend)
// ==========================================
void handleApiPower() {
  if (!server.authenticate(www_login, www_senha)) return server.requestAuthentication();
  
  estadoAr = !estadoAr;
  enviarComandoAr();
  
  String strEstado = estadoAr ? "LIGADO" : "DESLIGADO";
  String response = "{\"sucesso\":true,\"estado\":\"" + strEstado + "\"}";
  server.send(200, "application/json", response);
}

void handleApiTemperatura() {
  if (!server.authenticate(www_login, www_senha)) return server.requestAuthentication();

  if (!server.hasArg("mod")) {
    server.send(400, "application/json", "{\"erro\":\"bad_request\"}");
    return;
  }
  
  int modificador = server.arg("mod").toInt();
  int alvo_temp = temperatura_atual + modificador;
  
  if (alvo_temp < 17 || alvo_temp > 30) {
    server.send(200, "application/json", "{\"sucesso\":false,\"aviso\":\"Limite atingido!\"}");
    return;
  }
  
  temperatura_atual = alvo_temp;
  enviarComandoAr();
  
  String response = "{\"sucesso\":true,\"nova_temp\":" + String(temperatura_atual) + "}";
  server.send(200, "application/json", response);
}

void handleApiHora() {
  if (!server.authenticate(www_login, www_senha)) return server.requestAuthentication();

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char horaStr[10];
  sprintf(horaStr, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  
  String response = "{\"hora\":\"" + String(horaStr) + "\"}";
  server.send(200, "application/json", response);
}

// ==========================================
// CONFIGURAÇÃO INICIAL (Setup)
// ==========================================
void setup() {
  Serial.begin(115200);
  ac.begin(); 

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid_casa, senha_casa);
  WiFi.softAP(ssid_ap, password_ap);

  Serial.print("\nConectando ao WiFi...");
  int tentativasWiFi = 0;
  
  // Proteção: Desiste após 15 segundos se não achar o roteador
  while (WiFi.status() != WL_CONNECTED && tentativasWiFi < 30) {
    delay(500);
    Serial.print(".");
    tentativasWiFi++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Conectado!");
    Serial.printf("IP Local: %s\n", WiFi.localIP().toString().c_str());
    
    Serial.print("Sincronizando relogio (NTP)...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    int tentativasNTP = 0;
    
    // Proteção: Desiste após 10 segundos se a internet estiver caída
    while (time(nullptr) < 100000 && tentativasNTP < 20) {
      delay(500);
      Serial.print(".");
      tentativasNTP++;
    }
    
    if(time(nullptr) < 100000) {
      Serial.println("\nFalha NTP: Sem internet. Relogio iniciara em 00:00:00.");
    } else {
      Serial.println("\nHora Sincronizada com sucesso!");
    }
  } else {
    Serial.println("\nFalha ao conectar no roteador. Operando isolado (Modo AP).");
    Serial.printf("IP do AP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/power", HTTP_POST, handleApiPower);
  server.on("/api/temperatura", HTTP_POST, handleApiTemperatura);
  server.on("/api/hora", HTTP_GET, handleApiHora);

  server.begin();
  Serial.println("Servidor HTTP pronto e iniciado.");
}

void loop() {
  server.handleClient();
  yield();
}

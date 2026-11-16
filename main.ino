#include "thingProperties.h"
#include <DHT.h>

// --- Mapeamento de Pinos (ESP32-S3 DevKit) ---
#define PIN_POT      1   // ADC1_CH0
#define PIN_LDR      2   // ADC1_CH1
#define PIN_DHT      4
#define PIN_LED_R    5
#define PIN_LED_G    6
#define PIN_LED_B    7
#define PIN_BUZZER   8
#define PIN_BOTAO    9

// --- Configurações dos Sensores ---
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

const int LIMITE_LUMINOSIDADE = 1800;

// --- Variáveis de Controle ---
volatile bool sistemaAtivoBotao = true;
volatile bool estadoBotaoMudou = false;
volatile unsigned long ultimaInterrupcao = 0;

bool sensorTempAtivo = true;
bool detectorLuzAtivo = true;
bool buzzerAtivo = true;
bool buzzerTocando = false;

bool overrideManualLigado = false;
bool overrideManualDesligado = false;

bool corTemporariaAtiva = false;
unsigned long inicioCorTemporaria = 0;
uint8_t tempR = 0, tempG = 0, tempB = 0;
String nomeCorTemporaria = "";

unsigned long ultimoSerial = 0;
unsigned long ultimaLeituraDHT = 0;

// --- Interrupção Externa do Botão ---
void IRAM_ATTR isrBotao() {
  unsigned long agora = millis();
  if (agora - ultimaInterrupcao > 250) {
    sistemaAtivoBotao = !sistemaAtivoBotao;
    estadoBotaoMudou = true;
    ultimaInterrupcao = agora;
  }
}

// --- Aplica PWM no LED RGB (Cátodo Comum) ---
void aplicarCorRGB(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}

// --- Espectro de Temperatura de Cor via Potenciômetro ---
void calcularCorPotenciometro(int valorADC, uint8_t &r, uint8_t &g, uint8_t &b, String &nomeCor) {
  if (valorADC < 820) {
    float t = valorADC / 820.0;
    r = 255; g = (uint8_t)(80 * t); b = 0;
    nomeCor = (t < 0.5) ? "Vermelho" : "Laranja";
  } else if (valorADC < 1640) {
    float t = (valorADC - 820) / 820.0;
    r = 255; g = (uint8_t)(80 + 140 * t); b = 0;
    nomeCor = "Amarelo";
  } else if (valorADC < 2460) {
    float t = (valorADC - 1640) / 820.0;
    r = 255; g = (uint8_t)(220 + 35 * t); b = (uint8_t)(255 * t);
    nomeCor = "Branco";
  } else if (valorADC < 3280) {
    float t = (valorADC - 2460) / 820.0;
    r = (uint8_t)(255 * (1.0 - t)); g = (uint8_t)(255 - 35 * t); b = 255;
    nomeCor = "Azul Turquesa";
  } else {
    float t = (valorADC - 3280) / 815.0;
    r = 0; g = (uint8_t)(220 * (1.0 - t)); b = 255;
    nomeCor = "Azul";
  }
}

// --- Callback dos Comandos Recebidos do Painel Online ---
void onComandoChange() {
  String cmd = comando;
  cmd.trim();

  if (cmd.equalsIgnoreCase("Ligar")) {
    overrideManualLigado = true;
    overrideManualDesligado = false;
  } else if (cmd.equalsIgnoreCase("Desligar")) {
    overrideManualLigado = false;
    overrideManualDesligado = true;
  } else if (cmd.equalsIgnoreCase("Vermelho")) {
    corTemporariaAtiva = true;
    inicioCorTemporaria = millis();
    tempR = 255; tempG = 0; tempB = 0;
    nomeCorTemporaria = "Vermelho (1s)";
  } else if (cmd.equalsIgnoreCase("Amarelo")) {
    corTemporariaAtiva = true;
    inicioCorTemporaria = millis();
    tempR = 255; tempG = 220; tempB = 0;
    nomeCorTemporaria = "Amarelo (1s)";
  } else if (cmd.equalsIgnoreCase("Azul")) {
    corTemporariaAtiva = true;
    inicioCorTemporaria = millis();
    tempR = 0; tempG = 0; tempB = 255;
    nomeCorTemporaria = "Azul (1s)";
  } else if (cmd.equalsIgnoreCase("Desativar Temperatura")) {
    sensorTempAtivo = false;
  } else if (cmd.equalsIgnoreCase("Ativar Temperatura")) {
    sensorTempAtivo = true;
  } else if (cmd.equalsIgnoreCase("Desativar Detector")) {
    detectorLuzAtivo = false;
  } else if (cmd.equalsIgnoreCase("Ativar Detector")) {
    detectorLuzAtivo = true;
    overrideManualLigado = false;
    overrideManualDesligado = false;
  } else if (cmd.equalsIgnoreCase("Desativar Buzzer")) {
    buzzerAtivo = false;
  } else if (cmd.equalsIgnoreCase("Ativar Buzzer")) {
    buzzerAtivo = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_BOTAO), isrBotao, FALLING);

  dht.begin();
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();

  // Atualiza o painel quando o botão físico for pressionado
  if (estadoBotaoMudou) {
    estadoBotaoMudou = false;
    comando = sistemaAtivoBotao ? "Botao: Sistema ON" : "Botao: Sistema OFF";
  }

  // Leitura apenas da temperatura do DHT22 a cada 2 segundos
  if (sensorTempAtivo && (millis() - ultimaLeituraDHT >= 2000)) {
    ultimaLeituraDHT = millis();
    float t = dht.readTemperature();
    if (!isnan(t)) {
      temperatura = t;
    } else {
      Serial.println("Falha ao ler temperatura do DHT22!");
    }
  }

  if (detectorLuzAtivo) {
    luminosidade = analogRead(PIN_LDR);
  }

  int valorPot = analogRead(PIN_POT);
  bool perigoTemperatura = sensorTempAtivo && (temperatura < 0.0 || temperatura > 30.0);

  // Controle do Buzzer
  if (sistemaAtivoBotao && buzzerAtivo && perigoTemperatura) {
    if (!buzzerTocando) {
      tone(PIN_BUZZER, 2000);
      buzzerTocando = true;
    }
  } else {
    if (buzzerTocando) {
      noTone(PIN_BUZZER);
      buzzerTocando = false;
    }
  }

  // Encerra efeito de cor temporária após 1 segundo
  if (corTemporariaAtiva && (millis() - inicioCorTemporaria >= 1000)) {
    corTemporariaAtiva = false;
  }

  // Regras de acionamento do LED RGB
  bool ligarLed = false;
  if (!sistemaAtivoBotao || perigoTemperatura || overrideManualDesligado) {
    ligarLed = false;
  } else if (overrideManualLigado || corTemporariaAtiva) {
    ligarLed = true;
  } else if (detectorLuzAtivo) {
    ligarLed = (luminosidade < LIMITE_LUMINOSIDADE);
  } else {
    ligarLed = true;
  }

  if (ligarLed) {
    uint8_t r, g, b;
    String nomeCor;
    if (corTemporariaAtiva) {
      r = tempR; g = tempG; b = tempB;
      nomeCor = nomeCorTemporaria;
    } else {
      calcularCorPotenciometro(valorPot, r, g, b, nomeCor);
    }
    aplicarCorRGB(r, g, b);
    cor_atual = nomeCor;
  } else {
    aplicarCorRGB(0, 0, 0);
    cor_atual = "OFF";
  }

  // Exibição no Monitor Serial a cada 1 segundo
  if (millis() - ultimoSerial >= 1000) {
    ultimoSerial = millis();
    Serial.print("LED RGB: ");
    Serial.println((cor_atual != "OFF") ? "ON" : "OFF");
    Serial.printf("Temperatura: %.1f °C | LDR: %d\n",
                  temperatura, luminosidade);
    if (perigoTemperatura) {
      Serial.println("Perigo! Desligar!");
    }
  }
}

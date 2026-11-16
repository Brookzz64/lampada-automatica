#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

const char DEVICE_LOGIN_NAME[] = "7786c9a2-0504-4ae0-9d5d-624072d5cc56";
const char SSID[]              = "AndroidAP2B9A";
const char PASS[]              = "noid5756";
const char DEVICE_KEY[]        = "Ph22cdzc31LfYTeRANUwHfhhu";

void onComandoChange();

float temperatura;
int luminosidade;
String cor_atual;
String comando;

void initProperties() {
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);

  ArduinoCloud.addProperty(temperatura, READ, 5 * SECONDS, NULL);
  ArduinoCloud.addProperty(luminosidade, READ, 5 * SECONDS, NULL);
  ArduinoCloud.addProperty(cor_atual, READ, 2 * SECONDS, NULL);
  ArduinoCloud.addProperty(comando, READWRITE, ON_CHANGE, onComandoChange);
}

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

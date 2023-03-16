#include <esp_now.h>
#include <esp_wifi.h>
#include <ESPNtpClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <FastBot.h>
#include "private.h"

typedef struct sensor_data_t {    // Формат данных датчика для отправки по ESP-Now на шлюз
  int           unit;             // Номер блока для определения, какой датчик отправляет
  float         temp;             // Температура (Цельсия)
  float         battery;          // Уровень заряда батареи (Процент)
  char          ID[80];           // Любой открытый текст для идентификации устройства
  int           wakeTimeMS;       // Время пробуждения датчика до отправки данных
  unsigned long updated;          // Время epoch получения шлюзом
} sensor_data_t;                  // Время устанавливается шлюзом/получателем (не используется датчиком, но является частью структуры для удобства)

unsigned long previousMillis = 0;
const long interval = 5000;

sensor_data_t bufSensorData;                 // буфер для входящих данных
sensor_data_t sensorData[CONFIG_UNITS + 1];  // буфер для всех данных датчика

WiFiMulti wifiMulti;

WebServer server(80);

FastBot bot(BOT_KEY);


// Обратный вызов при получении данных от отправителя
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  char macStr[24];
  snprintf(macStr, sizeof(macStr), " %02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("\nData received from: "); Serial.println(macStr);
  memcpy(&bufSensorData, data, sizeof(bufSensorData));

  // Отправляем данные в Serial
  Serial.print ("ID: ");
  Serial.print (bufSensorData.ID);
  Serial.print ("  Unit: ");
  Serial.print (bufSensorData.unit);
  Serial.print ("  Temp: ");
  Serial.print (bufSensorData.temp);
  Serial.print ("  Battery: ");
  Serial.print (bufSensorData.battery);
  Serial.print ("  Wake: ");
  Serial.print (bufSensorData.wakeTimeMS);
  Serial.println ("");

  // Telegram
  bot.sendMessage("ID: " + \
    String(bufSensorData.ID) + ", Unit: " + \
    String(bufSensorData.unit) + ", Temp: " + \
    String(bufSensorData.temp) + ", Battery: " + \
    String(bufSensorData.battery) \
  );

  // Сохранение данных
  int i = bufSensorData.unit;
  if ( (i >= 1) && (i <= CONFIG_UNITS) ) {
    memcpy(&sensorData[i], data, sizeof(bufSensorData));
  };
}


void handleRoot()
{
  String msg;
  String id;
  msg = "{\"code\":200,";
  msg += "\"items\":[";
  for (int i=1; i<=CONFIG_UNITS; i++) {
    if (strlen(sensorData[i].ID) == 0) {
      id = String(i);
    } else {
      id = sensorData[i].ID;
    }
    msg += "{\"id:\":\""+id+"\",\"unit\":"+String(i)+",\"temp:\":"+String(sensorData[i].temp)+",\"battery:\":"+String(sensorData[i].battery)+"}";
    msg += (CONFIG_UNITS > i) ? "," : "";
  }
  msg += "]}";
  server.send(200, "application/json", msg);
}


void handleNotFound()
{
  String msg;
  msg = "{\"code\":404,";
  msg += "\"error\":\"File Not Found\",";
  msg += "\"uri:\":\""+server.uri()+"\",";
  msg += "\"method:\":\""+String((server.method() == HTTP_GET) ? "GET" : "POST")+"\",";
  msg += "\"arguments:\":"+String(server.args())+",";
  msg += "\"arg\":[";
  for (uint8_t i = 0; i < server.args(); i++) {
    msg += "{\"" + server.argName(i) + "\":\"" + server.arg(i) + "\"}";
    msg += (server.args() > i + 1) ? "," : "";
  }
  msg += "]}";
  server.send(404, "application/json", msg);
}


void setupConfig()
{
  Serial.begin(115200);
  while (!Serial) {};
  delay(100);
  Serial.println("\n\n");

  WiFi.mode(WIFI_AP);
  Serial.printf("My HW mac: %s", WiFi.macAddress().c_str());
  Serial.println("");

  wifi_config_t current_conf;
  esp_wifi_get_config(WIFI_IF_STA, &current_conf);
}


void setupWifi()
{
  Serial.print("Connecting to WiFi ");

  // Установите устройство в режим AP для начала
  WiFi.mode(WIFI_AP_STA);                         // Требуется AP и STA (!ВАЖНО)

  wifiMulti.addAP(WIFI_SSID, WIFI_PSWD);          // Использую wifiMulti по привычке
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Подключились
  Serial.println(" Done");

  // Возвращаем данные Wi-Fi
  Serial.println("Set as AP_STA station.");
  Serial.print  ("SSID: "); Serial.println(WiFi.SSID());
  Serial.print  ("Channel: "); Serial.println(WiFi.channel());
  Serial.print  ("IP address: "); Serial.println(WiFi.localIP());
  Serial.print  ("DNS IP: "); Serial.println(WiFi.dnsIP());
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8,8,8,8)); 
  delay(10);
  Serial.print  ("DNS IP new: "); Serial.println(WiFi.dnsIP());
  delay(1000);
}


void setupESPNow()
{
  // Конфигурация gateway AP — установка SSID и канала
  int channel = WiFi.channel();
  if (WiFi.softAP(WIFI_SSID, WIFI_PSWD, channel, 1)) {
    Serial.println("AP Config Success.");
    Serial.println("AP SSID: " + String(WIFI_SSID));
  } else {
    Serial.println("AP Config failed.");
  }

  // Возвращаем MAC-адреса
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP - Now Init Success");
  } else {
    Serial.println("ESP - Now Init Failed");
    ESP.restart();                           // просто перезапускаем, если мы не можем запустить ESP-Now
  }

  // ESP-Now инициализирован. Регистрируем обратный вызов для получения данных
  esp_now_register_recv_cb(OnDataRecv);
}


void setupWebServer()
{
  // Устанавливаем функции обратного вызова для веб-сервера
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  // Запускаем веб-сервер
  server.begin();
  Serial.print("WEB server started on SSID: ");
  Serial.print (WiFi.SSID());
  Serial.print (" with IP: ");
  Serial.println(WiFi.localIP());
}


void setupNTP()
{
  NTP.setTimeZone(TZ_Etc_UTC);
  NTP.begin();
}


void setupBot()
{
  bot.setChatID(CHAT_ID);
  bot.setTextMode(FB_MARKDOWN);
}


void setup()
{
  setupConfig();
  setupWifi();
  setupESPNow();
  setupWebServer();
  setupNTP();
  setupBot();
}


void mock(uint8_t index, long min, long max)
{
  uint8_t GatewayMac[] = {0x30, 0xAE, 0xA4, 0xF1, 0xFD, 0xFC};

  strcpy(bufSensorData.ID, "Mock");

  bufSensorData.unit = index;
  bufSensorData.temp = random(min, max);
  bufSensorData.battery = 70;

  uint8_t sendBuf[sizeof(sensorData)];
  bufSensorData.wakeTimeMS = millis();
  memcpy(sendBuf, &bufSensorData, sizeof(bufSensorData));

  const uint8_t *mac_addr = &GatewayMac[0];
  const uint8_t *data = sendBuf;
  const int data_len = sizeof(bufSensorData);

  OnDataRecv(mac_addr, data, data_len);
}


void loop()
{
  // unsigned long currentMillis = millis();
  // if (currentMillis - previousMillis >= interval) {
  //   previousMillis = currentMillis;
  //   mock(25, 40, 90);
  //   mock(31, 10, 40);
  // }

  server.handleClient();

  bot.tick();
}

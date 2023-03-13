#include "connect.h"
#include "everytime.h"

sensor_data_t bufSensorData;        // buffer for incoming data
sensor_data_t sensorData[UNITS+1];  // buffer for all sensor data

uint8_t GatewayMac[] = {0x02, 0x10, 0x11, 0x12, 0x13, 0x14};

WiFiMulti wifiMulti;

WebServer server(80);

// Callback when data is received from any Sender
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  char macStr[24];
  snprintf(macStr, sizeof(macStr), " %02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial.print("\nData received from: "); Serial.println(macStr);
  memcpy(&bufSensorData, data, sizeof(bufSensorData));

  // Print data
  Serial.print ("ID: ");
  Serial.print (bufSensorData.ID);
  Serial.print ("  Unit: ");
  Serial.print (bufSensorData.unit);
  Serial.print ("  Temp: ");
  Serial.print (bufSensorData.temp);
  Serial.print ("  Vbat: ");
  Serial.print (bufSensorData.Vbat);
  Serial.print ("  Wake: ");
  Serial.print (bufSensorData.wakeTimeMS);
  Serial.println ("");

  // Store data
  int i = bufSensorData.unit;
  if ( (i >= 1) && (i <= UNITS) ) {
    add_new_measure(i, bufSensorData);
    memcpy(&sensorData[i], data, sizeof(bufSensorData));
  };
}


void handleRoot()
{
  String msg;
  msg = "{\"code\":200,";
  msg += "\"items\":[";
  for (int i=1; i<=UNITS; i++) {
    msg += "{\"unit\":"+String(i)+",\"temp:\":"+String(sensorData[i].temp) + ",\"battery:\":"+String(sensorData[i].Vbat)+"}";
    msg += (UNITS > i) ? "," : "";
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


void setupConnect()
{
  WiFi.mode(WIFI_AP); 
  esp_wifi_set_mac(WIFI_IF_STA, &GatewayMac[0]);
  
  // Connect to WiFi ------------------------------
  Serial.print("Connecting to WiFi ");
  
  // Set device in AP mode to begin with
  WiFi.mode(WIFI_AP_STA);                         // AP _and_ STA is required (!IMPORTANT)

  wifiMulti.addAP(SOFTAP_SSID, SOFTAP_PASS);      // I use wifiMulti ... just by habit, i guess ....
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Come here - we are connected
  Serial.println(" Done");
 
  // Print WiFi data
  Serial.println("Set as AP_STA station.");
  Serial.print  ("SSID: "); Serial.println(WiFi.SSID());
  Serial.print  ("Channel: "); Serial.println(WiFi.channel());
  Serial.print  ("IP address: "); Serial.println(WiFi.localIP());
  delay(1000);


  // Initialize ESP-Now ---------------------------

  // Config gateway AP - set SSID and channel 
  int channel = WiFi.channel();
  if (WiFi.softAP(SOFTAP_SSID, SOFTAP_PASS, channel, 1)) {
    Serial.println("AP Config Success. AP SSID: " + String(SOFTAP_SSID));
  } else {
    Serial.println("AP Config failed.");
  }
  
  // Print MAC addresses
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());

  // Init ESP-Now 
  #define ESPNOW_SUCCESS ESP_OK

  if (esp_now_init() == ESPNOW_SUCCESS) {
    Serial.println("ESP - Now Init Success");
  } else {
    Serial.println("ESP - Now Init Failed");
    ESP.restart();                                // just restart if we cant init ESP-Now
  }
  
  // ESP-Now is now initialized. Register a callback fcn for when data is received
  esp_now_register_recv_cb(OnDataRecv);

  // Set web server callback functions
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  // Start web server
  server.begin();
  Serial.print("WEB server started on SSID: ");
  Serial.print (WiFi.SSID()); 
  Serial.print (" with IP: "); 
  Serial.println(WiFi.localIP());
}

void mock(uint8_t index, long min, long max) 
{
    strcpy (bufSensorData.ID, "Mock");

    bufSensorData.unit = index;
    bufSensorData.temp = random(min, max);
    bufSensorData.Vbat = 70;

    uint8_t sendBuf[sizeof(sensorData)];
    bufSensorData.wakeTimeMS = millis();
    memcpy(sendBuf, &bufSensorData, sizeof(bufSensorData));

    const uint8_t *mac_addr = &GatewayMac[0];
    const uint8_t *data = sendBuf;
    const int data_len = sizeof(bufSensorData);

    OnDataRecv(mac_addr, data, data_len);
}

void loopConnect()
{
  server.handleClient();

  EVERY_N_SECONDS(INTERVAL_READ_SENSOR)
  {
    mock(_TEMP_1, 10, 40);
    mock(_TEMP_2, 40, 90);
  }

}
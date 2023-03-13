#include <WiFi.h>
#include <esp_now.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define WIFI_CHANNEL        1       // должен быть 1
#define UNIT                1       // Идентификатор сенсора
#define DEBUG_LOG                   // ключите (раскомментируйте), чтобы распечатать отладочную информацию. Отключение (комментарий) вывода отладки экономит около 4-5 мс...
//ADC_MODE(ADC_VCC);                // Раскомментируйте, чтобы включить чтение Vcc (если плата не может читать VBAT).
#define SLEEP_SECS        5*60-8    // [sec] Время сна между пробуждением и чтением. Будет 5 минут +/- 8 секунд. Варьируется, чтобы избежать передачи коллизий.
#define MAX_WAKETIME_MS   1000      // [ms]  Тайм-аут до принудительного перехода в сон, если отправка не удалась
#define ONE_WIRE_BUS            15  // Измените на другой порт, если необходимо
#define TEMPERATURE_PRECISION   12  // [9-12]. 12 => разрешение 0,0625C
                      /* 12-битная точность:
                         1 бит для знака, 7 бит для целой части и 4 бита для дробной части (4 цифры после запятой)
                         Диапазон температур: от xxx,0000C до xxx,9375C с дискретным шагом 0,0625C.
                      */
OneWire oneWire (ONE_WIRE_BUS);                   // Настройте экземпляр oneWire для связи с любыми устройствами OneWire.
DallasTemperature tempSensor(&oneWire);           // Передайте нашу ссылку oneWire на Dallas Temperature.
DeviceAddress tempDeviceAddress;                  // Мы будем использовать эту переменную для хранения адреса найденного устройства.

uint8_t Gateway_Mac[] = {0x02, 0x10, 0x11, 0x12, 0x13, 0x14};
                                  // MAC адрес шлюза

typedef struct sensor_data_t {    // Формат данных датчика для отправки по ESP-Now на шлюз
  int           unit;             // Номер блока для определения, какой датчик отправляет
  float         temp;             // Температура (Цельсия)
  float         Vbat;             // Уровень напряжения батареи (Вольт)
  char          ID[80];           // Любой открытый текст для идентификации устройства
  int           wakeTimeMS;       // Время пробуждения датчика до отправки данных
  unsigned long updated;          // Время epoch получения шлюзом. Устанавливается шлюзом/получателем. (Не используется датчиком, но является частью структуры для удобства.)
} sensor_data_t;


// -----------------------------------------------------------------------------------------
//  BATTERY LEVEL CALIBRATION
// -----------------------------------------------------------------------------------------
#define CALIBRATION         4.21 / 4.35                       // Measured V by multimeter / reported (raw) V 
                                                              // (Set to 1 if no calibration is needed/wanted)
#define VOLTAGE_DIVIDER     (130+220+100)/100 * CALIBRATION   // D1 Mini Pro voltage divider to A0. 
                                                              // May be different for other boards.


// -----------------------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------------------
sensor_data_t sensorData;
volatile boolean messageSent;     // флаг, чтобы сказать, когда сообщение отправлено, и мы можем безопасно идти спать


// -----------------------------------------------------------------------------------------
void gotoSleep()
// -----------------------------------------------------------------------------------------
{
  int sleepSecs;

  sleepSecs = SLEEP_SECS + ((uint8_t)esp_random()/16);      // добавить случайное время, чтобы избежать коллизий
  #ifdef DEBUG_SENSOR      
    Serial.printf("Up for %i ms, going to sleep for %i secs...\n", millis(), sleepSecs); 
  #endif

  esp_sleep_enable_timer_wakeup(sleepSecs * 1000000);
  //esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);
  esp_deep_sleep_start(); 
}


// -----------------------------------------------------------------------------------------
void setup()
// -----------------------------------------------------------------------------------------
{
  // отключаем Wi-Fi, пока его не используем, для экономии энергии
  WiFi.persistent( false );         // Не сохраняйте информацию о WiFi во Flash, чтобы сэкономить время


  #ifdef DEBUG_LOG
  Serial.begin(115200);
  while (!Serial) {};
  Serial.println("\n\nStart");
  #endif

  // инициализация датчика/шины
  //pinMode(ONE_WIRE_BUS, OUTPUT);     // используйте это, если используете режим PARASITE DS18B20 (vcc от линии передачи данных и, возможно, подтягивающий резистор ...)
  tempSensor.begin();
  tempSensor.getAddress(tempDeviceAddress, 0);
  tempSensor.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
  tempSensor.requestTemperatures();


  // считываем напряжение батареи
  int raw = analogRead(A0);
  sensorData.Vbat = raw * VOLTAGE_DIVIDER / 1023.0;
          // Альтернатива. Если не удается прочитать уровень заряда батареи на вашей плате, вместо этого прочитайте Vcc
          // const float calVal = 0.001108; // 3,27/2950=0,001108. Vcc 3.27 на мультиметре, 2950 от getVcc()
          // SensorData.Vbat = ESP.getVcc()/1023; // * calVal;
  #ifdef DEBUG_LOG
  Serial.print("Battery voltage:"); Serial.print(sensorData.Vbat); Serial.println(" V");
  #endif

  // скомпилировать сообщение для отправки
  strcpy (sensorData.ID, "Apt");
  strcat (sensorData.ID, " - ");
  strcat (sensorData.ID, "31");

  sensorData.unit = UNIT;
  sensorData.temp = tempSensor.getTempC(tempDeviceAddress);


  // настраиваем ESP-Now ---------------------------
  WiFi.mode(WIFI_STA); // режим станции для сенсорного узла esp-now
  WiFi.disconnect();
  #ifdef DEBUG_LOG
  Serial.printf("My HW mac: %s", WiFi.macAddress().c_str());
  Serial.println("");
  Serial.printf("Sending to MAC: %02x:%02x:%02x:%02x:%02x:%02x", Gateway_Mac[0], Gateway_Mac[1], Gateway_Mac[2], Gateway_Mac[3], Gateway_Mac[4], Gateway_Mac[5]);
  Serial.printf(", on channel: %i\n", WIFI_CHANNEL);
  #endif

  // инициализация ESP-Now ----------------------------
  if (esp_now_init() != 0) {
    #ifdef DEBUG_LOG
    Serial.println("*** ESP_Now init failed. Going to sleep");
    #endif
    delay(100);
    gotoSleep();
  }


  esp_now_peer_info_t gateway;
  memcpy(gateway.peer_addr, Gateway_Mac, 6);
  gateway.channel = WIFI_CHANNEL;
  gateway.encrypt = false;            // нет шифрования
  esp_now_add_peer(&gateway);  


  esp_now_register_send_cb([](const uint8_t* mac, esp_now_send_status_t sendStatus) {
    // обратный вызов для отправленного сообщения
    messageSent = true;         // флаг отправки сообщения - теперь мы можем спокойно идти спать...
    #ifdef DEBUG_LOG
    Serial.printf("Message sent out, sendStatus = %i\n", sendStatus);
    #endif
  });

  messageSent = false;

  // Send message -----------------------------------
  #ifdef DEBUG_LOG
  Serial.println("Message Data: " + \
                  String(sensorData.ID) + ", Unit:" + \
                  String(sensorData.unit) + ", Temp:" + \
                  String(sensorData.temp) + "C, Vbat:" + \
                  String(sensorData.Vbat) \
                );
  #endif
  uint8_t sendBuf[sizeof(sensorData)];          // создать буфер отправки для отправки данных датчика (безопаснее)
  sensorData.wakeTimeMS = millis();             // установить время пробуждения
  memcpy(sendBuf, &sensorData, sizeof(sensorData));
  const uint8_t *peer_addr = gateway.peer_addr;
  esp_err_t result=esp_now_send(peer_addr, (uint8_t *) &sensorData, sizeof(sensorData)); 
  
  #ifdef DEBUG_LOG
  Serial.print("Wake: "); Serial.print(sensorData.wakeTimeMS); Serial.println(" ms");
  Serial.print("Sending result: "); Serial.println(result);
  #endif
}

// -----------------------------------------------------------------------------------------
void loop()
// -----------------------------------------------------------------------------------------
{
  // ждем, пока не будет отправлено сообщение ESP-Now или истечет время ожидания, 
  // затем переходим в спящий режим
  if (messageSent || (millis() > MAX_WAKETIME_MS)) {
    gotoSleep();
  }
}



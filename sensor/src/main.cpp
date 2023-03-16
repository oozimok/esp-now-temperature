#include <WiFi.h>
#include <esp_now.h>


#define WIFI_CHANNEL        1       // должен быть 1
#define UNIT_ID             31      // квартира
#define UNIT                1       // Идентификатор сенсора
//#define DEBUG_LOG                   // Включите (раскомментируйте), чтобы распечатать отладочную информацию. Отключение (комментарий) вывода отладки экономит около 4-5 мс...
#define SLEEP_SECS        5*60-8    // [sec] Время сна между пробуждением и чтением. Будет 5 минут +/- 8 секунд. Варьируется, чтобы избежать передачи коллизий.
#define MAX_WAKETIME_MS   1000      // [ms]  Тайм-аут до принудительного перехода в сон, если отправка не удалась
#define ADC_BATTERY_PIN         33  // Порт подключения измерителя напряжения батареи
#define ADC_BATTERY_MAX_VOLTAGE 4.2 // Максимальное напряжение батареи
#define ONE_WIRE_BUS            15  // Измените на другой порт, если необходимо
#define TEMPERATURE_PRECISION   12  // [9-12]. 12 => разрешение 0,0625C
                      /* 12-битная точность:
                         1 бит для знака, 7 бит для целой части и 4 бита для дробной части (4 цифры после запятой)
                         Диапазон температур: от xxx,0000C до xxx,9375C с дискретным шагом 0,0625C.
                      */

//#define SENSOR_DS18B20
#define SENSOR_LM35
#define SENSOR_LM35_PIN         35  // Порт подключения датчика температуры LM35

#ifdef SENSOR_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire (ONE_WIRE_BUS);                   // Настройте экземпляр oneWire для связи с любыми устройствами OneWire.
DallasTemperature tempSensor(&oneWire);           // Передайте нашу ссылку oneWire на Dallas Temperature.
DeviceAddress tempDeviceAddress;                  // Мы будем использовать эту переменную для хранения адреса найденного устройства.
#endif

#ifdef SENSOR_LM35
#include "esp_adc_cal.h" 

uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}
#endif

uint8_t Gateway_Mac[] = {0x30, 0xAE, 0xA4, 0xF1, 0xFD, 0xFC};
                                  // MAC адрес шлюза

typedef struct sensor_data_t {    // Формат данных датчика для отправки по ESP-Now на шлюз
  int           unit;             // Номер блока для определения, какой датчик отправляет
  float         temp;             // Температура (Цельсия)
  float         battery;          // Уровень заряда батареи (Процент)
  char          ID[80];           // Любой открытый текст для идентификации устройства
  int           wakeTimeMS;       // Время пробуждения датчика до отправки данных
  unsigned long updated;          // Время epoch получения шлюзом. Устанавливается шлюзом/получателем. (Не используется датчиком, но является частью структуры для удобства.)
} sensor_data_t;


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

  #ifdef SENSOR_DS18B20
    // инициализация датчика/шины
    //pinMode(ONE_WIRE_BUS, OUTPUT);     // используйте это, если используете режим PARASITE DS18B20 (vcc от линии передачи данных и, возможно, подтягивающий резистор ...)
    tempSensor.begin();
    tempSensor.getAddress(tempDeviceAddress, 0);
    tempSensor.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
    tempSensor.requestTemperatures();
  #endif


  // считываем напряжение батареи
  float reading = 0; 
  for(int i = 0; i < 5; ++i) {
    reading += analogRead(ADC_BATTERY_PIN);
  }
  reading /= 5;
  float batteryLevelVoltage = reading * 3.307f / 4095.0f * ADC_BATTERY_MAX_VOLTAGE / 3.3f;   // рассчитываем текущий уровень напряжения
  float batteryLevel = (reading - 2340.0f) * (100.0f - 0.0f) / (4095.0f - 2340.0f) + 0.0f;   // рассчитываем процентное значение
  sensorData.battery = batteryLevel;


  #ifdef DEBUG_LOG
    Serial.print("Battery:"); Serial.print(batteryLevel); Serial.println(" %");
  #endif


  // скомпилировать сообщение для отправки
  String(UNIT_ID).toCharArray(sensorData.ID, 80);
  sensorData.unit = UNIT;
  
  #ifdef SENSOR_DS18B20
    sensorData.temp = tempSensor.getTempC(tempDeviceAddress);
  #endif

  #ifdef SENSOR_LM35
    int analogReadTemp = analogRead(SENSOR_LM35_PIN);
    float voltageTemp = readADC_Cal(analogReadTemp); 
    sensorData.temp = voltageTemp / 10;
  #endif
 
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
  gateway.ifidx = WIFI_IF_STA;
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
      String(sensorData.ID) + ", Unit: " + \
      String(sensorData.unit) + ", Temp: " + \
      String(sensorData.temp) + ", Battery: " + \
      String(sensorData.battery) \
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



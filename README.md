# ESP-Now Temperature
Устройства на базе ESP32 отправляющие данные о температуре по беспроводной связи с помощью технологии ESP-Now на шлюз, 
подключенный к Интернету с помощью WiFi (одновременно).

## Зачем это?
- Используются дешевые стандартные платы ESP32, запрограммированных в среде PlatformIO;
- 1+ год работы на аккумуляторе 1200мАч или 18650;
- Постоянная работа от небольшой солнечной панели (80x55 мм);
- Приемник (шлюз) ESP-Now подключен к WiFi/Интернету - одновременно;
- Больший радиус действия, чем у WiFi. Нет необходимости во внешних антеннах.

## Типичное использование
- Мониторинг температуры бойлеров в многоквартирных домах. 
- Устройства работающие от аккумуляторов и/или солнечных батарей, подключенные к домашней автоматике, приложениям Blynk, Thingspeak, MQTT и т.д.
- Больший радиус действия и связь по сравнению с WiFi.

## Время жизни
Типичное время жизни платы ESP32 и датчика температуры:
- 6+ месяцев при использовании 1200mAh LiPo батареи и датчика температуры DS18B20
- 12+ месяцев при использовании литий-ионного аккумулятора 2200mAh и DS18B20.
- Непрерывная работа с использованием небольшой солнечной панели 80x55 мм 5В и зарядного устройства TP4056 с батареей 1200 или 2200 мАч.
- "Почти" непрерывная работа с солнечной панелью 45x45 мм 5В, TP4056 и небольшим аккумулятором 500 мАч.

## Описание датчиков
Сообщения ESP-Now отправляет всего несколько байтов, которые достаточны для отправки данных датчика.
Тем не менее, я остановился на более общем и длинном формате, поскольку дополнительное время для передачи большей полезной нагрузки имеет довольно низкое влияние на общее энергопотребление и срок службы батареи.

Потребление энергии можно разделить на 3 класса:
1. Период глубокого сна: 300 секунд, 50-200 мкА.

   _Примечание. Некоторые платы потребляют гораздо больше этого значения в глубоком сне._

2. Время пробуждения, считывание показаний датчиков, уровня заряда батареи и т.д. при выключенном WiFi: 100-500 мс, 15-30 мА.

   _Предпочтительны быстрые датчики._

3. Время бодрствования, отправка данных на ESP-Now с включенным WiFi: 60 мс, 70-150 мА.

   _Быстро, благодаря ESP-Now._

Из приведенных выше типичных значений видно, что основное потребление энергии происходит в режиме глубокого сна, поэтому важно использовать платы ESP с низким током глубокого сна. 

## Краткое описание шлюза
Шлюз является ведомым устройством в терминологии ESP-Now и принимает данные датчика, отправленные на его MAC-адрес.
WiFi и ESP-Now могут быть активированы и работать одновременно. 
Это позволяет одному MCU передавать данные датчика дальше с помощью WiFi. 
Это, кажется, не очень известно. И это немного сложно для работы.
Смотрите исходный код, как это сделать.


## Краткое описание системы
Система состоит из 20 маломощных датчиков и 1 шлюза. 
Каждый датчик посылает одноадресное сообщение каждые 5 минут на MAC-адрес шлюза. 
Шлюз получает сообщения, обрабатывает их и отправляет дальше по WiFi на любой локальный или интернет-сервис. 
Шлюз может принимать сообщения от датчиков (на ESP-Now) и одновременно общаться по WiFi без каких-либо пропусков связи. 
Шлюз, подключенный к Blynk для отображения и мониторинга датчиков, является хорошим примером, который работает очень хорошо.

Ограничением является то, что шлюз должен использовать один и тот же канал WiFi на ESP-Now и WiFi. 
И из-за некоторых ограничений в реализации ESP WiFi, это работает только на первом канале. 
(Я читал статьи о том, что это должно быть возможно и на других каналах, но у меня получилось только на первом канале). 
Это означает, что WiFi роутер должен быть настроен на канал 1 в диапазоне 2,4 ГГц. 
Он не может работать на "автоматическом канале".

Есть некоторые хитрости, связанные с установкой WiFi, чтобы заставить его работать правильно. 
См. исходный код. (Возможно, есть и другие хитрости, необходимые для работы на любом другом канале, кроме первого).

Возможно и предпочтительно использовать программно определяемый MAC-адрес в шлюзе вместо аппаратного MAC-адреса по умолчанию. 
Это позволяет без проблем заменить аппаратное обеспечение шлюза.

Можно запустить несколько систем в паралельном режиме. 
Просто используйте уникальный MAC-адрес для каждой системы. 

Утверждается, что технологии ESP-Now имеет "в 3 раза большую дальность доступа", чем WiFi. 
Я не проводил никаких измерений, но я отметил достаточно большую дальность доступа для моих датчиков в моих установках.

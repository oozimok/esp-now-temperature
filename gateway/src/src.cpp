#include "bars.h"
#include "connect.h"
#include "oled.h"

void setup()
{
    // Init Serial
    Serial.begin(115200);
    while (!Serial) {};
    delay(100);
    Serial.println("\n\n");
    
    // Print sketch intro ---------------------------
    Serial.println();
    Serial.println("===========================================");

    // подготовка истории измерений
    init_measures();

    // настраиваем wifi
    setupConnect();

    // настраиваем дисплей
    setupOLED();
}

void loop()
{
    loopConnect();
    loopOLED();
}

#include <Wire.h>
#include "everytime.h"
#include "bars.h"
#include <GyverOLED.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) ///< Generic macro for obtaining number of elements of an array

// Вывод графика температуры или крупных цифр
#define DISPLAY_SENSORS 0
#define DISPLAY_PLOT_TEMP_1 1
#define DISPLAY_PLOT_TEMP_2 2

void update_current_measures_every_1s();
void setupOLED();
void print_big_measures();
void print_small_measures();
void draw_bars();
void loopOLED();

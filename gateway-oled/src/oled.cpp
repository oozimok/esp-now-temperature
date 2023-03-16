#include "oled.h"

GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;

uint8_t display_sensor_type = _TEMP_1;

// список режимов по порядку
uint8_t display_mode_list[] = {
    DISPLAY_SENSORS,
    DISPLAY_PLOT_TEMP_1,
    DISPLAY_PLOT_TEMP_2,
};

// индекс текущего режима в списке
uint8_t display_mode_index = 0;

// текущий режим
uint8_t display_mode = DISPLAY_SENSORS;

// количество режимов отображения
const uint8_t display_mode_count = ARRAY_SIZE(display_mode_list);

// обновить значения маленьких цифр
bool need_update_plot_numbers = true;


void setupOLED()
{
    // инициализация i2c
    Wire.begin();

    // экран
    oled.init();
    oled.clear();
    oled.setScale(1);
    oled.setCursorXY(10, 8);
    oled.print(__DATE__);
    oled.setCursorXY(10, 16);
    oled.print(__TIME__);
    oled.update();
    delay(400);
    oled.clear();
    oled.update();
} 

// выводит большие цифры на весь экран без графиков
void print_big_measures()
{
    char s[6];
    oled.setCursorXY(0, 10);
    oled.setScale(2);
    oled.invertText(false);
    oled.print(F("TEMP1:"));
    sprintf(s, "%4u", measures[_TEMP_1].current);
    oled.print(s);
    oled.setScale(1);
    oled.print(F("c"));

    oled.setCursorXY(0, 40);
    oled.setScale(2);
    oled.invertText(false);
    oled.print(F("TEMP2:"));
    sprintf(s, "%4u", measures[_TEMP_2].current);
    oled.print(s);
    oled.setScale(1);
    oled.print(F("c"));
}

// выводит маленькие цифры справа на графиках
void print_small_measures()
{
    int x = 104;
    char s[6];
    oled.setScale(1);
    oled.setCursorXY(x, 0);
    oled.invertText(display_sensor_type == _TEMP_1);
    oled.print(F("Tmp1"));
    oled.invertText(false);
    oled.setCursorXY(x, 0 + 9);
    sprintf(s, "%4u", measures[_TEMP_1].current);
    oled.print(s);
    oled.setCursorXY(x, 0 + 9 + 9);
    oled.print(F("c"));

    oled.setCursorXY(x, 32);
    oled.invertText(display_sensor_type == _TEMP_2);
    oled.print(F("Tmp2"));
    oled.invertText(false);
    oled.setCursorXY(x, 32 + 9);
    sprintf(s, "%4u", measures[_TEMP_2].current);
    oled.print(s);
    oled.setCursorXY(x, 32 + 9 + 9);
    oled.print(F("c"));
}

// рисует графики
void draw_bars()
{
    // столбики графика
    uint16_t h = 64 - 1;
    for (uint8_t i = 0; i < TOTAL_MEASURES; i++)
    {
        if (samples[i] > 0)
        {
            uint16_t x = i * 2;
            uint16_t y = h - samples[i];
            oled.fastLineV(x, y, h);
        }
    }

    // вертикальные линии пунктиром - через 15 минут
    for (uint8_t i = 0; i < 12; i++)
    {
        oled.dot(21, 3 + i * 5);
        oled.dot(41, 3 + i * 5);
        oled.dot(61, 3 + i * 5);
        oled.dot(81, 3 + i * 5);
    }

    // рамка вокруг графика
    oled.rect(0, 0, 100, 63, OLED_STROKE);

    // слева на графике минимум и максимум
    oled.setScale(1);
    oled.invertText(false);
    oled.setCursorXY(0, 0); // oled display
    oled.print(measures[display_sensor_type].max);
    oled.setCursorXY(0, 64 - 8); // oled display
    oled.print(measures[display_sensor_type].min);
}

void loopOLED()
{
    EVERY_N_SECONDS(INTERVAL_READ_SENSOR)
    {
        if (display_mode == DISPLAY_SENSORS)
        {
            // каждую секунду выводить значения в режиме больших цифр
            oled.clear();
            print_big_measures();
            oled.update();
        }
        if ((display_mode == DISPLAY_PLOT_TEMP_1) || (display_mode == DISPLAY_PLOT_TEMP_2))
        {
            // на графике обновить маленькие цифры - текущие измерения
            print_small_measures();
            oled.update();
        }
    }

    // добавить измерение в данные для графика
    EVERY_N_SECONDS(INTERVAL_UPDATE_MEASURES)
    {
        // добавляет текущее измерение, усредненное с предыдущим в историю
        update_measure_data();
    }

    // переключить большие цифры или график
    EVERY_N_SECONDS(INTERVAL_CHANGE_DISPLAY)
    {
        // выбираем следующий режим по индексу из списка
        // индекс измениться, когда снова сюда попадем
        display_mode_index = (display_mode_index + 1) % display_mode_count;
        display_mode = display_mode_list[display_mode_index];
        display_sensor_type = (display_mode == DISPLAY_PLOT_TEMP_1) ? _TEMP_1 : _TEMP_2;
        if ((display_mode == DISPLAY_PLOT_TEMP_1) || (display_mode == DISPLAY_PLOT_TEMP_2))
        {
            // если режим с графиками, то очистить экран и нарисовать график и маленькие цифры
            prepare_display_samples(display_sensor_type);
            oled.clear();
            print_small_measures();
            draw_bars();
            oled.update();
        }
    }
}

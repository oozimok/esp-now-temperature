#ifndef BARS_H
#define BARS_H

#include <Arduino.h>
#include <stdint.h>
#include <sensor.h>

// на экране 50 отсчетов через пиксель - всего 100 пикселей
// отсчеты через 60 секунд, всего 50 минут
#define TOTAL_MEASURES 50

// история измерений
struct SensorsMeasures
{
    // позиция записи в data
    uint16_t data_index;

    // минимум в data
    uint16_t min;

    // максимум в data
    uint16_t max;

    // текущее значение температуры
    uint16_t current;

    // текущее значение батареи
    uint16_t battery;

    // текущее значение current усреднится с предыдущим input
    // потом это значение попадет в data
    uint16_t input;

    // измерения от датчика
    uint16_t data[TOTAL_MEASURES];
};

// тип данных. используется как индекс в массиве measures[2]
#define _TEMP_1 1
#define _TEMP_2 2

// Данные измерений для главного модуля
extern SensorsMeasures measures[CONFIG_UNITS+1];
// данные для графика
extern uint8_t samples[TOTAL_MEASURES];

// инициализация массивов и переменных
void init_measures();

// добавление в input новых отсчетов
// если прошли полностью, то вычислить среднее
// и записать в data
void add_new_measure(uint8_t index, sensor_data_t bufSensorData);

// обновить данные в массиве для отображения
void update_measure_data();

// подготовить отсчеты для графика
void prepare_display_samples(uint8_t x);

#endif

#include "bars.h"

SensorsMeasures measures[UNITS+1];

// данные для графика
uint8_t samples[TOTAL_MEASURES];

void init_measures()
{
    for (uint8_t x = 1; x <= 2; x++)
    {
        for (uint8_t i = 0; i < TOTAL_MEASURES; i++)
        {
            measures[x].data[i] = 0;
        }
        measures[x].input = 0;
        measures[x].data_index = 0;
        measures[x].min = 0;
        measures[x].max = 0;
        measures[x].current = 0;
        measures[x].battery = 0;
    }

    for (uint8_t i = 0; i < TOTAL_MEASURES; i++)
    {
        samples[i] = 0;
    }
}

// заносит значение из input в массив истории
void update_measure_data()
{
    for (uint8_t x = 1; x <= 2; x++)
    {
        uint8_t i = measures[x].data_index;
        measures[x].data[i] = measures[x].input;
        measures[x].data_index = (i + 1) % TOTAL_MEASURES;
    }
}

// принимает текущие измерения и усредняет с предыдущими значениями
void add_new_measure(uint8_t index, sensor_data_t bufSensorData)
{
    uint16_t t = (bufSensorData.temp == 0) ? 1 : bufSensorData.temp;
    uint16_t b = (bufSensorData.Vbat == 0) ? 1 : bufSensorData.Vbat;

    t = (t > 300) ? 300 : t;
    b = (b > 100) ? 100 : b;

    measures[index].current = t;
    measures[index].battery = b;
    measures[index].input = (measures[index].input >> 1) + (t >> 1);
}

void prepare_display_samples(uint8_t x)
{
    uint16_t mn = measures[x].data[0];
    uint16_t mx = measures[x].data[0];
    uint16_t H = 60;
    for (uint8_t i = 0; i < TOTAL_MEASURES; i++)
    {
        uint16_t a = measures[x].data[i];
        if (a > 0)
        {
            mx = (a > mx) ? a : mx;
            mn = (a < mn) ? a : mn;
        }
    }

    if ((x == _TEMP_1) || (x == _TEMP_2))
    {
        // окгруление до большей сотни
        mx = ((mx / 100) + 1) * 100;
        mn = (mn < 200) ? 0 : 100;
    }

    measures[x].min = mn;
    measures[x].max = mx;

    float k = 0;
    if ((mx - mn) < 10)
    {
        k = float(H) / float(mx - mn + 20);
    }
    else
    {
        k = float(H) / float(mx - mn);
    }

    uint8_t sample_index = 0;

    for (uint8_t i = measures[x].data_index; i < TOTAL_MEASURES; i++)
    {
        if (measures[x].data[i] == 0)
        {
            samples[sample_index] = 0;
        }
        else
        {
            uint8_t v = (uint8_t)(float((measures[x].data[i] - mn) * k));
            samples[sample_index] = (v == 0) ? 1 : v;
        }
        sample_index++;
    }
    for (uint8_t i = 0; i < measures[x].data_index; i++)
    {
        if (measures[x].data[i] == 0)
        {
            samples[sample_index] = 0;
        }
        else
        {
            uint8_t v = (uint8_t)(float((measures[x].data[i] - mn) * k));
            samples[sample_index] = (v == 0) ? 1 : v;
        }
        sample_index++;
    }
}

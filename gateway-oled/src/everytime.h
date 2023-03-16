#pragma once

#include <Arduino.h>
#include <stdint.h>

#define GET_MILLIS millis

// читать датчик каждую секунду (написано, что так ему лучше)
#define INTERVAL_READ_SENSOR 1

// обновить данные для графиков, там 50 значений
// если записывать через 30 (60) секунд, то на экране будет 25 минут (50 минут)
// сетка на экране через 5 минут или 10 минут
#define INTERVAL_UPDATE_MEASURES 30

// Выводить графики или большие цифры через 10 секунд
#define INTERVAL_CHANGE_DISPLAY 10

static inline uint16_t div1024_32_16(uint32_t in32)
{
    uint16_t out16 = (in32 >> 10) & 0xFFFF;
    return out16;
}

static inline uint16_t bseconds16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t s16;
    s16 = div1024_32_16(ms);
    return s16;
}

static inline uint16_t seconds16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t s16;
    s16 = ms / 1000;
    return s16;
}

static inline uint16_t minutes16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t m16;
    m16 = (ms / (60000L)) & 0xFFFF;
    return m16;
}

/// Return the current hours since boot in an 8-bit value.  Used as part of the
/// "every N time-periods" mechanism
static inline uint8_t hours8()
{
    uint32_t ms = GET_MILLIS();
    uint8_t h8;
    h8 = (ms / (3600000L)) & 0xFF;
    return h8;
}

template<typename timeType, timeType(*timeGetter)()>
class CEveryNTimePeriods
{
    public:
        timeType mPrevTrigger;
        timeType mPeriod;

        CEveryNTimePeriods()
        {
            reset();
            mPeriod = 1;
        };
        CEveryNTimePeriods(timeType period)
        {
            reset();
            setPeriod(period);
        };
        void setPeriod(timeType period)
        {
            mPeriod = period;
        };
        timeType getTime()
        {
            return (timeType)(timeGetter());
        };
        timeType getPeriod()
        {
            return mPeriod;
        };
        timeType getElapsed()
        {
            return getTime() - mPrevTrigger;
        }
        timeType getRemaining()
        {
            return mPeriod - getElapsed();
        }
        timeType getLastTriggerTime()
        {
            return mPrevTrigger;
        }
        bool ready()
        {
            bool isReady = (getElapsed() >= mPeriod);
            if (isReady)
            {
                reset();
            }
            return isReady;
        }
        void reset()
        {
            mPrevTrigger = getTime();
        };
        void trigger()
        {
            mPrevTrigger = getTime() - mPeriod;
        };

        operator bool()
        {
            return ready();
        }
};
typedef CEveryNTimePeriods<uint16_t, seconds16> CEveryNSeconds;
typedef CEveryNTimePeriods<uint16_t, bseconds16> CEveryNBSeconds;
typedef CEveryNTimePeriods<unsigned long, millis> CEveryNMillis;
typedef CEveryNTimePeriods<uint16_t, minutes16> CEveryNMinutes;
typedef CEveryNTimePeriods<uint8_t, hours8> CEveryNHours;

#define CONCAT_HELPER( x, y ) x##y
#define CONCAT_MACRO( x, y ) CONCAT_HELPER( x, y )
#define EVERY_N_MILLIS(N) EVERY_N_MILLIS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)
#define EVERY_N_MILLIS_I(NAME,N) static CEveryNMillis NAME(N); if( NAME )
#define EVERY_N_SECONDS(N) EVERY_N_SECONDS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)
#define EVERY_N_SECONDS_I(NAME,N) static CEveryNSeconds NAME(N); if( NAME )
#define EVERY_N_BSECONDS(N) EVERY_N_BSECONDS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)
#define EVERY_N_BSECONDS_I(NAME,N) static CEveryNBSeconds NAME(N); if( NAME )
#define EVERY_N_MINUTES(N) EVERY_N_MINUTES_I(CONCAT_MACRO(PER, __COUNTER__ ),N)
#define EVERY_N_MINUTES_I(NAME,N) static CEveryNMinutes NAME(N); if( NAME )
#define EVERY_N_HOURS(N) EVERY_N_HOURS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)
#define EVERY_N_HOURS_I(NAME,N) static CEveryNHours NAME(N); if( NAME )

#define CEveryNMilliseconds CEveryNMillis
#define EVERY_N_MILLISECONDS(N) EVERY_N_MILLIS(N)
#define EVERY_N_MILLISECONDS_I(NAME,N) EVERY_N_MILLIS_I(NAME,N)

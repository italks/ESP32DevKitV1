#ifndef __MAIN_H
#define __MAIN_H

typedef enum
{
    SUNNY = 0,
    CLOUDY = 1,
    SOMBER = 2,
    LIGHT_RAIN = 3,
    MIDDLE_RAIN = 4,
    HEAVY_RAIN = 5

}WeatherTypedef;




typedef struct 
{
    int temp;
    unsigned int humi;
    unsigned int air;
    int MAXtemp;
    unsigned int MAXhumi;
    unsigned int MAXair;
    WeatherTypedef weather;
    int netMinTemp;
    int netMaxTemp;
    unsigned int year;
    unsigned char month;
    unsigned char day;
    unsigned char weekdays;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
    unsigned long int fansNum;
    unsigned char menuNum;
}SystemType;












#endif
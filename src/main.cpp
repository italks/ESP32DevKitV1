/**************************************************************          
项目：ESP32温湿度天气系统
作者：化作尘
日期：2021年11月18日

屏幕接口对应:        
                GND（电源负极）----------GND  （输出负极）
                VCC（电源正极）----------3V3  （输出3.3v正极）
                SCL（IIC时钟线）-----------D22   板子自带的I2c
                SDA（IIC数据线)-----------D21   板子自带的I2c
DHT11接口对应:            
                GND(4脚)---------GND    （负极）
                VCC（1脚）------------3V3   （3.3v正极）
                DATA（2脚）-----------D5   （程序中可自行设定管脚）
                空脚（3脚）------------不接
**************************************************************/                                

#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <Arduino.h>
#include <font.h>
#include <main.h>
#include <stdio.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <iostream>
#include <MQ135.h>
using std::string;


//时间有关变量
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

struct tm timeinfo;
String currentTime = "";
String currentDay = "";


//wifi
// #define WIFINAME "2220"
// #define WIFIPASWD "13825092946"

// #define WIFINAME "HUAZUOCHEN"
// #define WIFINAME2 "zhoulizhi"
// #define WIFIPASWD2 "12345678"
// #define WIFINAME2 "Redmi_7BDF"
// #define WIFIPASWD2 "RjW!I05h@MPI"
#define WIFINAME2 "AndroidDeveloper"
#define WIFIPASWD2 "jGHNq9wsMx8S4ejd"
//蓝牙
BluetoothSerial SerialBT;
#define BLUETOOTH_NAME "ESP32DevKitV1"

//按键
#define KEY_IO 34

//页数
#define PageNum 4
//系统信息结构体
SystemType System;
//WIFI相关
boolean isNTPConnected = false;
static const char ntpServerName[] = "ntp1.aliyun.com"; //NTP服务器，阿里云
#define TIME_URL "https://api.uukit.com/time"	//api接口

int timeZone = 9;                                      //时区，北京时间为+8
//OLED显示相关
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
//DHT11相关
#define DHT11_PIN 5
DHT dht11;
//MQ135相关
#define MQ135_PIN 33
//JOSN相关
DynamicJsonDocument doc(2048);
//粉丝数
// bilibili api: follower
// String UID = "441086905";
 String UID = "13536275";

String followerUrl = "http://api.bilibili.com/x/relation/stat?vmid=" + UID; // 粉丝数
//天气API
String City = "石家庄市";
//String WeatherAPI = "http://wthrcdn.etouch.cn/weather_mini?city=" + City;
//String WeatherAPI = "http://www.weather.com.cn/data/cityinfo/101280101.html";
String WeatherAPI = "http://t.weather.itboy.net/api/weather/city/101090101";

int disFlag = 1;
void WiFi_Connect();
void page1(void);
void page2(void);
void page3(void);
void page4(void);
void DisPlayTack(void);
void saveConfig();
void loadConfig();
void getBiliBiliFollower(void);
void getWeather(void);
void GetBetTime(void);
void Send_DHTW(void);
void Del_SerialBTVal(void);
void SysParaInit();
void GetDHT_MQ135();
void keyscan();





//初始化代码
void setup(){
    //初始化OLED显示屏
    u8g2.setI2CAddress(0x3C*2);
    u8g2.begin();

    //串口初始化
    Serial.begin(9600);
    u8g2.enableUTF8Print();

    //初始化DHT11
    dht11.setup(DHT11_PIN,DHT::DHT_MODEL_t::DHT11);
 
    //mq-135  ADC初始化
    adcAttachPin(MQ135_PIN);
    analogReadResolution(4);
    analogSetWidth(9);

    //按键相关
    pinMode(KEY_IO, INPUT_PULLUP);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    
    //连接WIFI
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_unifont_t_chinese2);//温湿度
    Serial.print("Connecting.. ");
    u8g2.print("Connect to WIFI");
    u8g2.setCursor(0, 14+16*0);
    u8g2.print("Connect to WIFI");
    u8g2.setCursor(0, 14+16*1);
    u8g2.print(WIFINAME2);
    u8g2.sendBuffer();
    WiFi_Connect();
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.status());
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.macAddress());

    //蓝牙初始化
    SerialBT.begin(BLUETOOTH_NAME); // 如果没有参数传入则默认是蓝牙名称是: "ESP32"
    SerialBT.setPin("1234");   // 蓝牙连接的配对码
    Serial.printf("蓝牙初始化成功，等待连接\r\nBT initial ok and ready to pair. \r\n");

    
    SysParaInit();//系统参数初始化
}

void loop(){
    GetBetTime();//获取时间
    getBiliBiliFollower();//获取粉丝数
    getWeather();//获取天气
    while(1){
        // keyscan();//按键扫描
        // DisPlayTack();//刷新显示
        page1();
        Send_DHTW();//发送温湿度值
        Del_SerialBTVal();//处理蓝牙数据
    }
    
}



//按键扫描
void keyscan()
{
		static int key_up=1;  //按键松开标志位，
		if(digitalRead(KEY_IO)==0 && key_up==1)//判断按键是否按下
		{
			delay(10);//延时消抖
			key_up=0;//按下状态，（防止循环执行按键控制程序）
			if(digitalRead(KEY_IO)==0)    //再次判断，排除是松开状态或外界杂波干扰
			{	
                System.menuNum++;
                if( System.menuNum == PageNum+1) System.menuNum = 1;
                
                Serial.println(System.menuNum);
                disFlag = 1;//更新显示
			}
		}
		else if(digitalRead(KEY_IO)==1)	key_up=1;
}
//获取温湿度
void GetDHT_MQ135()
{
    int temp,humi,adcVal;
    temp = dht11.getTemperature();
    humi = dht11.getHumidity();
    if(temp<100 && humi>0 && humi<101){
        System.temp = temp;
        System.humi = humi;
    }
    // adcVal = analogRead(33);
    // float volt = adcVal*3.3/4096;
	// System.air = (int )pow((3.4880*10*volt)/(5-volt),(1.0/0.3203));
    //初始化MQ135
    // MQ135 gasSensor = MQ135(MQ135_PIN);
    // System.air = gasSensor.getRZero();
    
    MQ135 gasSensor = MQ135(MQ135_PIN);
    System.air = gasSensor.getRZero();
    //3958344704
    //ppm
    //8476

    // Serial.printf("adcVal = %d\r\n",adcVal);
    // Serial.printf("volt = %f\r\n",volt);
    // Serial.printf("air = %d\r\n",System.air);
    // Serial.printf("温度=%d\r\n",temp);
    // Serial.printf("湿度=%d\r\n",humi);
}
//系统参数初始化
void SysParaInit()
{
    //初始化信息结构体
    memset(&System,0,sizeof(System));//清空结构体
    System.menuNum = 1;
    System.MAXtemp = 50;
    System.MAXhumi = 100;
    System.MAXair = 2000;
}
void Send_DHTW(void)
{
    static int cnt=0;
    cnt++;
    
    GetDHT_MQ135();
    if(cnt==100)    
    SerialBT.printf("TEMPVAL:%d\r\n",System.temp);
    else if(cnt==200)  
    {
        SerialBT.printf("HUMIVAL:%d\r\n",System.humi);
    }else if(cnt==300)  
    {
        SerialBT.printf("AIRVAL:%d\r\n",System.air);
        cnt = 0;
    }
}
void Del_SerialBTVal(void)
{
    string readBuf;
    String SreadBuf;
        while (SerialBT.available())
        {
            readBuf+=SerialBT.read();
            delay(1);
        }
        const char *p = readBuf.data();
        SreadBuf = readBuf.data();
        if(readBuf.length()>0)
        {
            Serial.print(p);//SerialBT.read());
            readBuf.clear();

            if(SreadBuf.startsWith("SET_TEMPVAL:"))
            {
                SreadBuf.remove(0,12);
                System.MAXtemp = SreadBuf.toInt();
                u8g2.clearBuffer();

                u8g2.drawXBM(0, 16,  16, 16, wen1);
                u8g2.setCursor(16,  14+16*1);
                u8g2.print("度");
                u8g2.drawXBM(32, 16,  16, 16, maohao);
                u8g2.drawXBM(112, 16,  16, 16, sheshidu);

                u8g2.setCursor(77, 14+16*1);
                u8g2.println(System.MAXtemp);
                u8g2.sendBuffer();
                delay(1000);
                disFlag=1;
            }else if(SreadBuf.startsWith("SET_HUMIVAL:"))
            {
                SreadBuf.remove(0,12);
                System.MAXhumi = SreadBuf.toInt();
                u8g2.clearBuffer();
                u8g2.drawXBM(0, 16,  16, 16, shi1);
                u8g2.setCursor(16,  14+16*1);
                u8g2.print("度");
                u8g2.drawXBM(32, 16,  16, 16, maohao);
                u8g2.drawXBM(112, 16,  16, 16, baifenhao);

                u8g2.setCursor(77, 14+16*1);
                u8g2.println(System.MAXhumi);
                u8g2.sendBuffer();
                delay(1000);
                disFlag=1;
            }else if(SreadBuf.startsWith("SET_AIRVAL:"))
            {
                SreadBuf.remove(0,11);
                System.MAXair = SreadBuf.toInt();
                u8g2.clearBuffer();

                u8g2.drawXBM(0, 16,  16, 16, kong1);
                u8g2.drawXBM(16, 16,  16, 16, qi4);
                u8g2.drawXBM(32, 16,  16, 16, zhi4);
                u8g2.drawXBM(48, 16,  16, 16, liang4);
                u8g2.drawXBM(64, 16,  16, 16, maohao);
                u8g2.setCursor(104,  14+16*1);
                u8g2.print("ppm");

                u8g2.setCursor(77, 14+16*1);
                u8g2.println(System.MAXair);
                u8g2.sendBuffer();
                delay(1000);
                disFlag=1;
            }
            Serial.printf("%d %d %d\r\n", System.MAXtemp,System.MAXhumi,System.MAXair);
        }
}
int years, months, days, hours, minutes, seconds, weekdays;
//获取本地时间
void Gettimedate(void)
{
    int lastsec=61;
    currentTime.clear();
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        GetBetTime();//获取时间
        return;
    }
    //Serial.println(&timeinfo, "%F %T %A"); // 格式化输出
    if(lastsec != timeinfo.tm_sec)
    {
        lastsec = timeinfo.tm_sec;
        years = System.year = timeinfo.tm_year+1900;
        months = System.month = timeinfo.tm_mon+1;
        days = System.day = timeinfo.tm_mday;
        hours = System.hour = timeinfo.tm_hour;
        minutes = System.minute = timeinfo.tm_min;
        seconds = System.second = timeinfo.tm_sec;
        weekdays = System.weekdays = timeinfo.tm_wday+1;
  
        currentTime.clear();
        if (hours < 10)
            currentTime += 0;
        currentTime += hours;
        currentTime += ":";
        if (minutes < 10)
            currentTime += 0;
        currentTime += minutes;
        currentTime += ":";
        if (seconds < 10)
            currentTime += 0;
        currentTime += seconds;
        currentDay.clear();
        currentDay += years;
        currentDay += "/";
        if (months < 10)
            currentDay += 0;
        currentDay += months;
        currentDay += "/";
        if (days < 10)
            currentDay += 0;
        currentDay += days;
    }
}
// 显示page1的 各项参数到屏幕
String lastcurrentTime;
void page1(void) {

    static int lastTemp,lastHumi,lastAir;
    GetDHT_MQ135();
    if(currentTime != lastcurrentTime)
    {
        disFlag = 1;
        lastcurrentTime = currentTime;
    }

    if(System.temp!=lastTemp || System.humi!=lastHumi || disFlag==1 )//|| lastAir!=System.air)
    {
        disFlag = 0;
        
        lastTemp = System.temp;
        lastHumi = System.humi;
        u8g2.clearBuffer();

        // u8g2.setFont(u8g2_font_timR12_tr);
        // u8g2.setCursor(48, 14);
        // u8g2.print(currentDay);


        u8g2.setFont(u8g2_font_unifont_t_chinese2);//温湿度
        u8g2.drawXBM(48, 0,  16, 16, wen1);
        u8g2.drawXBM(64, 0,  16, 16, shi1);
        u8g2.drawXBM(111, 0,  16, 16, wifi);
        u8g2.setCursor(80, 14+16*0);
        u8g2.print("度");

        u8g2.drawXBM(0, 16,  16, 16, wen1);
        u8g2.setCursor(16,  14+16*1);
        u8g2.print("度");
        u8g2.drawXBM(32, 16,  16, 16, maohao);
        u8g2.drawXBM(112, 16,  16, 16, sheshidu);

        u8g2.drawXBM(0, 32,  16, 16, shi1);
        u8g2.setCursor(16,  14+16*2);
        u8g2.print("度");
        u8g2.drawXBM(32, 32,  16, 16, maohao);
        u8g2.drawXBM(112, 32,  16, 16, baifenhao);

        u8g2.drawXBM(0, 48,  16, 16, kong1);
        u8g2.drawXBM(16, 48,  16, 16, qi4);
        // u8g2.drawXBM(32, 48,  16, 16, zhi4);
        // u8g2.drawXBM(48, 48,  16, 16, liang4);
        u8g2.drawXBM(32, 48,  16, 16, maohao);

        u8g2.setCursor(80,  14+16*1);
        u8g2.println(System.temp);
        u8g2.setCursor(80,  14+16*2);
        u8g2.println(System.humi);
        u8g2.setCursor(40,  14+16*3);
        u8g2.println(System.air);
        // u8g2.setCursor(104,  14+16*3);
        // u8g2.print("ppm");
        
    }
    
    
        static int cnt = 0;
        cnt++;
        if(System.temp > System.MAXtemp)
        {
            if(cnt<10)u8g2.drawXBM(0, 0,  16, 16, temp);
            else if(cnt<20)u8g2.drawXBM(0, 0,  16, 16, none);
            else cnt=0;
            //u8g2.sendBuffer();
        }else u8g2.drawXBM(0, 0,  16, 16, none);
        if(System.humi > System.MAXhumi)
        {
            if(cnt<10)u8g2.drawXBM(16, 0,  16, 16, humi);
            else if(cnt<20)u8g2.drawXBM(16, 0,  16, 16, none);
            else cnt=0;
            //u8g2.sendBuffer();
        }else u8g2.drawXBM(16, 0,  16, 16, none);
        if(System.air > System.MAXair)
        {
            if(cnt<10)u8g2.drawXBM(32, 0,  16, 16, airimg);
            else if(cnt<20)u8g2.drawXBM(32, 0,  16, 16, none);
            else cnt=0;
            //u8g2.sendBuffer();
        }else u8g2.drawXBM(32, 0,  16, 16, none);
        u8g2.sendBuffer();
    delay(1);

    
}
void page2(void) {
    static WeatherTypedef lastWeather;
    static int lastmintemp,lastmaxtemp;

    //Serial.println(System.netMinTemp);
    if(lastWeather!=System.weather || System.netMinTemp!=lastmintemp || System.netMaxTemp!=lastmaxtemp  || disFlag==1)
    {
        disFlag = 0;
        lastWeather = System.weather;
        u8g2.clearBuffer();

        // u8g2.setFont(u8g2_font_timR12_tr);
        // u8g2.setCursor(0, 14);
        // u8g2.print(currentTime);

        u8g2.setFont(u8g2_font_unifont_t_chinese3);
        u8g2.setCursor(48, 14+16*0);
        u8g2.print("天气");
        u8g2.setCursor(48, 14+16*1);
        u8g2.print("石家庄");
        //u8g2.drawXBM(70, 16*1, 16, 16, guang3);
        // u8g2.drawXBM(86, 16*1, 16, 16, zhou1);
        u8g2.setCursor(70, 14+16*2);

        switch(System.weather)
        {
            case SUNNY:
            //System.weather = CLOUDY;
            u8g2.drawXBM(70, 16*2, 16, 16, qing2);
            u8g2.drawXBM(10, 32,  32, 32, qing32x32);
            break;
            case CLOUDY:
            //System.weather = SOMBER;
            u8g2.print("多云");
            u8g2.drawXBM(10, 32,  32, 32, duoyun32x32);
            break;
            case SOMBER:
            //System.weather = LIGHT_RAIN;
            u8g2.drawXBM(70, 16*2, 16, 16, yin1);
            u8g2.drawXBM(10, 32,  32, 32, yin32x32);
            break;
            case LIGHT_RAIN:
            //System.weather = MIDDLE_RAIN;
            u8g2.drawXBM(10, 32,  32, 32, xiaoyu32x32);
            u8g2.print("小");
            u8g2.drawXBM(70+16, 16*2, 16, 16, yu3);
            break;
            case MIDDLE_RAIN:
            //System.weather = HEAVY_RAIN;
            u8g2.drawXBM(10, 32,  32, 32, zhongyu32x32);
            u8g2.print("中");
            u8g2.drawXBM(70+16, 16*2, 16, 16, yu3);
            break;
            case HEAVY_RAIN:
            //System.weather = SUNNY;
            u8g2.drawXBM(10, 32,  32, 32, dayu32x32);
            u8g2.print("大");
            u8g2.drawXBM(70+16, 16*2, 16, 16, yu3);
            break;
            default:
            u8g2.print("多云");
            u8g2.drawXBM(10, 32,  32, 32, duoyun32x32);
            break;

        }

        u8g2.drawXBM(111, 0,  16, 16, wifi);
        u8g2.setCursor(70, 14+16*3);
        u8g2.println(System.netMinTemp);
        u8g2.print("~");
        u8g2.println(System.netMaxTemp);
        u8g2.drawXBM(112, 16*3, 16, 16, sheshidu);
        u8g2.sendBuffer();
    }
    
    delay(10);
}
void page3(void) {
    int lastsec=61;
    currentTime.clear();
    Gettimedate();
    //Serial.println(&timeinfo, "%F %T %A"); // 格式化输出
    if(lastsec != timeinfo.tm_sec)
    {
        u8g2.clearBuffer();
        //u8g2.setCursor(48, 14+16*0);
        //u8g2.print("时间");
        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        u8g2.setCursor(0, 14);
        
        if(WiFi.isConnected())
        u8g2.drawXBM(111, 0,  16, 16, wifi);
        else
        u8g2.drawXBM(111, 0, 16, 16, XX);
            // u8g2.print("无网络!"); //如果上次对时失败，则会显示无网络
        //String currentTime = "";
        

        u8g2.setFont(u8g2_font_timR12_tr);
        u8g2.setCursor(0, 14);
        u8g2.print(currentTime);
        // u8g2.setCursor(80, 14);
        // u8g2.setFont(u8g2_font_timR12_tr);
        // u8g2.print(currentDay);

        u8g2.setFont(u8g2_font_logisoso24_tr);
        u8g2.setCursor(0, 44);
        u8g2.print(currentTime);
        u8g2.setCursor(0, 61);
        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        u8g2.print(currentDay);
        u8g2.drawXBM(80, 48, 16, 16, xing);
        u8g2.setCursor(95, 62);
        u8g2.print("期");
        if (weekdays == 1)
            u8g2.print("日");
        else if (weekdays == 2)
            u8g2.print("一");
        else if (weekdays == 3)
            u8g2.print("二");
        else if (weekdays == 4)
            u8g2.print("三");
        else if (weekdays == 5)
            u8g2.print("四");
        else if (weekdays == 6)
            u8g2.print("五");
        else if (weekdays == 7)
            u8g2.drawXBM(111, 49, 16, 16, liu);

        
        u8g2.drawXBM(111, 0,  16, 16, wifi);
        u8g2.sendBuffer();
    }
    
    delay(1);
}
void page4(void) {
    static long last_Num = 1;

    disFlag = 1;
    if(last_Num != System.fansNum  || disFlag==1 )
    {
        disFlag = 0;
        last_Num = System.fansNum;

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        u8g2.setCursor(0, 14);
        u8g2.print("Versison: V1.0");        
        
        // u8g2.setCursor(64, 14);
        // u8g2.print(currentDay);

        

        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        // u8g2.setCursor(0, 14+16*1);
        u8g2.drawUTF8(0,14+16*1,"blbl:周立志");
        u8g2.setCursor(16*4, 14+16*1);

        u8g2.drawXBM(16*4, 16, 16, 16, jie2);
        u8g2.drawXBM(16*0, 16*2, 16, 16, fen3);
        u8g2.drawXBM(16*1, 16*2, 16, 16, si1);
        u8g2.setCursor(16*2, 14+16*2);
        u8g2.print("数:");
        u8g2.setCursor(16*4 ,14+16*2);
        u8g2.println(System.fansNum);
        u8g2.setCursor(0, 14+16*3);
        u8g2.print("www.italks.top");
        u8g2.sendBuffer();
    }
    delay(10);
}
//显示任务
void DisPlayTack(void)
{
    switch(System.menuNum)
    {
    case 1:
        page1();
    break;
    case 2:
        page3();
    break;
    case 3:
        page2();
    break;
    case 4:
        page4();
    break;
    }
}
//	WiFi的初始化和连接
void WiFi_Connect()
{
    // int cnt = 40;
	// WiFi.begin(WIFINAME,WIFIPASWD);
	// while (WiFi.status() != WL_CONNECTED && cnt--)
	// { //这里是阻塞程序，直到连接成功
	// 	delay(300);
	// 	Serial.print(".");
	// }
    // WiFi.disconnect();
    int cnt = 40;
	WiFi.begin(WIFINAME2,WIFIPASWD2);
    while (WiFi.status() != WL_CONNECTED)
	{ //这里是阻塞程序，直到连接成功
		delay(2500);
        WiFi.begin(WIFINAME2,WIFIPASWD2);
		Serial.print(".");
	}
    Serial.print("已连接.");
}
//存储配置到"EEPROM"
void saveConfig()
{ 
    Serial.println("save config");
    EEPROM.begin(sizeof(System));
    uint8_t *p = (uint8_t *)(&System);
    for (uint i = 0; i < sizeof(System); i++)
    {
        EEPROM.write(i, *(p + i));
    }
    EEPROM.commit(); //此操作会消耗flash写入次数
}
//从"EEPROM"加载配置
void loadConfig()
{ 
    Serial.println("load config");
    EEPROM.begin(sizeof(System));
    uint8_t *p = (uint8_t *)(&System);
    for (uint i = 0; i < sizeof(System); i++)
    {
        *(p + i) = EEPROM.read(i);
    }
    timeZone = 8;//config.tz;
}
//获取粉丝数
void getBiliBiliFollower(void)
{
	HTTPClient http;
	http.begin(followerUrl); //HTTP begin
	int httpCode = http.GET();

	if (httpCode > 0)
	{
		// httpCode will be negative on error
		Serial.printf("HTTP Get Code: %d\r\n", httpCode);

		if (httpCode == HTTP_CODE_OK) // 收到正确的内容
		{
			String resBuff = http.getString();

			//	输出示例：{"mid":123456789,"following":226,"whisper":0,"black":0,"follower":867}}
			Serial.println(resBuff);

			//	使用ArduinoJson_6.x版本，具体请移步：https://github.com/bblanchon/ArduinoJson
			deserializeJson(doc, resBuff); //开始使用Json解析
			System.fansNum = doc["data"]["follower"];
			Serial.printf("Follers: %ld \r\n", System.fansNum);
		}
	}
	else
	{
		Serial.printf("HTTP Get Error: %s\n", http.errorToString(httpCode).c_str());
	}

	http.end();
}
//获取天气
void getWeather(void)
{
	HTTPClient http;
	http.begin(WeatherAPI); //HTTP begin
	int httpCode = http.GET();
    //long weather;
	if (httpCode > 0)
	{
		// httpCode will be negative on error
		Serial.printf("HTTP Get Code: %d\r\n", httpCode);

		if (httpCode == HTTP_CODE_OK) // 收到正确的内容
		{
			String resBuff = http.getString();
			//	输出示例：{"mid":123456789,"following":226,"whisper":0,"black":0,"follower":867}}
			Serial.println(resBuff);
				//使用ArduinoJson_6.x版本，具体请移步：https://github.com/bblanchon/ArduinoJson
			deserializeJson(doc, resBuff); //开始使用Json解析
            // get the JsonObject in the JsonDocument
            JsonObject root = doc.as<JsonObject>();
            JsonObject data = root["data"];
            JsonArray forecast = data["forecast"];
            
            // get the value in the JsonObject
            const char * type = forecast[0]["type"];
            const char * high = forecast[0]["high"];
            const char * low = forecast[0]["low"];
            char temp1buf[1024],temp2buf[1024],wearbuf[1024];

            Serial.println(type);
            Serial.println(high);
            Serial.println(low);

            strcpy(temp1buf,low);
            strcpy(temp2buf,high);
            strcpy(wearbuf,type);
            System.netMinTemp = atoi(temp1buf+6);
            System.netMaxTemp = atoi(temp2buf+6);

            
            Serial.println(System.netMinTemp);
            Serial.println(System.netMaxTemp);
            if(strncmp(wearbuf,"晴",2) == 0){
                System.weather = SUNNY;
            }else if (strncmp(wearbuf,"多云",4) == 0){
                System.weather = CLOUDY;
            }else if (strncmp(wearbuf,"阴",2) == 0){
                System.weather = SOMBER;
            }else if (strncmp(wearbuf,"小雨",4) == 0){
                System.weather = LIGHT_RAIN;
            }else if (strncmp(wearbuf,"阵雨",4) == 0){
                System.weather = LIGHT_RAIN;
            }else if (strncmp(wearbuf,"中雨",4) == 0){
                System.weather = MIDDLE_RAIN;
            }else if (strncmp(wearbuf,"大雨",4) == 0){
                System.weather = HEAVY_RAIN;
            }else if (strncmp(wearbuf,"暴雨",4) == 0){
                System.weather = HEAVY_RAIN;
            }
            Serial.printf("温度：%d-%d weather:%d\r\n",System.netMinTemp,System.netMaxTemp,System.weather);
		}
	}
	else
	{
		Serial.printf("HTTP Get Error: %s\n", http.errorToString(httpCode).c_str());
	}

	http.end();
}
//获取网络时间
void GetBetTime(void)
{
     HTTPClient http;
	http.begin(TIME_URL); //HTTP begin
	int httpCode = http.GET();
    //long weather;
	if (httpCode > 0)
	{
		// httpCode will be negative on error
		Serial.printf("HTTP Get Code: %d\r\n", httpCode);

		if (httpCode == HTTP_CODE_OK) // 收到正确的内容
		{
			String resBuff = http.getString();
			//	输出示例：{"mid":123456789,"following":226,"whisper":0,"black":0,"follower":867}}
			Serial.println(resBuff);
			//	使用ArduinoJson_6.x版本，具体请移步：https://github.com/bblanchon/ArduinoJson
			deserializeJson(doc, resBuff); //开始使用Json解析
            // get the JsonObject in the JsonDocument
            JsonObject root = doc.as<JsonObject>();
            JsonObject time = root["data"];
            // get the value in the JsonObject
            const char * time1 = time["gmt"];
           // char temp1buf[16],temp2buf[16],wearbuf[64];
            //strcpy(temp1buf,time1);
            System.year = atoi(time1);
            System.month = atoi(time1+5);
            System.day = atoi(time1+8);
            System.hour = atoi(time1+11);
            System.minute = atoi(time1+14);
            System.second = atoi(time1+17);

            Serial.printf("日期：%d-%d-%d\r\n",System.year ,System.month,System.day);
		}
	}
	else
	{
		Serial.printf("HTTP Get Error: %s\n", http.errorToString(httpCode).c_str());
	}

	http.end();       
}
           

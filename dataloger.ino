//#include <SoftwareSerial.h>
#include <stdlib.h>
//#include <DHT.h>
#include <string.h>
#include <Wire.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750FVI.h>
#include <LSD.h>
#include <LBT.h>
#include <LBTServer.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LWiFiUDP.h>
#include <LDateTime.h>
#include <LGPS.h>
#include <HttpClient.h>
//#define MAX_STRING_LEN 20
//#define DHTTYPE DHT11
#define commandSize 10
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DEVICEID "DiEUcEpe" // Input your deviceId
#define DEVICEKEY "dqvrthDKg0PNbxF2" // Input your deviceKey
#define SITE_URL "api.mediatek.com"


BH1750FVI LightSensor;
String sensorData[10];
/*
   存放 sensor 資料
   0:溫度
   1:濕度
   2:光照度
   3:土壤濕度
   4:風向
   5:風速
   6:最大風速
   7:1小時雨量
   8:24小時雨量
   9:氣壓
*/
float sensorValue[10];
/*誤差校正比例參數
   0:溫度
   1:濕度
   2:光照度
   3:土壤濕度
   4:風速
   5:24小時雨量
   6:氣壓
*/
float correctValue[7];
char outstr[20];
float temp;
/*系統參數設定字串*/
unsigned int configs[10];
//String senso
/*感應器下次取得時間設定字串*/
unsigned int sensor_time[5];
String upload_time = "";
boolean isBatch;
/*serial 命令*/
boolean commandComplete = false;
String commandStr;
String command[commandSize];
char databuffer[35];
boolean initsd;
boolean initWeb;
boolean gpsOn;
boolean initTime;
boolean initlcd;
bool isDebug;
bool bsaveData;
bool blcdOn;
/*藍燈亮*/
bool bblueOn;
/*綠燈亮*/
bool bgreenOn;

/*連到wifi*/
bool bWifi;

/*web server*/
char web_server[100];
int port = 80;
String postPage;
char btName[4] = "m01";
unsigned int dustPin = 2; //設定土壤溼度計
unsigned int analogValue = 0;
unsigned int blue_led = 4;
unsigned int gree_led = 3;
unsigned int lcdLastTime = 0;
char dataLog[] = "DATALOG.TXT";
char gpsLog[] = "GPSLOG.TXT";
//String msg;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 167 };
char wifi_name[20];
char wifi_passwd[20];
datetimeInfo tm;
gpsSentenceInfoStruct gpsInfo;
LFile gpsFile;
LFile dataFile;
LFile configFile;
/*紀錄執行歷程*/
LFile logFile;
LWiFiClient httpClient;
RTC_DS1307 rtc;


String readConfig(String configName) {
  if (!initsd) return "";
  String str = "";
  if (!configFile) {
    openConfigFile(true, FILE_READ);
  } else {
    configFile.seek(0);
  }
  if (configFile) {
    while (configFile.available()) {
      str = configFile.readStringUntil('\n');
      if (str.indexOf(configName) > -1) {
        int pos = str.indexOf("=");
        //printToSerial(str);
        if (pos > -1) {
          str = str.substring(pos + 1);
          str.trim();
          return str;
        }
      }
    }
    return str;
    //iniFile.close();

  }
  return "";
}


/*計算下次 sensor 取得資料時間*/
void sensor_next_time(int index) {
  LDateTime.getTime(&tm);
  unsigned int pos = 0;
  int interval = configs[index];
  if (interval > 0) {
    unsigned int hh = tm.hour + int(interval / 60);
    unsigned int mm = tm.min + (interval % 60);
    sensor_time[index] = hh * 60 + mm;
  } else {
    sensor_time[index] = 0;
  }
}

/*啟動 blue tooth */
bool initBluetooth() {
  String str = readConfig("btName");
  if (str != "") {
    str.toCharArray(btName, 3);
  }
 
  if (LBTServer.begin((uint8_t*)btName)) {
    printToSerial("BT server is started.", true);
    return true;
  } else {
    printToSerial("Fail to start BT.", true);
    return false;
  }
}


/*啟動 wifi*/
bool initWiFi() {
  LWiFi.end();
  LWiFi.begin();
  printToSerial("wifi name:", true);
  printToSerial((char*)wifi_name, true);
  printToSerial("wifi password:", true);
  printToSerial((char*)wifi_passwd, true);
  if (LWiFi.connectWPA(wifi_name, wifi_passwd) > 0) {
    printToSerial("wifi is connected", true);
    return true;
  } else {
    printToSerial("wifi connect failed", true);
    return false;
  }
}



/*連線到 mediatek cloud sandbox */
bool connectWCS() {
  bool returnvalue = true;
  unsigned int i = 0;
  while (!httpClient.connect(SITE_URL, 80))
  {
    printToSerial("Re-Connecting to WCS", false);
    delay(1000);
    i++;
    if (i >= 10) {
      returnvalue = false;
      break;
    }
  }
  delay(100);
  return returnvalue;
}

/*gps 開關*/
void gpsPower(bool on) {
  if (on) {
    LGPS.powerOn();
    gpsOn = true;
    gpsFile = LSD.open(gpsLog, FILE_WRITE);
    delay(3000);
    printToSerial("gps power on", true);
  } else {
    LGPS.powerOff();
    gpsOn = false;
    if (gpsFile) {
      gpsFile.flush();
      gpsFile.close();
    }
    printToSerial("gps power off", true);
  }
}

/*啟動光照度計*/
bool initLightSensor() {
  LightSensor.begin();
  LightSensor.SetAddress(0x23);
  LightSensor.SetMode(Continuous_H_resolution_Mode);
  return true;
}

/*啟動液晶顯示器*/
bool initLcd() {
  lcd.init();   // initialize the lcd
  lcd.init();
  lcd.backlight();
  initlcd = true;
  blcdOn = true;
  return true;
}

/*初始時鐘*/
bool initRTCTime() {
DateTime now = rtc.now();
tm.year = now.year();
tm.mon = now.month();
tm.day = now.day();;
tm.hour = now.hour();;
tm.min = now.minute();
tm.sec = now.second();
LDateTime.setTime(&tm);
}

/*
  bool intiTemperature() {
  dht.begin();
  }
*/

/*取回設定檔取樣時間間隔*/
int getDefaultInt(String str) {
  if (str != "") {
    return str.toInt();
  } else {
    return 10;
  }
}

/*取回設定檔校正值*/
float getDefaultCor(String str) {
  if (str != "") {
    return str.toFloat();
  } else {
    return 100;
  }

}


/*設定執行環境參數*/
void setenv() {
  Serial.println("call setenv");
  //printToSerial("call setenv", true);
  String str;
  str = readConfig("debug");
  if (str == "") {
    isDebug = true;
  }

  str = readConfig("temp");
  configs[0] = getDefaultInt(str);
  str = readConfig("temp_p");
  correctValue[0] = getDefaultCor(str);
  str = readConfig("humi");
  configs[1] = getDefaultInt(str);
  str = readConfig("humi_p");
  correctValue[1] = getDefaultCor(str);
  str = readConfig("illu");
  configs[2] = getDefaultInt(str);
  str = readConfig("illu_p");
  correctValue[2] = getDefaultCor(str);
  str = readConfig("soil");
  configs[3] = getDefaultInt(str);

  str = readConfig("soil_p");
  correctValue[3] = getDefaultCor(str);
  str = readConfig("aprs");
  configs[4] = getDefaultInt(str);
  str = readConfig("wind_p");
  correctValue[4] = getDefaultCor(str);
  str = readConfig("rain_p");
  correctValue[5] = getDefaultCor(str);
  str = readConfig("press_p");
  correctValue[6] = getDefaultCor(str);
  str = readConfig("batch");
  if (str!="") {
    configs[5] = str.toInt();
  }
  if (configs[5] == 1) {
    isBatch = true;
  } else {
    isBatch = false;
  }

  str = readConfig("wifi");
  if (str!="") {
    configs[7] = str.toInt();
  }
  str = readConfig("gprs");
  if (str!="") {
    configs[8] = str.toInt();
  }
  str = readConfig("url");
  if (str != "") {
    str.toCharArray(web_server, str.length());
  }
  str = readConfig("port");
  if (str!="") {
    port = str.toInt();
  }  
  if (port == 0) {
    port = 80;
  }

  str = readConfig("p_soil");
  if (str!="") {
    dustPin = str.toInt();
  }
  str = readConfig("page");
  if (str != "") {
    postPage = str;
  }
  str = readConfig("upload_time");
  if (str != "") {
    upload_time = str;
  }
  if (configs[7] == 1) {
    str = readConfig("wifi_name");
    str.toCharArray(wifi_name, 20);
    String str1 = readConfig("wifi_passwd");
    str1.toCharArray(wifi_passwd, 20);
    /*啟動 wifi*/
    //bWifi=initWiFi();
  }

  reset_next_time();

  Serial.println("end call setenv");

}

void reset_next_time() {
  unsigned int i = 0;
  for (i = 0; i <= 4; i++) {
    sensor_next_time(i);
  }
}
void setup() {
  // put your setup code here, to run once:
  pinMode(gree_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial.println("system start");
  rtc.begin();
  
  //setSyncProvider(RTC.get);
  //xbee.begin(9600);
  initLcd();
  /*
    while (!Serial) {
      ; // wait for serial port to connect. Needed for Leonardo only
     }
  */
  initsd = LSD.begin();
  if (!initsd) {
    printToSerial("SD card fail", true);
  } else {
    openLogFile(true, FILE_WRITE);
    setenv();
    /*若是啟用 debug 模式*/
    isDebug=false;
    
    if (isDebug) {
      while (!Serial) {
        ; // wait for serial port to connect. Needed for Leonardo only
      }
    }
    printToSerial("config read ok", true);
    initRTCTime();
    //printToSerial(web_server);
  }
  /*
    if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    } else {
    Serial.print("localIP:");
    Serial.println(Ethernet.localIP());
    initWeb = true;
    }
  */
  /*啟動藍芽*/
  initBluetooth();

  if (configs[7] == 1) {
    bWifi=initWiFi();
    //setSyncProvider(getNtpTime);
  }
  
  printToSerial("start Date:" + currentDate(false), true);
  printToSerial("start Time:" + currentTime(), true);
  digitalWrite(gree_led, HIGH);
  digitalWrite(blue_led, HIGH);

  lightLed(0);
}

void lightLed(unsigned int mode) {
  /*已起動*/
  if (mode == 0) {
    greenLed(true);
    blueLed(false);
  } else if (mode == 1) { //啟用 bluetooth
    greenLed(false);
    blueLed(true);
  } else if (mode == 2) { //資料傳輸中
    greenLed(true);
    blueLed(true);
  }
}

void loop() {
  /*
    if (commandComplete) {
    //執行命令
    doCommand();
    }
  */
  unsigned int minValue = currentMinValue();
  if (minValue == 0) {
    reset_next_time();
  }
  /*若lcd 開啟檢查是否須關閉*/
  if (blcdOn) {
    lcdon(minValue);
  }

  getRtData(minValue, false);
  if (bsaveData) {
    saveSensorData();
    delay(1000);
    bsaveData = false;
  }
  if (gpsOn) {
    LGPS.getData(&gpsInfo);
    saveGPSData();
  }
  /*
    轉成字串
    String str = (char*)buff;
  */
  if (LBTServer.connected())
  {
    lightLed(1);
    uint8_t buf[64];
    int bytesRead;
    //printToSerial("bluetooth connected");
    while (true)
    {
      bytesRead = LBTServer.readBytes(buf, 64);
      if (!bytesRead)
        break;
      buf[bytesRead] = 0;
      String str = (char*)buf;
      if (str.indexOf(";") > -1) {
        commandStr += str;
        commandComplete = true;
        break;
      } else {
        commandStr += str;
      }
    } // end of the while(true) portion
    if (commandComplete) {
      //printToSerial("do command");
      spliteString(commandStr);
      doCommand();
      commandComplete = false;
      commandStr = "";
    }


  } else {
    lightLed(0);
    commandStr = "";
    LBTServer.accept(5);
  }
  //printToSerial(currentTime(),false);  
  delay(1000);
}

void blueLed(bool bOn) {
  if (bblueOn == bOn) return;
  if (bOn) {
    digitalWrite(blue_led, LOW);
  } else {
    digitalWrite(blue_led, HIGH);
  }
  bblueOn = bOn;

}

void greenLed(bool bOn) {
  if (bgreenOn == bOn) return;
  if (bOn) {
    digitalWrite(gree_led, LOW);
  } else {
    digitalWrite(gree_led, HIGH);
  }
  bgreenOn = bOn;
}



void lcdon(int minValue ) {
  if (!initlcd) return;
  if (lcdLastTime == 0) {
    lcdLastTime = minValue;
  }
  if ((minValue - lcdLastTime) > 3) {
    lcd.noBacklight();
    blcdOn = false;
  } else {
    if (!blcdOn) {
      lcd.backlight();
      lcdLastTime = minValue;
      blcdOn = true;
    }
  }
}

void saveGPSData() {
  if (!initsd) return;
  if (gpsFile) {
    gpsFile.println((char *)gpsInfo.GPGGA);
    delay(1000);
  }
}

/*取得下一組資料,gps 取資料用*/
const char *nextToken(const char *src, char *buf) {
  int i = 0;
  while (src[i] != 0 && src[i] != ',')
    i++;

  if (buf) {
    strncpy(buf, src, i);
    buf[i] = 0;
  }

  if (src[i]) {
    i++;
  }
  return src + i;
}

/*將文字轉成數字*/
int transCharToInt(char *_buffer, int _start, int _stop)
{
  int _index;
  int result = 0;
  int num = _stop - _start + 1;
  int _temp[num];
  for (_index = _start; _index <= _stop; _index ++)
  {
    _temp[_index - _start] = _buffer[_index] - '0';
    result = 10 * result + _temp[_index - _start];
  }
  return result;
}


/*開啟氣象資料檔*/
void openDataFile(boolean bOpen, uint8_t mode) {
  if (bOpen) {
    if (dataFile) {
      dataFile.close();
    }
    dataFile = LSD.open(dataLog, mode);
  } else {
    if (dataFile) {
      dataFile.close();
    }
  }
}

/*開啟gps資料檔*/
void openGpsFile(boolean bOpen, uint8_t mode) {
  if (bOpen) {
    if (gpsFile) {
      gpsFile.close();
    }
    gpsFile = LSD.open(gpsLog, mode);
  } else {
    if (gpsFile) {
      gpsFile.close();
    }
  }
}

/*開啟config資料檔*/
void openConfigFile(boolean bOpen, uint8_t mode) {
  if (bOpen) {
    if (configFile) {
      configFile.close();
    }
    configFile = LSD.open("config.ini", mode);
  } else {
    if (configFile) {
      configFile.close();
    }
  }
}

/*開啟log資料檔*/
void openLogFile(boolean bOpen, uint8_t mode) {
  if (bOpen) {
    if (logFile) {
      logFile.close();
    }
    logFile = LSD.open("LOG.TXT", mode);
    if (logFile) {
      printToSerial("open log file done!", true);
    }
  } else {
    if (logFile) {
      logFile.close();
    }
  }
}


void doCommand() {
  int i;
  String cmd = "";
  cmd = getCommandValue(0);
  printToSerial("do command:" + cmd, true);
  if (cmd == "") return;
  if (cmd == "gettime") {
    String str = currentDate(true);
    printToSerial(str, false);
    LBTServer.println(str);
  } else if (cmd == "settime") {
    setRTCTime();
  } else if (cmd == "getconfig") {
    if (!initsd) return;
    if (configFile) {
      configFile.seek(0);
      while (configFile.available()) {
        LBTServer.println(configFile.readStringUntil('\n'));
      }
      LBTServer.println(";");
    }
  } else if (cmd == "setconfig") {
    setConfig();
  } else if (cmd == "getRtData") {
    lightLed(2);
    getRtData(0, true);
    LBTServer.println(getCsvData() + ";");
  } else if (cmd == "getAllData") {
    lightLed(2);
    getAllData();
  } else if (cmd == "getLog") {
    if (!initsd) return;
    if (logFile) {
      lightLed(2);
      logFile.flush();
      openLogFile(true, FILE_READ);
      if (logFile) {
        unsigned int i = 0;
        while (logFile.available()) {
          LBTServer.println(logFile.readStringUntil('\n'));
          if (i >= 20) {
            LBTServer.println(";");
            i = 0;
          } else {
            i++;
          }
        }
      }
      LBTServer.println(";");
      logFile.close();
      openLogFile(true, FILE_WRITE);
    }
  } else if (cmd == "delLog") {
    if (!initsd) return;
    char logf[] = "LOG.TXT";
    if (logFile) {
      logFile.close();
    }
    if (LSD.exists(logf)) {
      LSD.remove(logf);
    }
    openLogFile(true, FILE_WRITE);
  } else if (cmd == "delData") {
    if (!initsd) return;
    if (dataFile) {
      dataFile.close();
    }
    if (LSD.exists(dataLog)) {
      LSD.remove(dataLog);
    }
  } else if (cmd == "gpsOn") {
    gpsPower(true);
  } else if (cmd == "gpsOff") {
    gpsPower(false);
  } else if (cmd == "getRtGPS") {
    getRtGPSData();
  } else if (cmd == "delGps") {
    if (!initsd) return;
    if (LSD.exists(gpsLog)) {
      LSD.remove(gpsLog);
    }
  } else if (cmd == "lcdon") {
    blcdOn = false;
    lcdLastTime = currentMinValue();
    lcdon(lcdLastTime);
  } else if (cmd == "logSize" || cmd == "gpsLogSize") {
    if (!initsd) return;
    LFile iniFile;
    if (cmd = "logSize") {
      if (LSD.exists(dataLog)) {
        iniFile = LSD.open(dataLog, FILE_READ);
      }
    } else {
      if (LSD.exists(gpsLog)) {
        iniFile = LSD.open(gpsLog, FILE_READ);
      }
    }
    if (iniFile) {
      LBTServer.println("size," + String(iniFile.size()) + ";");
      iniFile.close();
    }
  }

  commandComplete = false;
}

void getRtGPSData() {
  if (!gpsOn) return;

  LBTServer.print(getGPSCsvData());
  LBTServer.println(";");
}

/*取得 GPS 座標*/
String getGPSCsvData() {
  const char *p = (char *)gpsInfo.GPGGA;
  char t[10];
  int th, tm, ts;
  char latitude[20];
  char ns[2];
  char longitude[20];
  char ew[2];
  char fixq[2];
  char n_satellite[3];
  String returnvalue = "";
  p = nextToken(p, 0);      // GGA
  p = nextToken(p, t);      // Time
  returnvalue = (char*)t;
  //LBTServer.print((char*)t);
  th = (t[0] - '0') * 10 + (t[1] - '0');
  tm = (t[2] - '0') * 10 + (t[3] - '0');
  ts = (t[4] - '0') * 10 + (t[5] - '0');
  p = nextToken(p, latitude);  // Latitude
  returnvalue = returnvalue + ",";
  //LBTServer.print(",");
  //LBTServer.print((char*)latitude);
  returnvalue = returnvalue + "," + (char*)latitude;
  p = nextToken(p, ns);      // N, S
  //LBTServer.print(",");
  //LBTServer.print((char*)ns);
  returnvalue = returnvalue + "," + (char*)ns;
  p = nextToken(p, longitude); // Longitude
  //LBTServer.print(",");
  //LBTServer.print((char*)longitude);
  returnvalue = returnvalue + "," + (char*)longitude;
  p = nextToken(p, ew);      // E, W
  //LBTServer.print(",");
  //LBTServer.print((char*)ew);
  returnvalue = returnvalue + "," + (char*)ew;
  p = nextToken(p, fixq);       // fix quality
  p = nextToken(p, n_satellite);       // number of satellites
  returnvalue = returnvalue;

}

String getCommandValue(int index) {
  return command[index];
}

void getAllData() {
  if (!initsd) return;
  openDataFile(true, FILE_READ);
  if (dataFile) {
    unsigned int i = 0;
    while (dataFile.available()) {
      LBTServer.println(dataFile.readStringUntil('\n'));
      if (i >= 20) {
        LBTServer.println(";");
        i = 0;
      } else {
        i++;
      }
    }
  }
  LBTServer.println(";");

}

void getRtData(unsigned int minValue, bool bReal) {
  getTemperature(minValue, bReal);
  getHumidity(minValue, bReal);
  getIllumination(minValue, bReal);
  getSoilHumidity(minValue, bReal);
  getAPRS(true);
}

String getCsvData() {
  String str = "";
  str = currentDate(false) + "," + currentTime() + ",";
  unsigned int i = 0;
  for (i = 0; i <= 9; i++) {
    //if (sensorData[i]=="") break;
    str = str + sensorData[i] + ",";
  }
  //printToSerial(str);
  return str.substring(0, str.length() - 1);
}

void setConfig() {
  openConfigFile(false, FILE_READ);
  char conf[] = "config.ini";
  if (LSD.exists(conf)) {
    LSD.remove(conf);
  }
  openConfigFile(true, FILE_WRITE);
  if (configFile) {
    String svalue = getCommandValue(1);
    configFile.write(svalue.c_str());
    configFile.flush();
    configFile.close();
    printToSerial("config file set done", true);
    openConfigFile(true, FILE_READ);
    /*更新完設定檔,重設環境*/
    setenv();
    if (configs[7] == 1) {
      bWifi=initWiFi();
    }

  }

}

void setRTCTime() {
//  if (!rtc.isrunning()) {  
//    printToSerial("RTC is NOT running!", true);
//    return;
//  }
  printToSerial("start setting date&time", true);
  String svalue = "";
  svalue = getCommandValue(1); //year
  unsigned int aYear=1970;
  unsigned int aMonth=1;
  unsigned int aDay=1;
  unsigned int aHour=12;
  unsigned int aMin=0;
  unsigned int aSec=0; 
  if (svalue != "") {
    aYear = svalue.toInt();// 從1970年開始算
  }
  svalue = getCommandValue(2); //month
  if (svalue != "") {
    aMonth = svalue.toInt();
  }
  svalue = getCommandValue(3); //Day
  if (svalue != "") {
    aDay = svalue.toInt();
  }
  svalue = getCommandValue(4); //Hour
  if (svalue != "") {
    aHour = svalue.toInt();
  }
  svalue = getCommandValue(5); //Minute
  if (svalue != "") {
    aMin = svalue.toInt();
  }
  svalue = getCommandValue(6); //Second
  if (svalue != "") {
    aSec = svalue.toInt();
  }
  rtc.adjust(DateTime(aYear, aMonth, aDay, aHour, aMin, aSec));
  //RTC.set(makeTime(rtctm));   
  initRTCTime();
  //LDateTime.setTime(&tm);
  printToSerial("setting date&time OK!", true);
  delay(1000);
  printToSerial("Current Date:" + currentDate(false), true);
  printToSerial("Current Time:" + currentTime(), true);
}

void batchPostToWeb() {
  printToSerial("start batch upload", true);
  unsigned int err = 10;
  boolean flag;
  int index = 0;
  if (!initsd) return;
  openDataFile(true, FILE_READ);
  if (dataFile) {
    String str = "";
    while (dataFile.available()) {
      str = dataFile.readStringUntil('\n');
      err = 10;
      //printToSerial(inputString);
      while (err > 0) {
        flag = postToWeb(str);
        if (!flag) {
          err--;
          /*若重試次數超過 10 次,直接跳離*/
          if (err < 10) {
            return;
          }
        } else {
          break;
        }
      }
    }
  }

  printToSerial("batch upload OK!", true);
  /*上傳完刪除檔案*/
  if (LSD.exists(dataLog)) {
    openDataFile(false, FILE_READ);
    LSD.remove(dataLog);
  }
}

/*
  void saveSensorData() {
  String data = getCsvData();
  printToSerial(data,false);
  if (isBatch) {
    saveToSD(data);
  } else if (!postToWeb(data)) {
    saveToSD(data);
  }
  }
*/
void saveSensorData() {
  String data = getCsvData();
  saveToSD(data);
  /*若可以連至雲端,及時上傳資料*/
  if (bWifi) {
    if (connectWCS()) {
      data = "temp_disp,," + sensorData[0];
      postToWeb(data);
  
      data = "humi_disp,," + sensorData[1];
      postToWeb(data);
  
      data = "rain_disp,," + sensorData[7];
      postToWeb(data);
  
      data = "rain_disp24,," + sensorData[8];
      postToWeb(data);
  
      data = "atmo_disp,," + sensorData[9];
      postToWeb(data);
  
    }
  }

}


boolean postToWeb(String data) {
  boolean flag = false;
  if (!connectWCS()) return flag; 
  //printToSerial(data,false);
  int dataLength = data.length();
  HttpClient http(httpClient);
  httpClient.print("POST /mcs/v2/devices/");
  httpClient.print(DEVICEID);
  httpClient.println("/datapoints.csv HTTP/1.1");
  httpClient.print("Host: ");
  httpClient.println(SITE_URL);
  httpClient.print("deviceKey: ");
  httpClient.println(DEVICEKEY);
  httpClient.print("Content-Length: ");
  httpClient.println(dataLength);
  httpClient.println("Content-Type: text/csv");
  httpClient.println("Connection: close");
  httpClient.println();
  httpClient.println(data);
  delay(500);

  int errorcount = 0;
  while (!httpClient.available())
  {
    errorcount += 1;
    if (errorcount > 10) {
      httpClient.stop();
      return false;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  //Serial.print("Content length is: ");
  //Serial.println(bodyLen);
  //Serial.println();

  String responseStr = "";

  char c;
  int ipcount = 0;
  int count = 0;
  int separater = 0;
  while (httpClient)
  {
    int v = httpClient.read();
    if (v != -1)
    {
      c = v;
      responseStr = responseStr + c;
    }
    else
    {
      //Serial.println("no more content, disconnect");
      httpClient.stop();

    }
  }
  if (responseStr != "Success.") {
    printToSerial(responseStr, true);
  }


  return flag;
}

String currentDate(bool comma) {
  LDateTime.getTime(&tm);
  if (comma) {
    return String(tm.year) + "," + String(tm.mon) + "," + String(tm.day) + "," + String(tm.hour) + "," + String(tm.min) + "," + String(tm.sec);
  } else {
    return String(tm.year) + "/" + String(tm.mon) + "/" + String(tm.day) ;
  }
}

String currentTime() {
  LDateTime.getTime(&tm);
  return String(tm.hour) + ":" + String(tm.min) + ":" + String(tm.sec);
}

int currentMinValue() {
  LDateTime.getTime(&tm);
  return tm.hour * 60 + tm.min;
}

void printToLcd(String str, int col, int row, bool isclear) {
  if (isclear) {
    lcd.clear();
  }
  lcd.setCursor(col, row);
  lcd.print(str);
  if (str.length() > 16) {
    lcd.setCursor(0, 1);
    lcd.print(str.substring(16));
  }

}

/*　執行批次上傳*/

void doBatchUpdate(String timeStr) {
  if (timeStr < upload_time) return;
  if (!isBatch) return;
  batchPostToWeb ();
}



char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, val);
  return sout;
}

/*
  mode 0
  取得溫度
  資料格式
  1,日期時間,攝氏度數,華式度數;
*/
void getTemperature(unsigned int minValue, bool bReal) {
  if (sensor_time[0] == 0) {
    sensorData[0] = "_";
    return;
  }
  if (sensorValue[0] == 0) {
    sensorData[0] = "_";
    return;
  }
  if (!bReal) {
    if (minValue < sensor_time[0]) return;
    temp = sensorValue[0] ;
  } else {
    temp = Temperature(true);
  }
  /*校正輸出值*/
  temp = temp * correctValue[0]/100;
  dtostrf(temp, 10, 0, outstr);
  String str1(outstr);
  str1.trim();
  sensorData[0] = str1;
  sensor_next_time(0);
  bsaveData = true;
  //printToSerial(sensorData);
  //printToLcd(sensorData, 0, 0, true);
}

/*
  mode 1
  取得濕度
  資料格式
  1,日期時間,濕度;
*/

void getHumidity(unsigned int minValue, bool bReal) {
  if (sensor_time[1] == 0) {
    sensorData[1] = "_";
    return;
  }
  if (sensorValue[1] == 0) {
    sensorData[1] = "_";
    return;
  }
  if (!bReal) {
    if (minValue < sensor_time[1]) return;
    temp = sensorValue[1];
  } else {
    temp = Humidity(true);
  }
  /*校正輸出值*/
  temp = temp * correctValue[1]/100;
  dtostrf(temp, 10, 2, outstr);
  String str3(outstr);
  str3.trim();
  sensor_next_time(1);
  sensorData[1] = str3;
  bsaveData = true;
  //printToSerial(sensorData);
  //printToLcd(sensorData, 0, 0, true);
}


/*
  mode 2
  取得光照度
  2,日期時間,勒克斯
*/

void getIllumination (unsigned int minValue, bool bReal) {
  if (sensor_time[2] == 0) {
    sensorData[2] = "_";
    return;
  }
  if (!bReal) {
    if (minValue < sensor_time[2]) return;
  }
  uint16_t lux = LightSensor.GetLightIntensity();// Get Lux value
  /*校正輸出值*/
  temp = lux * correctValue[2]/100;
  dtostrf(lux, 10, 2, outstr);
  String str3(outstr);
  str3.trim();
  sensor_next_time(2);
  sensorData[2] = str3;
  bsaveData = true;
  //printToSerial(sensorData);
  //printToLcd(sensorData, 0, 0, true);
}

/*
  mode 3
  取得土壤濕度
  3,日期時間,濕度類比數值
*/
void getSoilHumidity(unsigned int minValue, bool bReal) {
  if (sensor_time[3] == 0) {
    sensorData[3] = "_";
    return;
  }
  if (!bReal) {
    if (minValue < sensor_time[3]) return;
  }
  analogValue = analogRead(dustPin);
  analogValue = analogValue * correctValue[3]/100;
  String str3(analogValue);
  sensor_next_time(3);
  sensorData[3] = str3;
  bsaveData = true;
  //printToSerial(sensorData);
  //printToLcd(sensorData, 0, 0, true);
}

/*
   0:溫度
   1:濕度
   2:光照度
   3:土壤濕度
   4:風向
   5:風速
   6:最大風速
   7:1小時雨量
   8:24小時雨量
   9:氣壓

*/
void getAPRS(bool bReal) {
  if (sensor_time[4] == 0) {
    sensorData[4] = "_";
    sensorData[5] = "_";
    sensorData[6] = "_";
    sensorData[7] = "_";
    sensorData[8] = "_";
    sensorData[9] = "_";
    return;
  }

  /*風向*/
  dtostrf(sensorValue[4], 10, 0, outstr);
  String str3(outstr);
  str3.trim();
  sensorData[4] = str3;


  /*風速*/
  /*校正輸出值*/
  sensorValue[5] = sensorValue[5] * correctValue[5]/100;
  dtostrf(sensorValue[5], 10, 2, outstr);
  String str5(outstr);
  str5.trim();
  sensorData[5] = str5;

  /*最大風速*/
  /*校正輸出值*/
  sensorValue[6] = sensorValue[6] * correctValue[5]/100;
  dtostrf(sensorValue[6], 10, 2, outstr);
  String str6(outstr);
  str6.trim();
  sensorData[6] = str6;

  /*1小時雨量*/
  /*校正輸出值*/
  sensorValue[7] = sensorValue[7] * correctValue[6]/100;
  dtostrf(sensorValue[7], 10, 2, outstr);
  String str7(outstr);
  str7.trim();
  sensorData[7] = str7;

  /*24小時雨量*/
  /*校正輸出值*/
  sensorValue[8] = sensorValue[8] * correctValue[6]/100;
  dtostrf(sensorValue[8], 10, 2, outstr);
  String str8(outstr);
  str8.trim();
  sensorData[8] = str8;

  /*大氣壓力*/
  /*校正輸出值*/
  sensorValue[9] = sensorValue[9] * correctValue[6]/100;
  dtostrf(sensorValue[9], 10, 0, outstr);
  String str9(outstr);
  str9.trim();
  sensorData[9] = str9;


}


/*
  送出至 serial
*/
void printToSerial(String str, bool blog) {
  /*debug 模式才送出*/
  if (Serial) {
    Serial.println(str);
  }
  if (blog) {
    saveLog(str);
  }
  if (initLcd && blcdOn) {
    printToLcd(str, 0, 0, true);
  }
}

void saveLog(String str) {
  if (!initsd) return;
  if (logFile) {
    logFile.print(currentDate(false));
    logFile.print(" ");
    logFile.print(currentTime());
    logFile.print("-");
    logFile.println(str);
    //Serial.println(str);
  }
}

/*
  　存至 sd 卡
*/
void saveToSD(String str) {
  if (!initsd) {
    printToLcd("sd card fail", 0, 0, true);
    return;
  }
  openDataFile(true, FILE_WRITE);
  if (dataFile) {
    dataFile.println(str);
    dataFile.close();
  }
}


int WindDirection(bool bReal)                                                                  //Wind Direction
{
  sensorValue[4] = transCharToInt(databuffer, 1, 3);
  return sensorValue[4];
}

float WindSpeedAverage(bool bReal)                                                             //air Speed (1 minute)
{
  sensorValue[5] = 0.44704 * transCharToInt(databuffer, 5, 7);
  return sensorValue[5];
}

float WindSpeedMax(bool bReal)                                                                 //Max air speed (5 minutes)
{
  sensorValue[6] = 0.44704 * transCharToInt(databuffer, 9, 11);
  return sensorValue[6];
}

float Temperature(bool bReal)                                                                  //Temperature ("C")
{
  temp = (transCharToInt(databuffer, 13, 15) - 32.00) * 5.00 / 9.00;
  if (temp > 60 || temp < -20) {
    temp = sensorValue[0];
  }
  if (bReal) return temp;
  if (sensorValue[0] == 0) {
    sensorValue[0] = temp;
  } else {
    sensorValue[0] = (sensorValue[0] + temp) / 2;
  }
  return sensorValue[0];
}

float RainfallOneHour(bool bReal)                                                              //Rainfall (1 hour)
{
  sensorValue[7] = transCharToInt(databuffer, 17, 19) * 25.40 * 0.01;
  return sensorValue[7];
}

float RainfallOneDay(bool bReal)                                                               //Rainfall (24 hours)
{
  sensorValue[8] = transCharToInt(databuffer, 21, 23) * 25.40 * 0.01;
  return sensorValue[8];
}

int Humidity(bool bReal)                                                                       //Humidity
{
  temp = transCharToInt(databuffer, 25, 26);
  if (temp > 100) {
    temp = sensorValue[1];
  }
  if (bReal) return temp;
  if (sensorValue[1] == 0 || bReal) {
    sensorValue[1] = temp;
  } else {
    sensorValue[1] = (sensorValue[1] + temp) / 2;
  }
  return sensorValue[1];
}

float BarPressure(bool bReal)                                                                  //Barometric Pressure
{
  sensorValue[9] = transCharToInt(databuffer, 28, 32) / 10.00;
  if (sensorValue[9] > 1013.25) {
    sensorValue[9] = 1013.25;
  }
  return sensorValue[9];
}

void serialEvent1() {
  int index;
  for (index = 0; index < 35; index ++)
  {
    if (Serial1.available())
    {
      databuffer[index] = Serial1.read();
      if (databuffer[0] != 'c')
      {
        index = -1;
      }
    }
    else
    {
      index --;
    }
  }
  //Serial.println(databuffer);
  WindDirection(false);
  WindSpeedAverage(false);
  WindSpeedMax(false);
  Temperature(false);
  RainfallOneHour(false);
  RainfallOneDay(false);
  Humidity(false);
  BarPressure(false);
}

void serialEvent() {
  commandStr = "";
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      //printToSerial(commandStr);
      spliteString(commandStr);
      break;
    } else {
      commandStr += inChar;
    }
  }
  //printToSerial(commandStr);
}

void spliteString(String str) {
  if (str.indexOf(";") > -1) {
    str = str.substring(0, str.length() - 1);
  }
  int pos = 0;
  int i = 0;
  int oldpos = 0;
  int lastidx = 0;
  for (i = 0; i < commandSize; i++) {
    command[i] = "";
  }
  for (i = 0; i < commandSize; i++) {
    pos = str.indexOf(",", oldpos);
    if (pos > 0) {
      command[i] = str.substring(oldpos, pos);
      oldpos = pos + 1;
      lastidx = i;
      //printToSerial(command[i]);
    }
  }
  if (oldpos == 0) {
    command[0] = str;
  } else if (oldpos < str.length()) {
    command[lastidx + 1] = str.substring(oldpos);
  }
}



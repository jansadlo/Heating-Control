#include <RTClib.h>                           // RTC knihovna
#include <Wire.h>                             // I2C knihovna
#include <LiquidCrystal_I2C.h>                // LCD knihovna
#include <OneWire.h>                          // OneWire knihovna
#include <DallasTemperature.h>                // DS18B20 knihovna

#define DAY_BEGINS                   6        // hodina kdy začíná den
#define DAY_ENDS                     22       // hodina kdy již není den

#define PIN_POTENTIOMETER            A0       // pin ke střednímu vývodu potenciometru
#define PIN_TEMP_SENSOR              2        // pin připojený k DQ pinu senzoru DS18B20
#define PIN_WINDOW                   3        // pin k relé okna
#define PIN_MODE_SWITCH              4        // pin přepínače módu
#define PIN_RELAY                    5        // pin připojený k ovládání relé
#define PIN_BUTTON                   6        // pin tlačítka
#define PIN_HEATER_SENSOR            7        // pin senzoru kotle

#define T_MANUAL_RANGE_MAX           24       // maximální teplota rozsahu T_MANUAL
#define T_MANUAL_RANGE_MIN           16       // minimální teplota rozsahu T_MANUAL

#define TEMP_OPERATIONAL_RANGE_LOW   0        // spodní hranice provozního rozsahu teploty
#define TEMP_OPERATIONAL_RANGE_HIGH  50       // horní hranice provozního rozsahu teploty
#define TEMP_HEATING_MINIMAL         40       // minimální teplota otopné soustavy

#define REFRESH_DISPLAY_INTERVAL     1800000  // interval refreshe displeje (v ms)
#define MEASURE_TEMP_INTERVAL        30000    // interval měření teploty (v ms)
#define DISPLAY_INTERVAL             10000    // interval zapnutí displeje (v ms)
#define HEAT_ERR_CHECK_INTERVAL      600000   // interval kontroly chyb kotle (v ms)

#define TEMP_MINIMAL                 15       // teplota když apartmán není obydlen (minimální mód)
#define TEMP_NIGHT_DECREASE          1        // snížení teploty v době noci o x stupňů C
#define TEMP_HYSTERESIS              1        // hodnota hystereze směrem dolů (temp_Target -x°C)
#define MOVING_AVG_WIN_SIZE          10       // počet průměrovaných hodnot klouzavého průměru teploty



RTC_DS3231 rtc;                               // vytvoření objektu rtc
LiquidCrystal_I2C lcd(0x27,20,4);             // vytvoření objektu lcd, LCD je na defaultní adrese 0x27, má 20 znaků, 4 řádky
OneWire oneWireA(PIN_TEMP_SENSOR);            // nastavení oneWire instance na pinu PIN_TEMP_SENSOR
OneWire oneWireB(PIN_HEATER_SENSOR);          // nastavení oneWire instance na pinu PIN_TEMP_SENSOR
DallasTemperature sensorA(&oneWireA);         // pass oneWireA to DallasTemperature library
DallasTemperature sensorB(&oneWireB);         // pass oneWireB to DallasTemperature library


float temp_Manual;                            // teplota nastavená potenciometrem
float temp_Sensor = 0;                        // teplota senzoru ve stupních C
float temp_Corrected;                         // teplota kalibrovaného senzoru ve stupních C
float temp_Average;                           // klouzavý průměr temp_Corrected
float temp_Heater;                            // teplota senzoru kotle


float temp_RawHigh = 100;                     // RAW DATA ze senzoru při varu
float temp_RawLow = 0;                        // RAD DATA ze senzoru při trojném bodu
float temp_ReferenceHigh = 100;               // referenční teplota bodu varu !!! při úpravě podle atmosférického tlaku !!!
float temp_ReferenceLow = 0;                  // referenční teplota trojného bodu (přesná teplota 0,01 °C)


bool DST = true;                              // proměnná - letní čas
bool isDay;                                   // je den
bool windowClosed;                            // okno zavřené
bool modeMinimal;                             // minimální teplotní mód
bool heatOn;                                  // zapnout topení
bool previousModeState;                       // stav ModeMinimal z poslední smyčky
bool buttonOn = true;                         // tlačítko je stisknuté (v první smyčce - kvůli zapnutí displeje)
bool displayOn = true;                        // displej zapnutý (v první smyčce - výchozí podmínka pro vypnutí displeje po uplynutí intervalu)
bool previousHeatState = false;               // předchozí stav kotle
bool heatErr = false;                         // kotel v poruše


float temp_Target;                                // cílová teplota
float temp_Minimal = TEMP_MINIMAL;                // minimální teplota (nikdo nebydlí)
float temp_Night_Decrease = TEMP_NIGHT_DECREASE;  // snížení teploty v době noci
float temp_Hysteresis = TEMP_HYSTERESIS;          // hodnota hystereze (temp_Target -x°C)
float temp_Upper_Threshold;                       // horní mez temp_Target
float temp_Lower_Threshold;                       // spodní mez temp_Target


unsigned long previousTempMillis = 0;                             // předchozí čas uplynutí intervalu měření teploty
const long intervalTemp = MEASURE_TEMP_INTERVAL;                  // interval měření teploty

unsigned long previousDisplayRefreshMillis = 0;                   // předchozí čas uplynutí intervalu refreshe displeje
const long intervalDisplayRefresh = REFRESH_DISPLAY_INTERVAL;     // interval refreshe displeje

unsigned long displayStartTime = 0;                               // časovač podsvícení nastaven (v první smyčce displej zapnut -> =0)
const long intervalDisplay = DISPLAY_INTERVAL;                    // interval zapnutého displeje

unsigned long heatOnStartTime = 0;                                // čas zapnutí kotle
unsigned long previousHeatErrCheckMillis = 0;                     // předchozí čas uplynutí intervalu kontroly kotle
const long intervalHeatErrCheck = HEAT_ERR_CHECK_INTERVAL;        // interval kontroly chyb kotle


void setup () {
  Serial.begin(9600);

  if (! rtc.begin()) {                       // NASTAVENÍ RTC MODULU
    Serial.println("Couldn't find RTC");
    // Serial.flush();
    // while (1);                            // zasekne program
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));           // nastavení data a času pro RTC na datum a čas v okamžiku kompilace kódu
                                                            // PŘI FINÁLNÍM NASAZENÍ DO PROVOZU A NAHRÁNÍ DO HW, KDE RTC JIŽ ZNÁ PŘESNÝ ČAS, 
                                                            // TENTO ŘÁDEK !!!!! ODSTRANIT !!!!!
                                                            // (jinak by se při resetu či výpadku napětí tento pevně stanovený čas do RTC nahrál znovu)

  // rtc.adjust(DateTime(2022, 3, 27, 1, 59, 40));          // zápis nastavení času

  sensorA.begin();                           // inicializace senzoru v apartmánu
  sensorB.begin();                           // inicializace senzoru kotle

  pinMode(PIN_WINDOW, INPUT_PULLUP);         // konfigurovat PIN_WINDOW jako vstup, nastavit interní pullup
  pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);    // konfigurovat PIN_MODE_SWITCH jako vstup, nastavit interní pullup
  pinMode(PIN_RELAY, OUTPUT);                // konfigurovat PIN_RELAY jako výstup
  pinMode(PIN_BUTTON, INPUT_PULLUP);         // konfigurovat PIN_BUTTON jako vstup, nastavit interní pullup

  lcd.init();                                 // inicializace LCD
  lcd.display();                              // zapnutí displeje (vypnutí by bylo "lcd.noDisplay();")
  lcd.backlight();                            // zapnutí podsvícení (vypnutí by bylo "lcd.noBacklight();")
  lcd.clear();                                // refresh displeje

  }

void loop () {
    
    DateTime now = rtc.now();

    byte yy = now.year() % 100;               // zbytek po dělení 100
    byte mm = now.month();
    byte dd = now.day();
    byte x1 = 31 - (yy + yy / 4 - 2) % 7;     // poslední neděle v Březnu
    byte x2 = 31 - (yy + yy / 4 + 2) % 7;     // poslední neděle v Říjnu

    if (((mm > 3 && mm < 10) || (mm == 3 && dd >= x1) || (mm == 10 && dd < x2)) && now.hour() >= 2 && DST == false)            // KDYŽ datum je větší nebo rovno než poslední neděle v Březnu AND
   {                                                                                                                           // datum je menší než poslední neděle v Říjnu AND jsou 2 a více hodin AND není letní čas
    DST = true;                                                                                                                // je letní čas
    rtc.adjust(DateTime(yy, mm, dd, now.hour() + 1, now.minute(), now.second()));                                              // zvyš hodinu o 1
   }
    else if ((!(((mm > 3 && mm < 10) || (mm == 3 && dd >= x1) || (mm == 10 && dd < x2))) && now.hour() >= 3 && DST == true))   // KDYŽ datum je opak data z podmínky výše AND jsou 3 a více hodin AND je letní čas
   {
    DST = false;                                                                                                               // není letní čas
    rtc.adjust(DateTime(yy, mm, dd, now.hour() - 1, now.minute(), now.second()));                                              // sniž hodinu o 1
   }

/*------------------------------------------------------------------------------------------------*/

    isDay = now.hour() >= DAY_BEGINS && now.hour() < DAY_ENDS;        // isDay je true KDYŽ HODINA je většíneborovna než DAY_BEGINS a zároveň menší než DAY_ENDS

/*------------------------------------------------------------------------------------------------*/

    int analogValue = analogRead(PIN_POTENTIOMETER);                                                  // čtení vstupu na analogovém pinu PIN_POTENTIOMETER
    temp_Manual = floatMap(analogValue, 0, 1023, T_MANUAL_RANGE_MAX, T_MANUAL_RANGE_MIN);             // temp_Manual - použití funkce floatMap (přeškálovat na teplotu temp_Manual (rozsah od do))

/*------------------------------------------------------------------------------------------------*/

    if (! temp_Sensor)                                                // KDYŽ teplota nebyla ještě načtena
  {
    Serial.print("temp_Sensor == NULL; MEASURE TEMP CYCLE;");
    Serial.print(" REQUESTING TEMP...");
    sensorA.requestTemperatures();                                    // příkaz k získání teploty
    Serial.println(" DONE;");
    temp_Sensor = sensorA.getTempCByIndex(0);                         // čtení teploty ve stupních C
    
    // KALIBRACE temp_Sensor --> temp_Corrected
    float temp_RawRange = temp_RawHigh - temp_RawLow;
    float temp_ReferenceRange = temp_ReferenceHigh - temp_ReferenceLow;
    temp_Corrected = (((temp_Sensor - temp_RawLow) * temp_ReferenceRange) / temp_RawRange) + temp_ReferenceLow;

    temp_Average = movingAverage(temp_Corrected);                     // klouzavý průměr
  }
  
  unsigned long currentTempMillis = millis();                         // načtení současného času běhu smyčky
    if (currentTempMillis - previousTempMillis >= intervalTemp)       // MĚŘENÍ INTERVALU od posledního splnění podmínky
   {
    previousTempMillis = currentTempMillis;                           // uloží čas současného provedení IF do příští smyčky

    Serial.print("MEASURE TEMP CYCLE;");
    Serial.print(" REQUESTING TEMP...");
    sensorA.requestTemperatures();                                    // příkaz k získání teploty
    Serial.println(" DONE;");
    temp_Sensor = sensorA.getTempCByIndex(0);                         // čtení teploty ve stupních C

    
    // KALIBRACE temp_Sensor --> temp_Corrected
    float temp_RawRange = temp_RawHigh - temp_RawLow;
    float temp_ReferenceRange = temp_ReferenceHigh - temp_ReferenceLow;
    temp_Corrected = (((temp_Sensor - temp_RawLow) * temp_ReferenceRange) / temp_RawRange) + temp_ReferenceLow;

    temp_Average = movingAverage(temp_Corrected);                     // klouzavý průměr
  }

/*------------------------------------------------------------------------------------------------*/

    windowClosed = digitalRead(PIN_WINDOW);                           // nastavit windowClosed podle vstupu
                                                                      // obvod rozpojen windowClosed = TRUE
                                                                      // obvod uzavřen windowClosed = FALSE
  
    modeMinimal = digitalRead(PIN_MODE_SWITCH);                       // nastavit modeManual podle vstupu
                                                                      // obvod rozpojen modeManual = TRUE
                                                                      // obvod uzavřen modeManual = FALSE

    buttonOn = !digitalRead(PIN_BUTTON);                              // čtení stavu tlačítka (pullup převrací hodnotu)

/*------------------------------------------------------------------------------------------------*/

    if (modeMinimal) temp_Target = temp_Minimal;                      // KDYŽ v apartmánu nikdo nebydlí, cílová teplota je Minimal
    else temp_Target = temp_Manual;                                   // JINAK je teplota v manuálním režimu

/*------------------------------------------------------------------------------------------------*/

    if (!isDay) temp_Target = temp_Target - temp_Night_Decrease;      // v noci sniž temp_Target o stanovenou mez

    temp_Upper_Threshold = temp_Target;                               // horní hranice vytápěné teploty
    temp_Lower_Threshold = temp_Target - temp_Hysteresis;             // spodní hranice vytápěné teploty


    if (temp_Average > temp_Upper_Threshold && windowClosed)
   {
    heatOn = false;                                                   // proměnná heatOn je nepravda
    digitalWrite(PIN_RELAY, LOW);                                     // VYPNOUT RELÉ
   } 
    if (temp_Average < temp_Lower_Threshold && windowClosed)
   {
    heatOn = true;                                                    // proměnná heatOn je pravda
    digitalWrite(PIN_RELAY, HIGH);                                    // ZAPNOUT RELÉ
   }


    if (!windowClosed)                                                // KDYŽ je otevřené okno
   {
    heatOn = false;
    digitalWrite(PIN_RELAY, LOW);                                     // VYPNI RELÉ
   }

/*------------------------------------------------------------------------------------------------*/

    if (buttonOn)                                                     // KDYŽ je tlačítko stisknuté
  {
    displayOn = true;                                                 // stav displayOn = TRUE
    lcd.display();                                                    // zapni displej
    lcd.backlight();                                                  // zapni podsvícení
    displayStartTime = millis();                                      // nastav počáteční stav zapnutí displeje
  }

    else if (displayOn == true && millis() - displayStartTime >= intervalDisplay)
  {                                                                   // KDYŽ je stav podsvícení true A ZÁROVEŇ čas od stisknutí tlačítka > interval
    displayOn = false;                                                // stav displayOn = FALSE
    lcd.noBacklight();                                                // vypni displej
    lcd.noDisplay();                                                  // vypni podsvícení
  }

/*------------------------------------------------------------------------------------------------*/

    if (modeMinimal != previousModeState)                             // KDYŽ se došlo k přepnutí režimu/módu vytápění
  {
    displayOn = true;
    lcd.display();                                                    // zapni displej
    lcd.backlight();                                                  // zapni podsvícení
    displayStartTime = millis();                                      // nastav počáteční čas zapnutí displeje
    previousModeState = modeMinimal;                                  // uloží současný stav do příští smyčky
  }

/*------------------------------------------------------------------------------------------------*/

    if (heatOn && previousHeatState == false)                         // KDYŽ je kotel zapnutý A ZÁROVEŇ byl předtím vypnutý
  {

  }

/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*
    Serial.print("MEASURE TEMP HEATER CYCLE;");
    Serial.print(" REQUESTING TEMP...");
    sensorB.requestTemperatures();                                    // příkaz k získání teploty
    Serial.println(" DONE;");
    temp_Heater = sensorB.getTempCByIndex(0);                         // čtení teploty ve stupních C

                                                                                                  */
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
/*------------------------DODĚLAT ČASOVAČ KONTROLY KOTLE------------------------------------------*/
    
    heatErr = temp_Heater < TEMP_HEATING_MINIMAL;                     // heatErr je true KDYŽ teplota kotle je nižší než TEMP_HEATING_MINIMAL

/*------------------------------------------------------------------------------------------------*/
  
  unsigned long currentDisplayRefreshMillis = millis();                                         // načtení současného času běhu smyčky
    if (currentDisplayRefreshMillis - previousDisplayRefreshMillis >= intervalDisplayRefresh)   // MĚŘENÍ INTERVALU od posledního splnění podmínky
  {                                                                                             
    previousDisplayRefreshMillis = currentDisplayRefreshMillis;       // uloží čas současného provedení IF do příští smyčky
    lcd.clear();                                                      // REFRESH obrazovky
  }


    if (displayOn)
  {
    lcd.setCursor(0,0);                   // nastav kurzor na LCD na 0,0
    if (now.hour() < 10)                  // KDYŽ HODINA < 10
  {                                       //
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.print(now.hour());                // zobraz na LCD hodinu
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.print(now.hour());                // zobraz na LCD hodinu
  }
    lcd.print(':');
    if (now.minute() < 10)                // KDYŽ MINUTA < 10
  {                                       //
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.print(now.minute());              // zobraz na LCD minutu
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.print(now.minute());              // zobraz na LCD minutu
  }
    lcd.print(':');                       //
    if (now.second() < 10)                // KDYŽ SEKUNDA < 10
  {                                       //
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.print(now.second());              // zobraz na LCD sekundu
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.print(now.second());              // zobraz na LCD sekundu
  }

    // lcd.print(now.timestamp(DateTime::TIMESTAMP_TIME));     // ČASOVÉ RAZÍTKO (alternativa - zabírá více paměti)

    
    lcd.setCursor(12,0);                  // nastav kurzor na LCD na 12,0
    lcd.print("T:");                      // zobraz na LCD "T:"
    lcd.print(temp_Average,1);            // zobraz na LCD proměnnou temp_Average, jedno desetinné místo
    lcd.print(char(223));                 // zobraz na LCD znak "°"
    lcd.print("C");                       // zobraz na LCD "C"


    lcd.setCursor(0,1);                   // nastav kurzor na LCD na 0,1
    if (isDay)                            // KDYŽ isDay = true
  {
    lcd.print("DEN");                     // zobrazí na LCD "DEN"
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.print("NOC");                     // zobrazí na LCD "NOC"
  }


    lcd.setCursor(5,1);                   // nastav kurzor na LCD na 5,1
    lcd.print("T_MANUAL:");               // zobraz na LCD "T_MANUAL:"
    lcd.print(temp_Manual,1);             // zobraz na LCD proměnnou temp_Manual, jedno desetinné místo
    lcd.print(char(223));                 // zobraz na LCD znak "°"
    lcd.print("C");                       // zobraz na LCD "C"


    lcd.setCursor(0,2);                   // nastav kurzor na LCD na 0,2
    if (windowClosed)                     // KDYŽ windowClosed je TRUE
  {
    lcd.print("OKNO:ZAVR");               // zobraz na LCD "OKNO:ZAVR"
  }
    else                                  // JINAK
  {
    lcd.print("OKNO:OTEV");               // zobraz na LCD "OKNO:OTEV"
  }

    lcd.setCursor(13,2);                  // nastav kurzor na LCD na 13,2
    if (modeMinimal)                      // KDYŽ modeMinimal
  {
    lcd.print("NEBYDLI");                 // zobraz na LCD "NEBYDLI"
  }
    else                                  // JINAK
  {
    lcd.print("  BYDLI");                 // zobraz na LCD "  BYDLI"
  }

    lcd.setCursor(0,3);                   // nastav kurzor na LCD na 0,3
    lcd.print("CIL:");                    // zobraz na LCD "CIL:"
    lcd.print(temp_Target,1);             // zobraz na LCD proměnnou temp_Manual, jedno desetinné místo
    lcd.print(char(223));                 // zobraz na LCD znak "°"
    lcd.print("C");                       // zobraz na LCD "C"

    lcd.setCursor(11,3);                  // nastav kurzor na LCD na 13,3
    if (heatOn)                           // KDYŽ heatOn
  {
    if (!heatErr)                         // KDYŽ kotel není v poruše
    lcd.print("    TOPIM");               // zobraz na LCD "    TOPIM"
    else
    lcd.print("POR.KOTLE");               // zobraz na LCD "POR.KOTLE"
  }
    else                                  // JINAK
  {
    lcd.print("  NETOPIM");               // zobraz na LCD "  NETOPIM"
  }
  }

/*------------------------------------------------------------------------------------------------*/

    Serial.print("Date & Time: ");        // vypiš na sériovou linku "Date & Time: "
    Serial.print(now.year());             // LOMÍTKO
    Serial.print('/');
    if (now.month() < 10)                 // KDYŽ MĚSÍC < 10
  {
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.month());            // vypiš na sériovou linku měsíc
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.month());            // vypiš na sériovou linku měsíc
  }
    Serial.print('/');                    // LOMÍTKO
    if (now.day() < 10)                   // KDYŽ DEN < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.day());              // vypiš na sériovou linku den
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.day());              // vypiš na sériovou linku den
  }
    Serial.print(" ");                    // vypiš na sériovou linku " "
    if (now.hour() < 10)                  // KDYŽ HODINA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.hour());             // vypiš na sériovou linku hodinu
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.hour());             // vypiš na sériovou linku hodinu
  }
    Serial.print(':');                    // DVOJTEČKA
    if (now.minute() < 10)                // když MINUTA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.minute());           // vypiš na sériovou linku minutu
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.minute());           // vypiš na sériovou linku minutu
  }
    Serial.print(':');                    // DVOJTEČKA
    if (now.second() < 10)                // KDYŽ SEKUNDA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.second());           // vypiš na sériovou linku sekundu
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.second());           // vypiš na sériovou linku sekundu
  }
    // Serial.print(now.timestamp(DateTime::TIMESTAMP_FULL));     // ČASOVÉ RAZÍTKO (alternativa - zabírá více paměti)
    
    if (isDay)                            // KDYŽ isDay = true
  {
    Serial.print(" DEN; ");               // vypiš na sériovou linku " DEN;"
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(" NOC; ");               // vypiš na sériovou linku " NOC;"
  }


    Serial.print("T_MANUAL: ");           // vypiš na sériovou linku "T_MANUAL: "
    Serial.print(temp_Manual);            // vypiš na sériovou linku proměnnou temp_Manual
    Serial.print("°C");                   // vypiš na sériovou linku "°C"

    Serial.print(" T_SENSOR: ");          // vypiš na sériovou linku "T_SENSOR: "
    Serial.print(temp_Sensor);            // vypiš na sériovou linku proměnnou temp_Sensor
    Serial.print("°C");                   // vypiš na sériovou linku "°C"
    
    Serial.print(" T_CORRECTED: ");       // vypiš na sériovou linku "T_CORRECTED: "
    Serial.print(temp_Corrected);         // vypiš na sériovou linku proměnnou temp_Corrected
    Serial.print("°C");                   // vypiš na sériovou linku "°C"

    Serial.print(" T_AVERAGE: ");         // vypiš na sériovou linku "T_AVERAGE: "
    Serial.print(temp_Average);           // vypiš na sériovou linku proměnnou temp_Average
    Serial.print("°C");                   // vypiš na sériovou linku "°C"


    if (windowClosed) 
  {
    Serial.print(" OKNO:ZAVRENO ");
  }
    else 
  {
    Serial.print(" OKNO:OTEVRENO");
  }

    if (modeMinimal) 
  {
    Serial.print(" MOD:NEBYDLI");
  }
    else 
  {
    Serial.print(" MOD:BYDLI  ");
  }


    Serial.print(" T_TARGET: ");
    Serial.print(temp_Target);
    Serial.print("°C");


    if (heatOn)                           // KDYŽ heatOn = true
  {
    Serial.print(" TOPIM");               // vypiš na sériovou linku " TOPIM"
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(" NETOPIM");             // vypiš na sériovou linku " NETOPIM"
  }


      
    if (temp_Sensor < TEMP_OPERATIONAL_RANGE_LOW || temp_Sensor > TEMP_OPERATIONAL_RANGE_HIGH)    // KDYŽ temp_Sensor je mimo provozní rozsah
  {
    Serial.println();
    Serial.print(" TEMPERATURE OUT OF RANGE - ERROR ");
  }


      if (heatErr)                        // KDYŽ je kotel v poruše
  {
    Serial.println();
    Serial.print(" HEATER ERROR ");
  }

/*------------------------------------------------------------------------------------------------*/

    Serial.println();                     // nový řádek

}


// FUNKCE PRO KLOUZAVÝ PRŮMĚR

float movingAverage(float value) {
  const byte nvalues = MOVING_AVG_WIN_SIZE;     // Moving average window size

  static byte current = 0;                      // Index for current value
  static byte cvalues = 0;                      // Count of values read (<= nvalues)
  static float sum = 0;                         // Rolling sum
  static float values[nvalues];

  sum += value;

                                                // If the window is full, adjust the sum by deleting the oldest value
  if (cvalues == nvalues)
    sum -= values[current];

  values[current] = value;                      // Replace the oldest with the latest

  if (++current >= nvalues)
    current = 0;

  if (cvalues < nvalues)
    cvalues += 1;

  return sum/cvalues;
}


// FUNKCE PRO MAPOVÁNÍ

float floatMap(float x, float in_min, float in_max, float out_min, float out_max) 
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

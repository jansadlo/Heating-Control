#include <RTClib.h>               // RTC knihovna
#include <Wire.h>                 // I2C knihovna
#include <LiquidCrystal_I2C.h>    // LCD knihovna
#include <OneWire.h>              // OneWire knihovna
#include <DallasTemperature.h>    // DS18B20 knihovna


#define DAY_BEGINS          6      // DEFINOVAT !!! hodinu kdy začíná den
#define DAY_ENDS            22     // DEFINOVAT !!! hodinu kdy již není den
#define TEMP_MAN_RANGE_MAX  26     // DEFINOVAT !!! maximální teplotu rozsahu TEMP_MAN
#define TEMP_MAN_RANGE_MIN  16     // DEFINOVAT !!! minimální teplotu rozsahu TEMP_MAN

#define TEMP_SENSOR_PIN              2    // Arduino pin připojený k DQ pinu senzoru DS18B20
#define TEMP_OPERATIONAL_RANGE_LOW   0    // spodní hranice provozního rozsahu teploty
#define TEMP_OPERATIONAL_RANGE_HIGH  50   // horní hranice provozního rozsahu teploty

#define PIN_POTENTIOMETER            A0   // pin ke střednímu vývodu potenciometru


RTC_DS3231 rtc;                           // vytvoření objektu rtc
LiquidCrystal_I2C lcd(0x27,20,4);         // vytvoření objektu lcd, LCD je na defaultní adrese 0x27, má 20 znaků, 4 řádky
OneWire oneWire(TEMP_SENSOR_PIN);         // nastavení oneWire instance na pinu TEMP_SENSOR_PIN
DallasTemperature sensors(&oneWire);      // pass oneWire to DallasTemperature library


char daysOfTheWeek[7][12] = {"Ne","Po","Ut","St","Ct","Pa","So"};

float floatMap(float x, float in_min, float in_max, float out_min, float out_max) {     // Funkce floatMap (pro pozdější mapování
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;              // rozsahu odporu potenciometru z analogového
}                                                                                       // vstupu, na rozsah teplot)

    int analogValue = analogRead(PIN_POTENTIOMETER);                                                  // čtení vstupu na analogovém pinu A0
    float temp_Manual = floatMap(analogValue, 0, 1023, TEMP_MAN_RANGE_MAX, TEMP_MAN_RANGE_MIN);       // deklarace temp_Manual - použití funkce floatMap (přeškálovat na teplotu temp_Manual (rozsah od do))


float temp_Sensor;                        // teplota senzoru ve stupních C

float temp_RawHigh = 100;                 // RAW DATA ze senzoru při varu
float temp_RawLow = 0;                    // RAD DATA ze senzoru při trojném bodu
float temp_ReferenceHigh = 100;           // referenční teplota bodu varu !!! při úpravě podle atmosférického tlaku !!!
float temp_ReferenceLow = 0;              // referenční teplota trojného bodu (přesná teplota 0,01 °C)

float temp_Corrected;                     // teplota kalibrovaného senzoru ve stupních C


bool isDay;

void setup () {
  Serial.begin(9600);

  if (! rtc.begin()) {                    // NASTAVENÍ RTC MODULU
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1);
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));           // nastavení data a času pro RTC na datum a čas v okamžiku kompilace kódu
                                                            // PŘI FINÁLNÍM NASAZENÍ DO PROVOZU A NAHRÁNÍ DO HW, KDE RTC JIŽ ZNÁ PŘESNÝ ČAS, 
                                                            // TENTO ŘÁDEK !!!!! ODSTRANIT !!!!!
                                                            // (jinak by se při resetu či výpadku napětí tento pevně stanovený čas do RTC nahrál znovu)

  sensors.begin();                        // inicializace senzoru

  lcd.init();                             // inicializace LCD
  lcd.backlight();                        // zapnutí podsvícení (vypnutí podsvícení by bylo "lcd.noBacklight();)

  }

void loop () {
    
    DateTime now = rtc.now();
    
    if (now.hour(), DEC >= DAY_BEGINS && now.hour(), DEC < DAY_ENDS)      // KDYŽ HODINA je většíneborovna než DAY_BEGINS nebo menší než DAY_ENDS
  {
    bool isDay = true;                    // je den -> isDay = true
  }                                       //
    else                                  // JINAK
  {                                       //
    bool isDay = false;                   // je noc -> isDay = false
  }

/*------------------------------------------------------------------------------------------------*/


    int analogValue = analogRead(PIN_POTENTIOMETER);                                                  // čtení vstupu na analogovém pinu PIN_POTENTIOMETER
    float temp_Manual = floatMap(analogValue, 0, 1023, TEMP_MAN_RANGE_MAX, TEMP_MAN_RANGE_MIN);       // temp_Manual - použití funkce floatMap (přeškálovat na teplotu temp_Manual (rozsah od do))


/*--------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------------------------------*/


  sensors.requestTemperatures();                       // příkaz k získání teploty
  float temp_Sensor = sensors.getTempCByIndex(0);      // čtení teploty ve stupních C


  // KALIBRACE temp_Sensor --> temp_Corrected
  float temp_RawRange = temp_RawHigh - temp_RawLow;
  float temp_ReferenceRange = temp_ReferenceHigh - temp_ReferenceLow;
  float temp_Corrected = (((temp_Sensor - temp_RawLow) * temp_ReferenceRange) / temp_RawRange) + temp_ReferenceLow;


/*--------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------------------------------*/


    lcd.clear();

  
    if (now.hour() < 10)                  // KDYŽ HODINA < 10
  {                                       //
    lcd.setCursor(0,0);                   // nastav kurzor na LCD na 0,0
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.setCursor(1,0);                   // nastav kurzor na LCD na 1,0
    lcd.print(now.hour(), DEC);           // zobraz na LCD hodinu
  }                                       //
  else                                    // JINAK
  {                                       //
    lcd.setCursor(0,0);                   // nastav kurzor na LCD na 0,0
    lcd.print(now.hour(), DEC);           // zobraz na LCD hodinu
  }
    lcd.setCursor(2,0);                   // DVOJTEČKA
    lcd.print(':');                       //

    if (now.minute() < 10)                // KDYŽ MINUTA < 10
  {                                       //
    lcd.setCursor(3,0);                   // nastav kurzor na na LCD na 3,0
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.setCursor(4,0);                   // nastav kurzor na LCD na 4,0
    lcd.print(now.minute(), DEC);         // zobraz na LCD minutu
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.setCursor(3,0);                   // nastav kurzor na LCD na 3,0
    lcd.print(now.minute(), DEC);         // zobraz na LCD minutu
  }
    lcd.setCursor(5,0);                   // DVOJTEČKA
    lcd.print(':');                       //
    if (now.second() < 10)                // KDYŽ SEKUNDA < 10
  {                                       //
    lcd.setCursor(6,0);                   // nastav kurzor na LCD na 6,0
    lcd.print("0");                       // zobraz na LCD "0"
    lcd.setCursor(7,0);                   // nastav kurzor na LCD na 7,0
    lcd.print(now.second(), DEC);         // zobraz na LCD sekundu
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.setCursor(6,0);                   // nastav kurzor na LCD na 6,0
    lcd.print(now.second(), DEC);         // zobraz na LCD sekundu
  }
    
    if (isDay)                            // KDYŽ isDay = true
  {
    lcd.setCursor(0,1);                   // nastav kurzor na LCD na 0,1
    lcd.print("DEN");                     // zobrazí na LCD "DEN"
  }                                       //
    else                                  // JINAK
  {                                       //
    lcd.setCursor(0,1);                   // nastav kurzor na LCD na 0,1
    lcd.print("NOC");                     // zobrazí na LCD "NOC"
  }


    lcd.setCursor(5,1);                   // nastav kurzor na LCD na 5,1
    lcd.print("TEMP_MAN:");               // zobraz na LCD "TEMP_MAN:"
    lcd.setCursor(14,1);                  // nastav kurzor na LCD na 14,1
    lcd.print(temp_Manual,1);             // zobraz na LCD proměnnou temp_Manual, jedno desetinné místo
    lcd.setCursor(18,1);                  // nastav kurzor na 18,1
    lcd.print(char(223));                 // zobraz na LCD znak "°"
    lcd.setCursor(19,1);                  // nastav kurzor na 19,1
    lcd.print("C");                       // zobraz na LCD "C"


    lcd.setCursor(9,0);                   // nastav kurzor na LCD na 9,0
    lcd.print("TEMP:");                   // zobraz na LCD "TEMP:"
    lcd.setCursor(14,0);                  // nastav kurzor na LCD na 14,0
    lcd.print(temp_Corrected,1);          // zobraz na LCD proměnnou temp_Corrected, jedno desetinné místo
    lcd.setCursor(18,0);                  // nastav kurzor na 18,0
    lcd.print(char(223));                 // zobraz na LCD znak "°"
    lcd.setCursor(19,0);                  // nastav kurzor na 19,0
    lcd.print("C");                       // zobraz na LCD "C"


/*--------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------------------------------*/

    Serial.print("Date & Time: ");        // vypiš na sériovou linku "Date & Time: "
    Serial.print(now.year(), DEC);        // LOMÍTKO
    Serial.print('/');
    if (now.month() < 10)                 // KDYŽ MĚSÍC < 10
  {
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.month(), DEC);       // vypiš na sériovou linku měsíc
  }                                       //
  else                                    // JINAK
  {                                       //
    Serial.print(now.month(), DEC);       // vypiš na sériovou linku měsíc
  }
    Serial.print('/');                    // LOMÍTKO
    if (now.day() < 10)                   // KDYŽ DEN < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.day(), DEC);         // vypiš na sériovou linku den
  }                                       //
  else                                    // JINAK
  {                                       //
    Serial.print(now.day(), DEC);         // vypiš na sériovou linku den
  }
    Serial.print(" (");                   // vypiš na sériovou linku " ("
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);    // vypiš na sériovou linku den týdne
    Serial.print(") ");                   // vypiš na sériovou linku ") "
    if (now.hour() < 10)                  // KDYŽ HODINA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.hour(), DEC);        // vypiš na sériovou linku hodinu
  }                                       //
  else                                    // JINAK
  {                                       //
    Serial.print(now.hour(), DEC);        // vypiš na sériovou linku hodinu
  }
    Serial.print(':');                    // DVOJTEČKA
    if (now.minute() < 10)                // když MINUTA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.minute(), DEC);      // vypiš na sériovou linku minutu
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.minute(), DEC);      // vypiš na sériovou linku minutu
  }
    Serial.print(':');                    // DVOJTEČKA
    if (now.second() < 10)                // KDYŽ SEKUNDA < 10
  {                                       //
    Serial.print('0');                    // vypiš na sériovou linku "0"
    Serial.print(now.second(), DEC);      // vypiš na sériovou linku sekundu
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(now.second(), DEC);      // vypiš na sériovou linku sekundu
  }
    if (isDay)                            // KDYŽ isDay = true
  {
    Serial.print(" DEN; ");               // vypiš na sériovou linku " DEN;"
  }                                       //
    else                                  // JINAK
  {                                       //
    Serial.print(" NOC; ");               // vypiš na sériovou linku " NOC;"
  }

/*------------------------------------------------------------------------------------------------*/

    Serial.print("TEMP_MAN: ");           // vypiš na sériovou linku "TEMP_MAN: "
    Serial.print(temp_Manual);            // vypiš na sériovou linku proměnnou temp_Manual
    Serial.print("°C");                   // vypiš na sériovou linku "°C"

/*------------------------------------------------------------------------------------------------*/

  // KDYŽ temp_Sensor je v provozním rozsahu
  if (temp_Sensor > TEMP_OPERATIONAL_RANGE_LOW && temp_Sensor < TEMP_OPERATIONAL_RANGE_HIGH)               
  {
    Serial.print(" temp_Sensor: ");
    Serial.print(temp_Sensor);
    Serial.print("°C");
    
    Serial.print(" temp_Corrected: ");
    Serial.print(temp_Corrected);
    Serial.println("°C");
  } 
  else
  {
    Serial.print(" temp_Sensor: ");
    Serial.print(temp_Sensor);
    
    Serial.print(" temp_Corrected: ");
    Serial.print(temp_Corrected);
    Serial.print("°C");

    Serial.println(" TEMPERATURE OUT OF RANGE - ERROR");
  }


/*--------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------------------------------*/

    delay(1000);                          // počkej 1 sekundu

}

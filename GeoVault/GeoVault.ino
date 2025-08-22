// FQBN: esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashFreq=40,FlashMode=dio

// zapojeni
#include "pinout.h"


/* AsyncLogger se pouziva pro logovani udalosti v asynchronnich aktivitach (webserver...).
Uklada zaznamy do pole v pameti a ty se pak vypisou v loop() pomoci volani dumpTo(); */
#include "src/logging/AsyncLogger.h"
AsyncLogger asyncLogger;

#include "src/logging/SerialLogger.h"
SerialLogger serialLogger( &Serial );


/** Sdileny stav aplikace - objekt drzici napr. chybu inicializace MP3, aby se dala vypsat  */
#include "AppState.h"
AppState appState;



/*
WebServer - pres izolacni vrstvu

Pouzivaji se tyto dve knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 
*/
#include "EasyWebServer.h"
EasyWebServer webserver( &asyncLogger );


/*
Konfigurace.
Je nutny alespon kousek filesystemu SPIFFS.
*/
#include "src/toolkit/BasicConfig.h"
#include "src/toolkit/ConfigProviderSpiffs.h"
BasicConfig config;
// může dostat serialLogger, protože poběží synchronně v loopu
ConfigProviderSpiffs configProvider( &serialLogger, &config, &appState);
bool saveConfigChange=false;


/*
Periodicke ulohy
https://github.com/joysfera/arduino-tasker
*/
#define TASKER_MAX_TASKS 10
#include <Tasker.h>
Tasker tasker;


/*
Displej 1.8" 128x160 
*/
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "Adafruit_ST77xx.h"
#include <SPI.h>
Adafruit_ST7735 * tft;

// https://github.com/petrbrouzda/ExtGfx
#include "src/extgfx/TextPainter.h"
#include "src/extgfx/HorizontalBar.h"
#include "src/extgfx/BasicColors.h"

// konverze viz https://github.com/petrbrouzda/fontconvert8-iso8859-2
#include "fonts/PragatiNarrow-Regular16pt8b.h"
#include "fonts/PragatiNarrow-Regular20pt8b.h"
#include "fonts/PragatiNarrow-Regular12pt8b.h"

TextPainter * painter;
TpFontConfig malePismo;
TpFontConfig vetsiPismo;
TpFontConfig mikroPismo;



/** 
GPS
http://arduiniana.org/libraries/tinygpsplus/
https://github.com/mikalhart/TinyGPSPlus?tab=readme-ov-file 

GPS pripojena na 3v3 vystup shodi cely cip
ale GPS ma povoleno 3.3-5 V, takze ji lze dat primo na baterku nebo na step-up
*/
#include <TinyGPS++.h>
TinyGPSPlus gps;

HardwareSerial hwSerial_1(1);



// serva 
// https://github.com/madhephaestus/ESP32Servo
// verze 3.0.8 je rozbita, downgrade na 3.0.7!  https://github.com/madhephaestus/ESP32Servo/issues/77
#include <ESP32Servo.h>

Servo servo1;
Servo servo2;


/*
Pro mereni kapacity baterky - kalibrace ADC mereni.
https://github.com/madhephaestus/ESP32AnalogRead 
*/
#include <ESP32AnalogRead.h>
ESP32AnalogRead adc;

#include "src/toolkit/map_double.h"
#include "src/toolkit/mnozne_cislo.h"


//---------- setup

void setup() {
  Serial.begin(115200);

  // nejprve nahodit displej a uvodni obrazovku, pak teprve delat dalsi akce
  
  // ----- displej -----

  SPI.end(); // release standard SPI pins
  SPI.begin(TFT_SCLK, SPI_MISO_UNUSED, TFT_MOSI, TFT_CS); // map and init SPI pins
  // For 1.44" and 1.8" TFT with ST7735 use:
  tft = new Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
  pinMode( TFT_BACKLIGHT, OUTPUT );
  digitalWrite( TFT_BACKLIGHT, HIGH );
  // Use this initializer if using a 1.8" TFT screen:
  tft->initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  // SPI speed defaults to SPI_DEFAULT_FREQ defined in the library, you can override it here
  // Note that speed allowable depends on chip and quality of wiring, if you go too fast, you
  // may end up with a black screen some times, or all the time.
  tft->setSPISpeed(40000000);
  tft->setRotation(1);

  // ----- extgfx -----
  // https://github.com/petrbrouzda/ExtGfx

  painter = new TextPainter( tft );
  
  // písmo pro běžné texty
  painter->createFontConfig( &malePismo, &PragatiNarrow_Regular16pt8b );
  malePismo.lineHeight = malePismo.lineHeight - 1;

  // nadpisy
  painter->createFontConfig( &vetsiPismo, &PragatiNarrow_Regular20pt8b );

  // horní lišta - počet družic
  painter->createFontConfig( &mikroPismo, &PragatiNarrow_Regular12pt8b );
  mikroPismo.firstLineHeightOffset -= 2;

  // vykreslime startup info
  uvodniObrazovka();

  // ---- ostatni inicializace --- 

  delay(2500); // aby urcite fungoval seriovy port
  Serial.println( "Startuju!" );

  // pri prvnim startu muze trvat 30 sekund!
  configProvider.openFsAndLoadConfig();

  webserver.startApAndWebserver();

  //  ---- GPS ----- 
  
  //  HW Serial port na daných pinech RX a TX
  // prvni parametr je RX, druhy TX - z pohledu mikrokontroleru
  hwSerial_1.begin(9600, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN );
  hwSerial_1.onReceive(serial_callback);

  tasker.setInterval( displayInfo, 10000 );

  // --- servo ----

  ESP32PWM::allocateTimer(0);
  servo1.setPeriodHertz(50);// Standard 50hz servo  
  servo1.attach(SERVO1);
  servo2.setPeriodHertz(50);// Standard 50hz servo  
  servo2.attach(SERVO2);

  servo1.write(SERVO1_ZAVRENO);
  servo2.write(SERVO2_ZAVRENO);


  // --- konfigurace

  char *p = (char*)config.getString( "target_lat", "50.0" );
  appState.targetLat = atof( p );
  p = (char*)config.getString( "target_lon", "15.0" );
  appState.targetLon = atof( p );
  appState.accuracy = config.getLong( "target_accuracy", 50 );


  // baterka

  adc.attach( ACCU_V );
  zmerAccu();
  delay(100);
  zmerAccu();
  uvodniObrazovka();

  // --- spusteni aplikace
  tasker.setTimeout( aktualizujStav, 5000 );  

  generujTextKey();
  
}


#define MAX_TEXTS 5

/** jednou za minutu zmeni texty, ktere se pouzivaji */
void generujTextKey() {
  randomSeed(analogRead(ACCU_V));
  appState.textKey = random(MAX_TEXTS);
  tasker.setTimeout( generujTextKey, 60000 );
}



void serial_callback()
{
  while (hwSerial_1.available() > 0) {
    gps.encode(hwSerial_1.read());
  }
}


void uvodniObrazovka()
{
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextColor(EG_GREEN);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, 80, 40, (char*)"STARTUJI");
  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, 80, 70, (char*)"Čekejte, prosím...");
  if( appState.accuVoltage > 0.0 ) {
    char text[100];
    sprintf( text, "Baterie %d %% (%.2f V)", appState.accuPercent, appState.accuVoltage );
    painter->printLabel( TextPainter::ALIGN_CENTER, 80, 100, text );
  }
}


int pocetMereni = 0;

// mereni baterky
void zmerAccu() {
  double vin = adc.readVoltage();
  double vreal = vin / (DELIC_R2/(DELIC_R1+DELIC_R2)) / KALIBRACE;
  if( appState.accuVoltage < 0 ) {
    // prvni mereni = rovnou nastavime
    appState.accuVoltage = vreal;
  } 
  // plovouci exponencialni prumer
  if( pocetMereni<10 ) {
    // prvnich nekolik mereni je potreba projet rychle
    appState.accuVoltage = 0.5 * appState.accuVoltage + 0.5 * vreal;
    tasker.setTimeout( zmerAccu, 1000 );
    pocetMereni++;
  } else {
    appState.accuVoltage = 0.8 * appState.accuVoltage + 0.2 * vreal;
    tasker.setTimeout( zmerAccu, 5000 );
  }
  serialLogger.log( "u=%.2f V (%.2f V)\n", appState.accuVoltage, vreal );
  appState.accuPercent = map_double( appState.accuVoltage, 3.2, 4.2, 0.0, 100.0 );
}



/**
 * Zde nastavte routy pro svou aplikaci.
 * Je zavolano z webserveru v dobe volani webserver.startWifiAndWebserver()
 */
void userRoutes( AsyncWebServer * server )
{
  server->on("/", HTTP_GET, onRequestRoot );
  server->on("/target", HTTP_GET, onRequestTarget );
  server->on("/a-target", HTTP_GET, onRequestTargetAction );
  server->on("/a-open", HTTP_GET, onRequestDoorOpen );
  server->on("/a-close", HTTP_GET, onRequestDoorClose );
  server->on("/debug", HTTP_GET, onRequestDebug );
  server->on("/a-debug", HTTP_GET, onRequestDebugAction );
  server->on("/servo", HTTP_GET, onRequestServo );
  server->on("/a-servo", HTTP_GET, onRequestServoAction );
  
}




// ----------- vlastni vykonna cast aplikace (loop)


long lastGpsBytes = 0;
boolean poziceUzByla = false;


void spoctiStav() {

  if( lastGpsBytes==gps.charsProcessed() ) {
    appState.stav = 0;
    return;
  }
  lastGpsBytes=gps.charsProcessed();

  if( !gps.satellites.isValid() || gps.satellites.value()==0 ) {
    appState.stav = 1;
    return;
  }

  if( !gps.location.isValid() ) {
    // nemame pozici
    appState.stav = 2;
    return;
  }

  poziceUzByla = true;

  // mame pozici!
  double distanceM = TinyGPSPlus::distanceBetween(
        gps.location.lat(),
        gps.location.lng(),
        appState.targetLat,
        appState.targetLon);
  appState.vzdalenostMetru = (int)distanceM;

  if( appState.vzdalenostMetru <= appState.accuracy ) {
    appState.stav = 20;
    if( !appState.dvereOtevreny ) {
      appState.servoRequestWaiting = true;
      appState.servoRequest = 1;
    }
  } else {
    if( gps.satellites.value()<6 ) {
      appState.stav = 10;
    } else {
      appState.stav = 11;
    }
  }
}

void vykresliBaterkuADruzice() {

  HbColorProfile c11( 0.0, EG_RED, EG_RED, EG_WHITE, EG_RED, EG_RED, EG_BLACK );
  HbColorProfile c12( 30.0, EG_YELLOW, EG_YELLOW, EG_BLACK, EG_YELLOW, EG_YELLOW, EG_BLACK );
  HbColorProfile c13( 50.0, EG_WHITE, EG_WHITE, EG_BLACK, EG_WHITE, EG_WHITE, EG_BLACK );
  // musi byt zakoncene NULLem; hodnoty musí být seřazené vzestupně
  HbColorProfile *colors1[] = { &c11, &c12, &c13, NULL };

  HorizontalBar hb1( tft, painter );
  hb1.setRange( 0, 100 );
#define ACCU_POS_X 135
  hb1.setPosition( ACCU_POS_X, 1, 160-ACCU_POS_X-1, 8 );
  hb1.setColors( (HbColorProfile**)&colors1 );
  hb1.setFont( &mikroPismo );

  hb1.setValue( (float)appState.accuPercent, (char*)"" );
  hb1.draw();

  if( gps.satellites.isValid() ) {
    char text[256];
    sprintf( text, "\x7f %d", gps.satellites.value() );
    if( gps.satellites.value()<5 ) {
      tft->setTextColor(EG_LIGHT_RED);
    } else {
      tft->setTextColor(EG_WHITE);
    }
    painter->setFont( &mikroPismo );
    painter->startText( 0, 0, 160, 128 );
    painter->printText( text );
  }
  
}

// nadpis
#define POS_X_NADPIS 80
#define POS_Y_NADPIS 15

// text
#define POS_X 0
#define POS_Y 37

void displayNeniGps() {
  tft->setTextColor(EG_LIGHT_RED);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS,  (char*)"Není spojení s GPS");

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  painter->printText( (char*)"Z GPS přijímače nepřicházejí žádná data. To je podivné.");

  vykresliBaterkuADruzice();
}


#define BEZNY_STARTUP_TIME_MS 90000
#define ROZSIRENY_TEXT_MS 60000


/**
 * Nemam pozici. Ukazuje se ale az 90+ sekund (BEZNY_STARTUP_TIME_MS) po startu.
 */
void displayNeniSignal() {
  tft->setTextColor(EG_LIGHT_RED);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS,  (char*)"Nevidím žádné družice");

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  char text[256];
  if( poziceUzByla ) {
    sprintf( text, "Abych fungoval správně, musím vidět na oblohu. Pokud jste vevnitř, běžte ven!" );
  } else {
    sprintf( text, "Abych fungoval správně, musím vidět na oblohu. Pokud jste vevnitř, běžte ven! (%d sekund od startu)", millis()/1000L );
  }
  painter->printText( text );

  vykresliBaterkuADruzice();
}



/**
 * Nemam pozici. Ukazuje se prvnich 90 sekund (BEZNY_STARTUP_TIME_MS) po startu.
 */
void displayStartupNeniSignal() {
  tft->setTextColor(EG_LIGHT_RED);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS, (char*)"Hledám, kde jsem");

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  char text[256];
  int t = ( BEZNY_STARTUP_TIME_MS - millis() ) / 1000L;
  sprintf( text, "Většinou to trvá do minuty a půl, pokud vidím na oblohu. Takže čekejte ještě %d sec.", t );
  if( millis() > ROZSIRENY_TEXT_MS ) {
    strcat( text, " Jsme venku a vidíme na oblohu?" );
  }
  painter->printText( text );
  vykresliBaterkuADruzice();  
}



void displayMaloSignalu() {
  tft->setTextColor(EG_YELLOW);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS, (char*)"Čekejte prosím...");

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  char text[256];
  sprintf( text, "Hledám, kde jsem. Vidím %d %s, ještě chvilku to bude trvat...", 
    gps.satellites.value(),
    mnozneCislo( gps.satellites.value(), "družici", "družice", "družic" )
  );
  painter->printText( text );

  vykresliBaterkuADruzice();
}


const char *nadpisy[] = {
  "Ještě tam nejsme...",
  "Tady se neotevřu!",
  "Hledej, šmudlo!",
  "Tady NE!",
  "Ještě kousek..."
};

const char * getNadpis() {
  return nadpisy[appState.textKey];
}


char textBuffer[256];

int kilometers[] = { 2, 3, 5, 10, 18, 30, 55, 72, 82, 90, 110, 150, 200, -1 };

// spárované s odpovídajícími nadpisy!
const char *textyKm[] = {
  "Cíl je víc než %d km odsud, takže trochu pohněte, ať už tam jsme.",
  "Otevřu se až v cíli, a ten je ještě víc než %d km daleko!",
  "Tady ale nic nenajdeš, k cíli zbývá ještě minimálně %d km.",
  "A támhle taky ne. Ještě je to kousek cesty, alespoň %d km.",
  "... teda kousek, no, trochu víc. %d km a možná ještě o trochu víc."
};

void textProVicKm() {
  int kmsOffset = 1;
  int vzdalenostKm = appState.vzdalenostMetru / 1000;
  for( int i=0; i<sizeof(kilometers); i++ ) {
    if( kilometers[i]==-1 ) {
      kmsOffset = i-1;
      break;
    }
    if( kilometers[i]>vzdalenostKm ) {
      kmsOffset = i-1;
      break;
    }
  }
  sprintf( textBuffer, textyKm[appState.textKey], kilometers[kmsOffset] );
}


int meters[] = { 1, 10, 20, 50, 100, 200, 300, 500, 750, 1000, 1250, 1500, 1700, 2000, -1 };

// spárované s odpovídajícími nadpisy!
const char *textyM[] = {
  "... ale už je to jen o trochu víc než %d metrů! Utíkejte!",
  "Otevřu se až v cíli, a ten není blíž než %d metrů!",
  "K cíli zbývá ještě o trochu víc než %d m.",
  "Tááámhle už ale ano! Je to jen o ždibec dál než %d metrů!",
  "... fakt malý kousek. Jen něco přes %d metrů!"
};

void textProBlizko() {
  int mOffset = 1;
  for( int i=0; i<sizeof(meters); i++ ) {
    if( meters[i]==-1 ) {
      mOffset = i-1;
      break;
    }
    if( meters[i]>appState.vzdalenostMetru ) {
      mOffset = i-1;
      break;
    }
  }
  sprintf( textBuffer, textyM[appState.textKey], meters[mOffset] );
}

void textProVzdalenost() {
  if( appState.vzdalenostMetru>2000 ) {
    textProVicKm();    
  } else {
    textProBlizko();
  }
}


void displayVzdalenost( bool spatnySignal ) {
  textProVzdalenost();
  if( appState.accuPercent < 20 ) {
    sprintf( textBuffer+strlen(textBuffer), " A dochází baterka, dobijte mě!");
  }
  if( spatnySignal ) {
    sprintf( textBuffer+strlen(textBuffer), " A špatně vidím na oblohu, vidím jen %d %s, zkuste se nějak posunout.",
        gps.satellites.value(),
        mnozneCislo( gps.satellites.value(), "družici", "družice", "družic" )
    );
  }
  
  tft->setTextColor(EG_YELLOW);
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS, (char*)getNadpis());

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  painter->printText( textBuffer );

  vykresliBaterkuADruzice();
}



void displayHura() {
  tft->setTextColor(EG_GREEN );
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, POS_X_NADPIS, POS_Y_NADPIS, (char*)"Hurá!");

  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( POS_X, POS_Y, 160-POS_X-POS_X, 128-POS_Y-1 );
  painter->printText( "Dosáhli jste cíle! Zámek je odemčen, můžete sundat víko. (Pokud se nepootevřelo samo, zamáčkněte ho, ať se zámky mohou uvolnit.)" );
  vykresliBaterkuADruzice();
}



void displayStav() {
  tft->fillScreen(ST77XX_BLACK);
  if( appState.dvereOtevreny ) {
    displayHura();
    return;
  }
  switch( appState.stav ) {
    // chybovy stav: nefunguje GPS
    case 0:   displayNeniGps(); break;
    case 1:   
      if( millis() < BEZNY_STARTUP_TIME_MS ) {
        displayStartupNeniSignal(); 
      } else {
        displayNeniSignal(); 
      }
      break;
    case 2:   displayMaloSignalu(); break;
    case 10:  displayVzdalenost( true ); break;
    case 11:  displayVzdalenost( false ); break;
  }
}




/**
 * volano Taskerem, spocte stav aplikace a vykresli displej
 */
void aktualizujStav() {

  if( !appState.debugMode ) {
    spoctiStav();
  }
  displayStav();   

  tasker.setTimeout( aktualizujStav, 5000 );
}


/**
 * volano Taskerem
 */
void displayInfo()
{
  serialLogger.log( "---- chars %d, sentsFix %d, failchksm %d", 
    gps.charsProcessed(),
    gps.sentencesWithFix(), 
    gps.failedChecksum() );

  if(  gps.satellites.isValid() ) {
    serialLogger.log( "Satelites: %d", gps.satellites.value());
  } else {
    serialLogger.log( "Satelites: ***");
  }
  if( gps.hdop.isValid() ) {
    serialLogger.log( "HDOP: %.2f", gps.hdop.hdop() );
  } else {
    serialLogger.log( "HDOP: ***");
  }
  
  if (gps.location.isValid())
  {
    serialLogger.log( "Location: %.6f, %.6f", gps.location.lat(), gps.location.lng() );
  }
  else
  {
    serialLogger.log( "Location: INVALID" );
  }

  if (gps.date.isValid() && gps.time.isValid() )
  {
    serialLogger.log( "Time: %d.%d.%d %02d:%02d:%02d",
      gps.date.day(),
      gps.date.month(),
      gps.date.year(),
      gps.time.hour(),
      gps.time.minute(),
      gps.time.second() );
  }
}


void onConfigChanged() 
{
    saveConfigChange=false;
    configProvider.saveConfig();
    
    // vypsat konfiguraci
    Serial.println( "--- zacatek konfigurace" );
    config.printTo(Serial);
    Serial.println( "--- konec konfigurace" );
}



void provedAkci() {
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextColor(EG_GREEN );
  painter->setFont( &vetsiPismo );
  painter->printLabel( TextPainter::ALIGN_CENTER, 80, 20, appState.servoRequest==1 ? (char*)"Otevírám!" : (char*)"Zavírám!" );
  tft->setTextColor(EG_WHITE);
  painter->setFont( &malePismo );
  painter->startText( 10, 50, 160-10-10, 128-50-1 );
  tft->setTextColor(EG_WHITE);
  painter->printText( "Čekejte prosím..." );

  if( appState.servoRequest == 1 ) {
    // otevirani
    for( int i = 0; i<180; i++ ) {
      int pos = i*SERVO1_SMER_OTEVIRANI + SERVO1_ZAVRENO;
      if( i > abs(SERVO1_OTEVRENO-SERVO1_ZAVRENO) ) {
        pos = SERVO1_OTEVRENO;
      }
      servo1.write(pos);

      pos = i*SERVO2_SMER_OTEVIRANI + SERVO2_ZAVRENO;
      if( i > abs(SERVO2_OTEVRENO-SERVO2_ZAVRENO) ) {
        pos = SERVO2_OTEVRENO;
      }
      servo2.write(pos);

      delay( 30 );
    }
    appState.dvereOtevreny = true;
  } else {
    // zavirani
    for( int i = 0; i<180; i++ ) {
      int pos = -i*SERVO1_SMER_OTEVIRANI + SERVO1_OTEVRENO;
      if( i > abs(SERVO1_OTEVRENO-SERVO1_ZAVRENO) ) {
        pos = SERVO1_ZAVRENO;
      }
      servo1.write(pos);

      pos = -i*SERVO2_SMER_OTEVIRANI + SERVO2_OTEVRENO;
      if( i > abs(SERVO2_OTEVRENO-SERVO2_ZAVRENO) ) {
        pos = SERVO2_ZAVRENO;
      }
      servo2.write(pos);

      delay( 30 );
    }
    appState.dvereOtevreny = false;
  }

  appState.servoRequest = 0;
  appState.servoRequestWaiting = false;  

  displayStav();
}


void loop() {

  // vypiseme asynchronni log, pokud v nem neco je
  asyncLogger.dumpTo( &Serial );

  // aby Tasker spoustel ulohy
  tasker.loop();

  // odbavit DNS pozadavky
  webserver.process();

  // pokud uzivatel zmenil nastaveni, ulozit do souboru a promitnout, kde je potreba
  // nezavesujeme primo na config.isDirty(), protoze bychom mohli trefit nekonzistenci pri nastavovani asynchronne z webserveru
  if( saveConfigChange ) {
    onConfigChanged();
  }

  if( appState.servoRequestWaiting ) {
    provedAkci();
  }

  // na strance 192.168.10.1/servo je mozne rucne nastavit uhel serva - aby se 
  // daly spravne nakonfigurovat hodnoty pro otevirani a zavirani
  if( appState.servoDirectCommand ) {
    servo1.write( appState.servo1Direct);
    servo2.write( appState.servo2Direct);
    appState.servoDirectCommand = false;
  }

}




// -------------- odsud dal je webserver

const char htmlHlavicka[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
    <title>GeoVault</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html {font-family: Arial; display: inline-block; text-align: left;}
      h2 {font-size: 2.0rem;}
      h3 {font-size: 1.65rem; font-weight: 600}
      p {font-size: 1.4rem;}
      input {font-size: 1.4rem;}
      input#text {width: 100%;}
      select {font-size: 1.4rem;}
      form {font-size: 1.4rem;}
      body {max-width: 600px; margin:10px ; padding-bottom: 25px;}
    </style>
  </head>
  <body>
)rawliteral";

const char htmlPaticka[] PROGMEM = R"rawliteral(
  </body>
  </html>
)rawliteral";




const char htmlZacatek[] PROGMEM = R"rawliteral(
  <h1>GeoVault</h1>
  <form method="GET" action="/?">
  <input type="submit" name="obnov" value="Obnov stav" > 
  </form>
)rawliteral";





void vlozInformace( AsyncResponseStream *response )  {
  if( appState.isProblem() ) {
    response->printf( "<p><b>%s:</b> [%s] před %d sec.</p>",
      appState.globalState==ERROR ? "Chyba" : "Varování",
      appState.problemDesc,
      (millis()-appState.problemTime) / 1000L
    );  
  }

  response->print( "<p><b>GPS:</b>" );
  response->print( "<br>" );
  if( gps.satellites.isValid() ) {
    response->printf( "- družic: %d, hdop %.1f m", 
      gps.satellites.value(),
      gps.hdop.isValid() ?  gps.hdop.hdop() : -1.0  );
  } else {
    response->printf( "- družic: ***" );
  }
  response->print( "<br>" );
  if( gps.location.isValid()) {
    response->printf( "- pos: %f %f", 
      gps.location.lat(), 
      gps.location.lng()
    );
  } else {
    response->printf( "- pos: ***" );
  }
  response->print( "</p><p>" );

  if( gps.location.isValid()) {
    double distanceKm = TinyGPSPlus::distanceBetween(
        gps.location.lat(),
        gps.location.lng(),
        appState.targetLat,
        appState.targetLon) 
        / 1000.0;
    double courseTo = TinyGPSPlus::courseTo(
        gps.location.lat(),
        gps.location.lng(),
        appState.targetLat,
        appState.targetLon);
    response->printf( "Cíl je %.2f km azimutem %.0f.",  distanceKm, courseTo );
  }

  response->printf( "<br>Dveře: %s.", appState.dvereOtevreny ? "otevřeny" : "zavřeny" );
  response->printf("<br>Baterie %d %% (%.2f V)</p>", appState.accuPercent, appState.accuVoltage );
}


void vlozUptime( AsyncResponseStream *response ) {
  response->printf( "<br><hr><br><p>Čas od spuštění zařízení: %d min</p>",
    millis()/60000L
  );
}


/** 
 * Je lepe misto Serial.print pouzivat AsyncLogger.
 * Je volano z webserveru asynchronne.
 * Nevolat odsud dlouhotrvajici akce!
 * I logovani by melo byt pres asyncLogger!
 * 
 * Kazda funkce onRequest* musi byt zaregistrovana v userRoutes()
 * 
 */
void onRequestRoot(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req root" );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( htmlZacatek );
  vlozInformace( response );
  response->print( "<hr>" );
  response->print( "<form method=\"GET\" action=\"/a-open?\"><input type=\"submit\" name=\"open\" value=\"Otevři!\"></form>" );
  response->print( "<br>&nbsp;<br>" );
  response->print( "<form method=\"GET\" action=\"/a-close?\"><input type=\"submit\" name=\"close\" value=\"Zavři!\"></form>" );
  response->print( "<hr><p><a href=\"/target\">Nastavení cíle</a></p>" );

  vlozUptime( response );
  response->print( htmlPaticka );
  request->send(response);
}


void onRequestTarget(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req target" );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( "<h1>Nastavení cíle</h1>" );
  response->print( "<p><a href=\"/\">Zpět</a></p>" );
  response->print( "<form method=\"GET\" action=\"/a-target\">" );
  response->printf( "<br>Šířka (50.123):<br><input type=\"text\" name=\"lat\" value=\"%f\">",  appState.targetLat );
  response->printf( "<br>Délka (15.1234):<br><input type=\"text\" name=\"lon\" value=\"%f\">",  appState.targetLon );
  response->printf( "<br>Povolená odchylka [m]:<br><input type=\"text\" name=\"acc\" value=\"%d\">",  appState.accuracy );
  response->print( "<br><input type=\"submit\" name=\"save\" value=\"Nastav!\"></form>" );

  vlozUptime( response );
  response->print( htmlPaticka );
  request->send(response);
}


void onRequestTargetAction(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req target save" );

  const char * p = webserver.getQueryParamAsString( request, "lat", "50.0" );
  config.setValue( "target_lat", p );
  appState.targetLat = atof( p );

  p = webserver.getQueryParamAsString( request, "lon", "15.0" );
  config.setValue( "target_lon", p );
  appState.targetLon = atof( p );

  long v = webserver.getQueryParamAsLong( request, "acc", 100 );
  config.setValue( "target_accuracy", v );
  appState.accuracy = v;

  saveConfigChange = true;

  request->redirect( "/target" );
}


void onRequestDebug(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req debug" );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( "<h1>Debug</h1>" );
  response->print( "<p><a href=\"/\">Zpět</a></p>" );
  response->print( "<form method=\"GET\" action=\"/a-debug\">" );
  response->printf( "<br>Vzdálenost [m]:<br><input type=\"text\" name=\"vzd\" value=\"%d\">",  appState.vzdalenostMetru );
  response->printf( "<br>Stav 10-11:<br><input type=\"text\" name=\"stav\" value=\"%d\">",  appState.stav );
  response->print( "<br><input type=\"submit\" name=\"save\" value=\"Nastav!\"></form>" );

  vlozUptime( response );
  response->print( htmlPaticka );
  request->send(response);
}

void onRequestDebugAction(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req debug action" );

  long vzd = webserver.getQueryParamAsLong( request, "vzd", 100 );

  appState.debugMode = true;

  appState.vzdalenostMetru = vzd;
  appState.stav = webserver.getQueryParamAsLong( request, "stav", 11 );;

  if( vzd < appState.accuracy ) {
    appState.servoRequestWaiting = true;
    appState.servoRequest = 1;
    appState.stav = 20;
  }

  request->redirect( "/debug" );
}

void onRequestServo(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req servo" );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( "<h1>Servo</h1>" );
  response->print( "<p><a href=\"/\">Zpět</a></p>" );
  response->print( "<form method=\"GET\" action=\"/a-servo\">" );
  response->printf( "<br>Servo 1 (0-180):<br><input type=\"text\" name=\"s1\" value=\"%d\">",  appState.servo1Direct );
  response->printf( "<br>Servo 2 (0-180):<br><input type=\"text\" name=\"s2\" value=\"%d\">",  appState.servo2Direct );
  response->print( "<br><input type=\"submit\" name=\"save\" value=\"Nastav!\"></form>" );

  vlozUptime( response );
  response->print( htmlPaticka );
  request->send(response);
}

void onRequestServoAction(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req servo action" );

  appState.servo1Direct = webserver.getQueryParamAsLong( request, "s1", 100 );
  appState.servo2Direct = webserver.getQueryParamAsLong( request, "s2", 100 );
  appState.servoDirectCommand = true;

  request->redirect( "/servo" );
}


void onRequestDoorOpen(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req open" );

  appState.servoRequestWaiting = true;
  appState.servoRequest = 1;

  request->redirect( "/" );
}


void onRequestDoorClose(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req close" );

  appState.servoRequestWaiting = true;
  appState.servoRequest = 2;

  request->redirect( "/" );
}


/**
ESP32 2.0.17

Using library Async TCP at version 3.4.0 in folder: E:\dev.moje\arduino\libraries\Async_TCP 
Using library ESP Async WebServer at version 3.7.7 in folder: E:\dev.moje\arduino\libraries\ESP_Async_WebServer 
Using library Tasker at version 2.0.3 in folder: E:\dev.moje\arduino\libraries\Tasker 
Using library Adafruit GFX Library at version 1.12.0 in folder: E:\dev.moje\arduino\libraries\Adafruit_GFX_Library 
Using library Adafruit BusIO at version 1.17.0 in folder: E:\dev.moje\arduino\libraries\Adafruit_BusIO 
Using library Adafruit ST7735 and ST7789 Library at version 1.11.0 in folder: E:\dev.moje\arduino\libraries\Adafruit_ST7735_and_ST7789_Library 
Using library TinyGPSPlus at version 1.0.3 in folder: E:\dev.moje\arduino\libraries\TinyGPSPlus 
Using library ESP32Servo at version 3.0.7 in folder: E:\dev.moje\arduino\libraries\ESP32Servo 
Using library ESP32AnalogRead at version 0.3.0 in folder: E:\dev.moje\arduino\libraries\ESP32AnalogRead 
Using library DNSServer at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\DNSServer 
Using library FS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\FS 
Using library WiFi at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\WiFi 
Using library Wire at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\Wire 
Using library SPI at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\SPI 
Using library SPIFFS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\SPIFFS 

*/
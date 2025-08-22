#ifndef __PINOUT_H__
#define __PINOUT_H__

// ------------ displej ---------------------

#define TFT_CS   5
#define TFT_RST  3
#define TFT_DC   1
#define TFT_MOSI 6
#define TFT_SCLK 4
#define SPI_MISO_UNUSED -1
// LOW = vypnuto, HIGH = zapnuto
#define TFT_BACKLIGHT 8

// ------------ GPS ----------------------

/** ESP32 TX - GPS RX */
#define GPS_RX_PIN 21
/** ESP32 RX - GPS Tx */
#define GPS_TX_PIN 20
#define GPS_BAUD 9600

// ------------ servo ---------------------

#define SERVO1 7
#define SERVO2 10

#define SERVO1_OTEVRENO 20
#define SERVO1_ZAVRENO 130
#define SERVO1_SMER_OTEVIRANI -1

#define SERVO2_OTEVRENO 20
#define SERVO2_ZAVRENO 130
#define SERVO2_SMER_OTEVIRANI -1


// ------------ akumulator ---------------------

#define ACCU_V 0

// POZOR! Hodnota musi byt float, takze .0 na konci
#define DELIC_R1 68000.0
// POZOR! Hodnota musi byt float, takze .0 na konci
#define DELIC_R2 20000.0
// kalibrace - hodnota namerena / hodnota spravna
#define KALIBRACE (2.90/3.72)

#endif

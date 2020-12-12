#include "ip_connection.h"
#include "bricklet_temperature.h"
#include "bricklet_humidity.h"
#include "bricklet_dual_relay.h"

#ifndef CELLAR_DEFINITIONS
#define CELLAR_DEFINITIONS

//
// Parameter zur Berechnung der absoluten Luftfeuchtigkeit
// absolute_humidity=relative_humidity*MH20/(RR*(temp+CTOK))*
//					(WA0+temp*(WA1+temp*(WA2+temp*(WA3+temp*(WA4+temp*(WA5+temp*WA6))))))
//
#define CTOK 273.15 		// Temperature (C) = Temperature (K) - 273.15
#define RR 8.31447215       // (Ideale) Gas Konstante J/K/mol
#define MH20 18.01528		// Molmasse Wasser in g/mol

#define WA0 6.107799961
#define WA1 4.436518521e-1
#define WA2 1.428945805e-2
#define WA3 2.650648471e-4
#define WA4 3.031240396e-6
#define WA5 2.034080948e-8
#define WA6 6.136820929e-11

//
// Parameter zur Berechnung des Taupunktes
//  dewpoint=(CON1*log(relative_humidity)+CON3*temp/(CON1+temp))/
//			(CON2-log(relative_humidity/100)-CON2*temp/(CON1+temp))
//
#define CON1 241.2
#define CON2 17.5043
#define CON3 4222.03716

//
// Parameter zur Berechnung der relativen Luftfeuchtigkeit für LIM=0
// (Lowest Isopleth of Mould). Unterhalb von LIM 0 ist mit keiner biologischen
// Aktivität (z.B. Myzellenwachstum) zu rechnen.
// relative_humidity(LIM=0) = (LIM_CON2*exp(LIM_CON1*temp)+LIM_CON3)*100;
//
//
#define LIM_CON1 -0.12
#define LIM_CON2 0.244
#define LIM_CON3 0.775

//
// Werte für "action" in operate_window Funktion
//
#define OPEN_WINDOW 1
#define CLOSE_WINDOW 0

//
// Werte für den Humidity Status
//
#define IN_DRY_OUT_HUMID 0
#define IN_HUMID_OUT_MORE_HUMID 1
#define IN_MORE_DRY_OUT_DRY 2
#define IN_DRY_OUT_MORE_DRY 3
#define IN_HUMID_OUT_DRY 4
#define IN_MORE_HUMID_OUT_HUMID 5
#define IN_DRY_OUT_TARGET 6
#define IN_HUMID_OUT_TARGET 7
#define IN_HUMID_OUT_HUMID 8
#define IN_DRY_OUT_DRY 9
#define IN_TARGET_OUT_DRY 10
#define IN_TARGET_OUT_HUMID 11
#define IN_TARGET_OUT_TARGET 12

//
// Nützliche Hilfsfunktionen
//
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//
// Fenster öffnen und schliessen
// action ist entweder OPEN_WINDOW oder CLOSE_WINDOW
//
int operate_window(int action, DualRelay* dr);

//
// Berechnung der absoluten Luftfeuchtigkeit aus der relativen
//
float absolute_humidity(float relative_humidity, float temp);

//
// Berechnung der relativen Luftfeuchtigkeit aus der absoluten
//
float relative_humidity(float absolute_humidity, float temp);

//
// Berechnung des Taupunkts
//
float dewpoint(float relative_humidity, float temp);

//
// LIM = Relative Luftfeuchtigkeit unter der kein Schimmelwachstum möglich ist
//
float lim(float temp);

//
// Ermittelung des humidity status auf Basis der drei relativen Luftfeuchtigkeiten
//
// rh_d = Relative Luftfeuchtigkeit der Aussenluft bei Kellertemperatur
// rh_s = Maximale Soll relative Luftfeuchtigkeit im Keller
// rh_i = Relative Lufteuchtigkeit im Keller
//
// Der humidity state gibt die Verhältnisse der drei Feuchtigkeitswerte an
//
// rh_d > rh_s > rh_i: humidity state -> IN_DRY_OUT_HUMID
// rh_d > rh_i > rh_s: humidity state -> IN_HUMID_OUT_MORE_HUMID
// rh_s > rh_d > rh_i: humidity state -> IN_MORE_DRY_OUT_DRY
// rh_s > rh_i > rh_d: humidity state -> IN_DRY_OUT_MORE_DRY
// rh_i > rh_s > rh_d: humidity state -> IN_HUMID_OUT_DRY
// rh_i > rh_d > rh_s: humidity state -> IN_MORE_HUMID_OUT_HUMID

// rh_d = rh_s > rh_i: humidity state -> IN_DRY_OUT_TARGET
// rh_d = rh_s < rh_i: humidity state -> IN_HUMID_OUT_TARGET
// rh_d = rh_i > rh_s: humidity state -> IN_HUMID_OUT_HUMID
// rh_d = rh_i < rh_s: humidity state -> IN_DRY_OUT_DRY
// rh_s = rh_i > rh_d: humidity state -> IN_TARGET_OUT_DRY
// rh_s = rh_i < rh_d: humidity state -> IN_TARGET_OUT_HUMID
// rh_s = rh_i = rh_d: humidity state -> IN_TARGET_OUT_TARGET
//
int humiditystate(float rh_d, float rh_s, float rh_i);

//
// Write Window state to influxdb
//
int write_window_state(char* host, char* port, char* db, char* room, int status,
                                int humstate, float crhum_ak, float lim, float min_rhum);

//
// Write Room environment data to influxdb
//
int write_room_data(char* host, char* port, char* db,
					char* room, char* sensor,
					float temp, float rhum, float ahum, float dtemp, float lim);
//
//  Get last Window state from influxdb
//
size_t writeFunction(char *ptr, size_t size, size_t nmemb, char *data);
int get_window_state(char* host, char* port, char* db, char* room, int* status);

#endif

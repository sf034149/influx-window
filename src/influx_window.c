#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>


#include "ip_connection.h"
#include "bricklet_temperature.h"
#include "bricklet_humidity.h"
#include "bricklet_dual_relay.h"
#include "cellar_functions.h"

// tinkerforge host und port
#define HOST "sras1.steininger.eu"
#define PORT 4223
#define INFLUX_HOST "dock2.steininger.eu"
#define INFLUX_PORT "8086"
#define INFLUX_DB "Kollo16"
#define ROOMS 4

// SOll und DELTA Werte zur optimalen Trocknung (hoffentlich)
#define RH_SOLL_DELTA_LIM 10.0 // Angestrebte Luftfeuchtigkeit noch 10%-Punkte unter LIM
#define RH_D_DELTA_WINDOW_OPEN 4.0 // bei offenem Fenster Delta zu Draussen zum Schalten
#define RH_D_DELTA_WINDOW_CLOSED 8.0 // bei geschl. Fenster Delta Draussen zum Schalten


#define SENSOR_KL 0
#define SENSOR_KW 1
#define SENSOR_AK 2
#define SENSOR_KH 3

int  main() {
    int room=0;
    int humidity_state=0;
    int last_humidity_state=0;
    int window_state;
    int last_window_state;
    uint16_t rhumarray[ROOMS];
    int16_t temparray[ROOMS];
    float ahumarray[ROOMS];
    float dtemparray[ROOMS];
    float min_temp;
    float max_temp;
    float min_rhum;

    Humidity h;
    Temperature t;
    DualRelay dr_KW;
    DualRelay dr_KH;
    uint8_t ret_mode;

    // Raum und Sensor Initialisierung
    // {Raumkürzel, UID_hum, UID_temp, UID_dual, Sensor ID}
    //


     char  sensdef[ROOMS][5][20]= {{"KL", "fR6", "dF3", "xxx", "001"},
                           {"KW", "fNF", "dzU", "Acp", "003"},
	                   {"AK", "kdC", "ng8", "xxx", "006"},
			   {"KH", "BZZ", "zue", "AdB", "007"}};


    // Massnahmen Initialisierung
    // [humidity_state]
    //		{Fenster:zu=0/auf=1, Entfeuchter:aus=0/an=1, State schlecht=0/gut=1}
    int action_entfeuchter[13][3]= {
        {CLOSE_WINDOW,0,1}, // IN_DRY_OUT_HUMID
        {CLOSE_WINDOW,1,0}, // IN_HUMID_OUT_MORE_HUMID
        {CLOSE_WINDOW,0,1}, // IN_MORE_DRY_OUT_DRY
        {OPEN_WINDOW,0,1}, // IN_DRY_OUT_MORE_DRY
        {OPEN_WINDOW,0,0}, // IN_HUMID_OUT_DRY
        {CLOSE_WINDOW,1,0}, // IN_MORE_HUMID_OUT_HUMID
        {CLOSE_WINDOW,0,1}, // IN_DRY_OUT_TARGET
        {OPEN_WINDOW,0,0}, // IN_HUMID_OUT_TARGET
        {CLOSE_WINDOW,1,0}, // IN_HUMID_OUT_HUMID
        {CLOSE_WINDOW,0,1}, // IN_DRY_OUT_DRY
    	{OPEN_WINDOW,0,1}, // IN_TARGET_OUT_DRY
        {CLOSE_WINDOW,0,1}, // IN_TARGET_OUT_HUMID
        {CLOSE_WINDOW,0,1}  // IN_TARGET_OUT_TARGET
        };
    // [humidity_state]
    //		{fenster:zu=0/auf=1, state schlecht=0/gut=1}
    int action[13][2]= {
        {CLOSE_WINDOW,1}, // IN_DRY_OUT_HUMID
        {CLOSE_WINDOW,0}, // IN_HUMID_OUT_MORE_HUMID
        {CLOSE_WINDOW,1}, // IN_MORE_DRY_OUT_DRY
        {OPEN_WINDOW,1}, // IN_DRY_OUT_MORE_DRY
        {OPEN_WINDOW,0}, // IN_HUMID_OUT_DRY
        {OPEN_WINDOW,0}, // IN_MORE_HUMID_OUT_HUMID -> hier könnte man auch das Fenster  schliessen
        {CLOSE_WINDOW,1}, // IN_DRY_OUT_TARGET
        {OPEN_WINDOW,0}, // IN_HUMID_OUT_TARGET
        {CLOSE_WINDOW,0}, // IN_HUMID_OUT_HUMID
        {CLOSE_WINDOW,1}, // IN_DRY_OUT_DRY
        {OPEN_WINDOW,1}, // IN_TARGET_OUT_DRY
        {CLOSE_WINDOW,1}, // IN_TARGET_OUT_HUMID
        {CLOSE_WINDOW,1}  // IN_TARGET_OUT_TARGET
        };

    // Create IP connection to tinkerforge HOST
    IPConnection ipcon;
    ipcon_create(&ipcon);

    // Connect to brickd
    if(ipcon_connect(&ipcon, HOST, PORT) < 0) {
            fprintf(stderr, "Could not connect\n to host:port %s:%i", HOST, PORT);
            exit(1);
    }

    // Create Dualrelay Object for all windows
    dual_relay_create(&dr_KW, sensdef[1][3], &ipcon);
    dual_relay_create(&dr_KH, sensdef[3][3], &ipcon);


    // Hauptschleife
    for (room=0;room<=ROOMS-1;room++) {

	humidity_create(&h, sensdef[room][1], &ipcon);
        temperature_create (&t, sensdef[room][2], &ipcon);

        // Korrektur der Frequenz des Temperatur
        // Bricklets im Heizungskeller um die Aussreisser
        // zu vermeiden
            if (room==3) {
                temperature_get_i2c_mode(&t, &ret_mode);
                if (ret_mode == TEMPERATURE_I2C_MODE_FAST) {
                    temperature_set_i2c_mode(&t, TEMPERATURE_I2C_MODE_SLOW);
                }
           }

	    // Get current humidity (unit is %RH/10)
        if(humidity_get_humidity(&h, &(rhumarray[room])) < 0) {
        	fprintf(stderr, "Could not get value, probably timeout\n");
            exit(1);
        }

        // Get current temperature (unit is °C/100)
	// Take care of wromg measurements in KH
	do {
            if(temperature_get_temperature(&t, &(temparray[room])) < 0) {
                fprintf(stderr, "Could not get value, probably timeout\n");
                exit(1);
            }
        } while (room==3 && (temparray[room]/100.0 < 8.0 || temparray[room]/100.0 > 30.0));


	ahumarray[room]=(float) absolute_humidity(rhumarray[room]/10.0, temparray[room]/100.0);

	write_room_data(INFLUX_HOST, INFLUX_PORT, INFLUX_DB,
			sensdef[room][0],
			sensdef[room][4],
			temparray[room]/100.0,
			rhumarray[room]/10.0,
			ahumarray[room],
			dewpoint(rhumarray[room], temparray[room]/100.0),
			lim(temparray[room]/100.0));
    }
/*
    min_temp=MIN(temparray[SENSOR_KW],MIN(temparray[SENSOR_KL],temparray[SENSOR_KH]));
    max_temp=MAX(temparray[SENSOR_KW],MAX(temparray[SENSOR_KL],temparray[SENSOR_KH]));
    min_rhum=MIN(rhumarray[SENSOR_KW],MIN(rhumarray[SENSOR_KL],rhumarray[SENSOR_KH])); */

    //get last  window_state
    get_window_state(INFLUX_HOST, INFLUX_PORT, INFLUX_DB, sensdef[1][0], &last_window_state);

//	last_window_state=CLOSE_WINDOW;
	fprintf(stderr, "Last Window State %i\n", last_window_state);

    if (last_window_state == CLOSE_WINDOW) {
	humidity_state = humiditystate(
		relative_humidity(ahumarray[SENSOR_AK],temparray[SENSOR_KW]/100.0)+RH_D_DELTA_WINDOW_CLOSED,
		lim(temparray[SENSOR_KW]/100.0)-RH_SOLL_DELTA_LIM,
		rhumarray[SENSOR_KW]/10.0);
    }
    else  {
	humidity_state = humiditystate(
		relative_humidity(ahumarray[SENSOR_AK],temparray[SENSOR_KW]/100.0)+RH_D_DELTA_WINDOW_OPEN,
            	lim(temparray[SENSOR_KW]/100.0)-RH_SOLL_DELTA_LIM,
            	rhumarray[SENSOR_KW]/10.0);
       }

    // Action Fenster offen/zu in Datenbank schreiben
    write_window_state(INFLUX_HOST, INFLUX_PORT, INFLUX_DB, sensdef[1][0], action[humidity_state][0],
			humidity_state, relative_humidity(ahumarray[SENSOR_AK],temparray[SENSOR_KW]/100.0),
			lim(temparray[SENSOR_KW]/100.0), rhumarray[SENSOR_KW]/10.0);

    /* write_window_state(INFLUX_HOST, INFLUX_PORT, INFLUX_DB, sensdef[3][0], action[humidity_state][0],
                       humidity_state, relative_humidity(ahumarray[SENSOR_AK],temparray[SENSOR_KH]/100.0),
                        lim(temparray[SENSOR_KH]/100.0), rhumarray[SENSOR_KH]/10.0); */

    //Fenster öffenen oder schliessen wenn sich was geändert hat


	fprintf (stderr, "Neuer Status %i, Alter Status %i\n", action[humidity_state][0], last_window_state);

    if (action[humidity_state][0] == OPEN_WINDOW && last_window_state == CLOSE_WINDOW)
	operate_window(OPEN_WINDOW, &dr_KW);
    else if (action[humidity_state][0] == CLOSE_WINDOW && last_window_state == OPEN_WINDOW)
   	operate_window(CLOSE_WINDOW, &dr_KW);


    temperature_destroy(&t);
    humidity_destroy(&h);
    dual_relay_destroy(&dr_KW);
    dual_relay_destroy(&dr_KH);

    // Calls ipcon_disconnect internally
    ipcon_destroy(&ipcon);
}






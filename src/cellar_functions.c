#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <curl/curl.h>

#define IPCON_EXPOSE_MILLISLEEP

#include "cellar_functions.h"
#include "ip_connection.h"
#include "bricklet_temperature.h"
#include "bricklet_humidity.h"
#include "bricklet_dual_relay.h"

//
// Funktion zum Öffnen und Schliessen des Fensters:
//
int operate_window(int action, DualRelay* dr)
{
	//erstmal alles ausschalten
	dual_relay_set_state(dr, false, false);
	millisleep (500);

switch (action) {

case OPEN_WINDOW:
	//Das Power Relay noch auf "AUS" stehen lassen und das Richtungsrelay auf "AUF" stellen
	dual_relay_set_state(dr, false, true);
	millisleep (500);
	// Jetzt das Power Relay per Monoflop (sicherheitshalber) 9 sekunden anschalten
	dual_relay_set_monoflop(dr, 1, true,9000);
	millisleep(10000);
	//jetzt alles ausschalten
	dual_relay_set_state(dr, false, false);
	break;

case CLOSE_WINDOW:
	//Das Power Relay noch auf "AUS" stehen lassen und das Richtungsrelay auf "ZU" stellen
	dual_relay_set_state(dr, false, false);
	millisleep (500);
	// Jetzt das Power Relay per Monoflop (sicherheitshalber) 15 sekunden anschalten
	dual_relay_set_monoflop(dr, 1, true,15000);
	millisleep(16000);
	//jetzt vorsichtshalber nochmal alles ausschalten
	dual_relay_set_state(dr, false, false);
	millisleep(500);
	//Das Power Relay noch auf "AUS" stehen lassen und das Richtungsrelay auf "AUF" stellen
	dual_relay_set_state(dr, false, true);
	millisleep (500);
 	// Jetzt das Power Relay per Monoflop 1,9 sekunden anschalten um den Druck zu reduzieren
	dual_relay_set_monoflop(dr, 1, true, 1900);
 	millisleep(2900);
 	//jetzt alles ausschalten
 	dual_relay_set_state(dr, false, false);
	}
	return 0;
}


//
// Berechnung der absoluten Luftfeuchtigkeit aus der relativen
//
float absolute_humidity(float relative_humidity, float temp)
{
  double absolute_humidity, ewater;
  ewater=WA0+temp*(WA1+temp*(WA2+temp*(WA3+temp*(WA4+temp*(WA5+temp*WA6)))));
  absolute_humidity=relative_humidity*ewater*MH20/(RR*(temp+CTOK));
  return(absolute_humidity);
}

//
// Berechnung der relativen Luftfeuchtigkeit aus der absoluten
//
float relative_humidity(float absolute_humidity, float temp)
{
  double relative_humidity, ewater;
  ewater=WA0+temp*(WA1+temp*(WA2+temp*(WA3+temp*(WA4+temp*(WA5+temp*WA6)))));
  relative_humidity = absolute_humidity*RR*(temp+CTOK)/(ewater*MH20);
  return(relative_humidity);
}

//
// Berechnung des Taupunkts
//
float dewpoint(float relative_humidity, float temp)
{
  float dewpoint;
  dewpoint=(CON1*log(relative_humidity/100)+CON3*temp/(CON1+temp))/
  	(CON2-log(relative_humidity/100)-CON2*temp/(CON1+temp));
  return(dewpoint);
}

//
// LIM = Relative Luftfeuchtigkeit unter der kein Schimmelwachstum möglich ist
//
float lim(float temp)
{
  float lim;
  lim=(LIM_CON2*exp(LIM_CON1*temp)+LIM_CON3)*100;
  return(lim);
}


//
// Ermittlung des humidity status auf Basis der drei relativen Luftfeuchtigkeiten
//
int humiditystate(float rh_d, float rh_s, float rh_i)
{
	int humidity_state=IN_DRY_OUT_HUMID;
	float tmp;

	if (rh_d < rh_s)   {
		tmp = rh_s;
		rh_s = rh_d;
		rh_d = tmp;
		humidity_state = IN_MORE_DRY_OUT_DRY;
	 }

	if (rh_d < rh_i)  {
                tmp = rh_i;
                rh_i = rh_d;
                rh_d = tmp;
		switch (humidity_state)  {
			case 0: humidity_state = IN_HUMID_OUT_DRY; break;
			case 2: humidity_state = IN_MORE_HUMID_OUT_HUMID; break;
			}
         }

	if (rh_s < rh_i)  {
                tmp = rh_i;
                rh_i = rh_s;
                rh_s = tmp;
		switch (humidity_state)  {
                	case 0: humidity_state = IN_HUMID_OUT_MORE_HUMID; break;
                	case 2: humidity_state = IN_DRY_OUT_MORE_DRY; break;
                    	case 4: humidity_state = IN_MORE_HUMID_OUT_HUMID; break;
                    	case 5: humidity_state = IN_HUMID_OUT_DRY; break;
		 		}
         }

	if (rh_d == rh_s) {
		switch (humidity_state) {
                    	case 0: humidity_state = IN_DRY_OUT_TARGET; break;
                	case 1: humidity_state = IN_HUMID_OUT_HUMID; break;
                    	case 2: humidity_state = IN_DRY_OUT_TARGET; break;
                    	case 3: humidity_state = IN_TARGET_OUT_DRY; break;
                    	case 4: humidity_state = IN_TARGET_OUT_DRY; break;
                    	case 5: humidity_state = IN_HUMID_OUT_HUMID; break;
                 }
         }

        if (rh_s == rh_i) {
                switch (humidity_state) {
                    	case 0: humidity_state = IN_TARGET_OUT_HUMID; break;
                    	case 1: humidity_state = IN_TARGET_OUT_HUMID; break;
                    	case 2: humidity_state = IN_DRY_OUT_DRY; break;
                    	case 3: humidity_state = IN_DRY_OUT_DRY; break;
                    	case 4: humidity_state = IN_HUMID_OUT_TARGET; break;
                    	case 5: humidity_state = IN_HUMID_OUT_TARGET; break;
                    	case 6: humidity_state = IN_TARGET_OUT_TARGET; break;
                    	case 8: humidity_state = IN_TARGET_OUT_TARGET; break;
			case 10: humidity_state = IN_TARGET_OUT_TARGET; break;
                 }
         }
	return (humidity_state);
}

//
// Write Room environment data to influxdb
//
int write_room_data(char* host, char* port, char* db,
					char* room, char* sensor,
					float temp, float rhum, float ahum, float dtemp, float lim)
{
	CURL *curl;
  	CURLcode res;
	char postdata[200];

	// get a curl handle
    curl = curl_easy_init();
    if(curl) {
    	// First set the URL that is about to receive our POST. This URL can
    	// just as well be a https:// URL if that is what should receive the
    	// data.
		sprintf(postdata, "http://%s:%s/write?db=%s", host, port, db);
    	curl_easy_setopt(curl, CURLOPT_URL, postdata);
        // Now specify the POST data
        sprintf(postdata,"envmeas,room=%s,sensor=%s temp=%2.2f,rhum=%2.1f,ahum=%2.2f,dtemp=%2.2f,lim=%2.1f\n",
                        room, sensor, temp, rhum, ahum, dtemp, lim);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // Check for errors
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    	// always cleanup
    	curl_easy_cleanup(curl);
	}
  curl_global_cleanup();
  return 0;
}

//
// Write Window state  to influx db
//
int write_window_state(char* host, char* port, char* db, char* room, int status,
				int humstate, float crhum_ak, float lim, float min_rhum)
{
	CURL *curl;
  	CURLcode res;
	char postdata[400];

	// get a curl handle
    curl = curl_easy_init();
    if(curl) {
    	// First set the URL that is about to receive our POST. This URL can
    	// just as well be a https:// URL if that is what should receive the
    	// data.

	sprintf(postdata, "http://%s:%s/write?db=%s", host, port, db);
    	curl_easy_setopt(curl, CURLOPT_URL, postdata);
        // Now specify the POST data
        sprintf(postdata,"winmeas,room=%s status=%i,humstate=%i,crhumak=%2.2f,lim=%2.1f,minrhum=%2.2f\n",
				room, status, humstate, crhum_ak, lim, min_rhum);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // Check for errors
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    	// always cleanup
    	curl_easy_cleanup(curl);
   	}
  curl_global_cleanup();
  return 0;
}

//
// Get Window state from influx db
//

size_t writeFunction(char *ptr, size_t size, size_t nmemb, char *data) {
    strncat(data, ptr, size * nmemb);
    return size * nmemb;
}

int get_window_state(char* host, char* port, char* db, char* room, int* status)
{
        CURL *curl;
        CURLcode res;
        char postdata[200];
	char response_string[1000];
	char tmp[2];
	char *commaptr;

        // get a curl handle
    curl = curl_easy_init();
    if(curl) {
        // First set the URL that is about to receive our POST. This URL can
        // just as well be a https:// URL if that is what should receive the
        // data.
        sprintf(postdata, "http://%s:%s/query?db=%s", host, port, db);
        curl_easy_setopt(curl, CURLOPT_URL, postdata);

        // Now specify the POST data
	sprintf(postdata,"q=SELECT last(\"status\") FROM \"winmeas\" WHERE \"room\"='%s'", room);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

	//  Now set the values for responses
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_string);


        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // Check for errors
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
        // always cleanup
        curl_easy_cleanup(curl);
	commaptr = strrchr(response_string,',');
	if (commaptr != NULL) {
		strncpy(tmp,commaptr+1,1);
		*status = atoi(tmp);
		}
	else *status=-1;
  	}
  curl_global_cleanup();
  return 0;
}


# influx-window
Kellerfenstersteuerungsprojekt übernommen von dock2 

Kompilieren erfolgt mit während des docker Container builds 
gcc -pthread -o /src/open_window /src/open_window.c \
    /src/bricklet_humidity.c /src/bricklet_temperature.c /src/bricklet_dual_relay.c \
    /src/ip_connection.c /src/cellar_functions.c  -lm -lcurl

Container wird gestartet mit 

docker run -d --link fs_influxdb:influxdb --restart unless-stopped --name fs_influx_window steininger/influx_window

         
     


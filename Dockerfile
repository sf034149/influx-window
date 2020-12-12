FROM alpine

# add packages 
RUN apk update \
    && apk upgrade \
    && apk add --update gcc curl-dev musl-dev tzdata curl \
    && cp /usr/share/zoneinfo/Europe/Berlin /etc/localtime \
    && echo "Europe/Berlin" >  /etc/timezone \
    && apk del tzdata \
    && rm -rf /var/cache/apk/* 

# compile application
RUN mkdir /src  
ADD src /src
RUN  gcc -pthread -o /src/influx_window /src/influx_window.c \ 
	/src/bricklet_humidity.c /src/bricklet_temperature.c /src/bricklet_dual_relay.c \
	 /src/ip_connection.c /src/cellar_functions.c  -lm -lcurl

RUN  gcc -pthread -o /src/close_window /src/close_window.c \
        /src/bricklet_humidity.c /src/bricklet_temperature.c /src/bricklet_dual_relay.c \
         /src/ip_connection.c /src/cellar_functions.c  -lm -lcurl

RUN  gcc -pthread -o /src/open_window /src/open_window.c \
        /src/bricklet_humidity.c /src/bricklet_temperature.c /src/bricklet_dual_relay.c \
         /src/ip_connection.c /src/cellar_functions.c  -lm -lcurl



# extend crontab to have 5 minutes periodic directory 
ADD crontab_root /tmp 
RUN mkdir /etc/periodic/5min \
    && chown root:root /etc/periodic/5min \
    && crontab /tmp/crontab_root \
    && rm /tmp/crontab_root

# copy application to periodic directory  
RUN cp /src/influx_window /etc/periodic/5min/ \ 
    && chmod -v +x /etc/periodic/5min/influx_window \
    && chown root:root /etc/periodic/5min/influx_window

CMD ["crond","-f"]

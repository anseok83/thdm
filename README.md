# THDM (THD Monitoring) for air quality monitoring

THDM is for air quality monitoring with various measures (currently, temperature, humidity and dust density are supported) through the internet.

## Features and screen shot

### Version 0.0.2 (May 2016)
* Getting sensor data (temperature, humidity and dust density) for air quality measurement
* Periodic reporting sensor data via Socket.io to server (THD Server)
* Storing reported sensor data to database (mongodb)
* Visualizing instantaneous and past sensor data via internet (monitoring web page)

### Version 0.0.5 (July 2016)
* Standalone operation (no need Raspberry pi or any kind of servers)
* Status lookup via 3 color LED
* Wifi network configurations

![An example of THD web page](/images/thd_webpage.png)

## Arduino board (THD Sensor)

![THD sensor](/images/thd_sensor.JPG)

A list of hardware components for THDM Sensors:

* WeMos D1 WiFi uno based ESP8266 shield for arduino compatible

  The information of the board can be found in http://www.wemos.cc/Products/d1.html.  
  I bought the part in this [link](http://www.aliexpress.com/snapshot/7611008261.html?orderId=74819981106270).  

* Temperature and humidity sensor module (KY-015)

  This module seems nothing but the temperature and humidity sensor (DHT-11) and a resistor. You may need a 5K resistor if you use the sensor directly.  
  The information of the sensor is in http://www.micropik.com/PDF/dht11.pdf.

* Sharp dust sensor (GP2Y1010AU0F)

  The datasheet of the sensor is in https://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf.  
  You may need a 150-ohm resistor and 220uF capacitor to use the sensor, or you can use a dust sensor adapter (http://www.dfrobot.com/index.php?route=product/product&product_id=1063#.Vz7O7pOLSL8).
  
* 3 color LED (KY-009)

  For more information, you can refer https://tkkrlab.nl/wiki/Arduino_KY-009_3-color_full-color_LED_SMD_modules.
  (In my case, the pins for red and green was changed from the printing.)

Connections of hardware components:

* WeMos D1

  A0 - Input from dust sensor part (PIN5 of GP2Y1010AU0F)  
  D3 - Output to dust sensor part (PIN3 of GP2Y1010AU0F)  
  D4 - Output/input with temperature and humidity sensor module (PIN S of KY-015)
  D5 - Output to 3 color LED (RED pin)
  D6 - Output to 3 color LED (GREEN pin)
  D7 - Output to 3 color LED (BLUE pin)
  3.3V - Vcc to all the components (dust sensor part and temperature and humidity sensor module)  
  GND - Ground to all the components

And I've utilized DHT sensor library, ESP8266 libraries and Socket.io library (https://github.com/washo4evr/Socket.io-v1.x-Library) for DHT Sensor board programming. Please find further details in each libraries' web pages.

Software components (including libraries):

* Socket-IO

  Used socket.io for initial information transfer (this is an optional procedure).
  
* WiFiManager

  Used WiFiManager library for flexible Wifi configuration without hard coding.
  
* Google chart (https://developers.google.com/chart/)

  Google chart APIs to plot instantaneous and past sensor data with line and gauge charts.


## History

* Uploaded a snapshot (v0.0.2) of THD Sensor and Server codes in the [examples](https://github.com/anseok83/thdm/tree/master/examples) directory. (2016-05-21)
* Uploaded a snapshot (v0.0.5) of THD Sensor in the [examples](https://github.com/anseok83/thdm/tree/master/examples) directory. (2016-07-15)


<br><br>


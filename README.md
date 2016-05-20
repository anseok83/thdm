# THDM (THD Monitoring)

THDM is for air condition monitoring system with various measures (currently, temperature, humidity and dust density are supported) through the internet. THDM composed of sensor parts (Arduino with WiFi and various sensors) and server parts (Node.js, mongodb and visualization components).

## Sensor part (Arduino)

A list of hardware components for THDM Sensors:

* WeMos D1 WiFi uno based ESP8266 shield for arduino Compatible

  The information of the board is in this link (http://www.wemos.cc/Products/d1.html).  
  I bought this part with this link (http://www.aliexpress.com/snapshot/7611008261.html?orderId=74819981106270).  

* Temperature and humidity sensor module (KY-015)

  This module seems nothing but the temperature and humidity sensor (DHT-11) and a resistor. You may need a 5K resistor if you use the sensor directly.  
  The information of the sensor is in this link (http://www.micropik.com/PDF/dht11.pdf).

* Sharp dust sensor (GP2Y1010AU0F)

  The datasheet of the sensor is in this link (https://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf).  
  You may need a 150-ohm resistor and 220uF capacitor to use the sensor, or you can use a dust sensor adapter (http://www.dfrobot.com/index.php?route=product/product&product_id=1063#.Vz7O7pOLSL8).

Connections of hardware components:

* WeMos D1

  A0 - Input from dust sensor part (PIN5 of GP2Y1010AU0F)  
  D3 - Output to dust sensor part (PIN3 of GP2Y1010AU0F)  
  D0 - Output/input with temperature and humidity sensor module (PIN S of KY-015)  
  3.3V - Vcc to all the components (dust sensor part and temperature and humidity sensor module)  
  GND - Ground to all the components

And I've utilized DHT sensor library, ESP8266 libraries and Socket.io library (https://github.com/washo4evr/Socket.io-v1.x-Library) for DHT Sensor board programming. Please find further details in each libraries' web pages.


## Server part (Raspberry Pi 2 Model B)

I use Raspberry Pi 2 Model B (https://www.raspberrypi.org/products/raspberry-pi-2-model-b) with Raspbian (https://www.raspberrypi.org/downloads/raspbian) for server side operations.

Software components for server side programmings:

* Node.js (v4.4.4)

  Node.js for THD Server with express, jquery, mongodb and socket.io.
  
* Mongodb (https://mlab.com)

  Could use local mongodb, but I've used mongodb storage provided by mlab.com (you can have free 500MB storage) for decreasing loads of the server.
  
* Google chart (https://developers.google.com/chart/)

  Used Google chart APIs to plot instantaneous and past sensor data with line and gauge charts.

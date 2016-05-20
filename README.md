# THDM (THD Monitoring)

THDM is a system to monitor air condition with various measures (temperature, humidity and dust density) with Internet and smart phone. The system composed of sensor parts (WiFi enabled Arduino-compatible board and various sensor modules) and server parts (Node.js, mongodb and visualization components).

## Sensor part

A list of components that THDM uses is below. 

* WeMos D1 WiFi uno based ESP8266 shield for arduino Compatible

  The information of the board is in this link (http://www.wemos.cc/Products/d1.html).

  I bought this part with this link (http://www.aliexpress.com/snapshot/7611008261.html?orderId=74819981106270).

* Temperature and humidity sensor module (KY-015)

  This module seems nothing but the temperature and humidity sensor (DHT-11) and a resistor. You may need a 5K resistor if you use the sensor directly.

  The information of the sensor is in this link (http://www.micropik.com/PDF/dht11.pdf).

* Sharp dust sensor (GP2Y1010AU0F)

  The datasheet of the sensor is in this link (https://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf).

  You may need a 150-ohm resistor and 220uF capacitor to use the sensor, or you can use a dust sensor adapter (http://www.dfrobot.com/index.php?route=product/product&product_id=1063#.Vz7O7pOLSL8).

Connections of components are below.

* WeMos D1

  A0 - Input from dust sensor part (PIN5 of GP2Y1010AU0F)
  
  D3 - Output to dust sensor part (PIN3 of GP2Y1010AU0F)
  
  D0 - Output/input with temperature and humidity sensor module (PIN S of KY-015)
  
  3.3V - Vcc to all the components (dust sensor part and temperature and humidity sensor module)
  
  GND - Ground to all the components

# Server part


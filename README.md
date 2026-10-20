![Project Screenshot](esp32c324ringled.png)

# ESP32-C3 24 WS2812B RGB Ring LED WiFi STA + AP control with CURL and HTTP POST control
This code turns an ESP32-C3 into a Wi-Fi-controlled LED strobe controller for a 24-LED NeoPixel strip, allowing users to control the strobe on/off state, brightness, RGB colour, number of LEDs per group, and strobe interval through a mobile-friendly web interface. The ESP32 creates its own password-protected Wi-Fi access point with a unique device ID and can also connect to an existing Wi-Fi router using credentials configured through the web page. It provides HTTP API endpoints to read the device status, retrieve individual settings, update LED parameters, and configure Wi-Fi credentials. All LED and Wi-Fi settings are saved in non-volatile memory so they persist across reboots, and the LED strobe timing uses millis() rather than blocking delays, allowing the web server to remain responsive while the LEDs alternate between on and off phases.


Curl commands as per the cheat sheet

# ESP32-C3 24 WS2812B RGB Ring LED WiFi AP control
This code programs an ESP32 microcontroller to control a strip of 24 addressable LEDs using a web-based interface. The ESP32 creates its own Wi-Fi Access Point using its unique chip ID, allowing users to connect with a phone or computer and open a webpage to adjust the LED brightness, RGB colour, number of LEDs illuminated in each group, and strobe interval in milliseconds. The LEDs flash in alternating groups to create a strobe effect, which can be switched on or off, while setting the interval to zero keeps all LEDs continuously lit. The code also saves the selected settings in the ESP32’s non-volatile memory so they are retained after a restart, and uses a non-blocking timing method to control the flashing without interrupting the web server.

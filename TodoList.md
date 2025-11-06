# Features to do

## Add BLE console support

- The response from BLE command will output to the cdc console like the hid command input
- teh response back to the BLE depends on the commands

## WIFI Web Server 

### Connection

- Mode
  - Wifi AP Mode for client connection and control
  - Station mode to connect to WiFi spot or Wifi AP, user can access the web server from the WAN

### Functions

- PWM output control
- Taco Meter input to measure motor RPM. 
- Flash LED output for the motor RPM measurement

## Settings

- User can save setting in novolatile memory
  - PWM duty and frequency
  - motor poles to convert Taco Meter input frequency to RPM.
  - Wifi ssid and password
- The saved settings will auto reload when power on
/*****************************************************************************
  A simple example of using the ArduinoBLE library (v1.3.6)
  to interact with an Arduino from another Bluetooth Low Energy
  device. You'll need to install the library via the Library Manager
  to build this sketch.
   
  This sample blinks the built-in LED every second by default.
  Use a Bluetooth LE app to adjust the blink speed remotely
  by connecting to the Arduino and writing a new value to the
  characteristic with LED_BLINK_DELAY_UUID.

  Tested Bluetooth LE apps:
  Windows GUI: Bluetooth LE Explorer (free on Windows store, but 
               warning - its fragile and crashes on pretty much
               any sort of error)

  Windows CLI: BLEConsole
               https://sensboston.github.io/BLEConsole/

  iOS:         LightBlue (free on App Store)

  I only tested this on an newish Arduino Nano ESP32, which as far as
  I know is the only model that supports BLE as of Sept 2023 (even
  the older ESP32s don't support it).

  Written by Jason Seba 
  jason-dot-seba-at-hotmail-dot-com
  https://github.com/JasonSeba

  This code is in the public domain. Use it however you want, but there
  are no warranties expressed or implied etc etc

  Last updated 9/3/2023
*****************************************************************************/

#include <ArduinoBLE.h>
// See https://www.arduino.cc/reference/en/libraries/arduinoble/

// The following variable can be modified via BLE
int  ledBlinkDelay = 1000; // milliseconds
int  ledBlinkTime  = 0;
bool ledStateHigh  = true;

// Optional: connect an LED to pin 9 and it'll blink too
int  auxLedPin     = 9;

// Bluetooth stuff

// Each BLE service & characteristic should have a unique UUID.
// Generate v4 UUIDs here: https://www.uuidgenerator.net/version4
#define SERVICE_UUID         "d3941223-3b39-4b74-b0eb-c2ef342eaeae"
#define LED_BLINK_DELAY_UUID "cd330c5c-6fb8-4c9c-8a9d-092c74748124"

// This is kind of arbitrary...
#define BLE_MAX_LENGTH  20

// Each BLE device needs a service
BLEService ledService(SERVICE_UUID);

// Each service needs a descriptor to give it name and a UUID
// The Bluetooth standard defines a bunch of "Assigned Numbers"
// See section 3.6 of this: https://www.bluetooth.com/specifications/assigned-numbers/
// 1800 is for "Generic Access service", which seems to work, although I have no idea
// if that's really the correct choice here or whether it even really matters.
BLEDescriptor ledNameDescriptor("1800", "LED Service");

// A characteristic is what you actually use to control/configure something,
// and they need a name descriptor as well.
// UUID 1801 is for "Generic Attribute service" which, again, seems to work 
// although I have no idea if it is the best choice.
// Characteristics have properties such as BLERead that determine how they
// can be access. Although we define the Indicate and Notify properties 
// here, they are not currently implemented in this sketch. Maybe some 
// day I'll get around to it - watch this space.
BLECharacteristic ledBlinkDelayCharacteristic(LED_BLINK_DELAY_UUID, BLEIndicate | BLERead | BLEWrite | BLENotify, BLE_MAX_LENGTH);
BLEDescriptor ledBlinkDelayNameDescriptor("1801", "LED Blink Delay (ms)");


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(auxLedPin, OUTPUT);

  Serial.begin(9600);

  // Must do this before any other calls to BLE!

  if (!BLE.begin()) {
    Serial.println("Error while trying to initialise ArduinoBLE");
    while (1);
  }

  Serial.print("Bluetooth address: ");
  Serial.println(BLE.address());

  // Not sure what the best values are here or what this really does...
  BLE.setConnectionInterval(0x06, 0x12);

  // Set advertised local name (this is what you'll see 
  // in the BLE application - defaults to Arduino, I think)
  BLE.setLocalName("Arduino BLE Local Name");

  // Add our service
  BLE.setAdvertisedService(ledService);

  // Add descriptor for our characteristic

  ledBlinkDelayCharacteristic.addDescriptor(ledBlinkDelayNameDescriptor);
  
  // Add the characteristic to the service
  ledService.addCharacteristic(ledBlinkDelayCharacteristic);

  // Add the service
  BLE.addService(ledService);
  
  // Assign event handlers for connecting and disconnecting
  // We don't really need these, just use them for debug messages
  BLE.setEventHandler(BLEConnected, connectHandler);
  BLE.setEventHandler(BLEDisconnected, disconnectHandler);
  
  // Assign event handlers for characteristics
  char temp[32];
  ledBlinkDelayCharacteristic.setEventHandler(BLEWritten, writtenHandler);
  ledBlinkDelayCharacteristic.setEventHandler(BLERead, readHandler);

  // Write the current ledBlinkDelay to the characteristic
  ledBlinkDelayCharacteristic.writeValue(ledBlinkDelay);
  
  // Set the device as connectable.
  BLE.setConnectable(true);

  // Start advertising.
  BLE.advertise();

  // Set the initial state of the LED to HIGH (on)
  // and kick of the timer to blink it
  ledBlinkTime = millis() + ledBlinkDelay;
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(auxLedPin, HIGH);
  ledStateHigh = true;
}

void loop() {
  // The following is super important and doesn't seem to be documented
  // anywhere except here: https://www.arduino.cc/reference/en/libraries/arduinoble/ble.poll/
  // You *must* call BLE.poll() periodically to process Bluetooth LE events.

  BLE.poll();

  // If the ledBlinkTime has expired, change the LED state

  if (ledBlinkTime < millis()) {
      // Switch LED from on to off or vice versa
      digitalWrite(LED_BUILTIN, ledStateHigh ? LOW : HIGH);
      digitalWrite(auxLedPin, ledStateHigh ? LOW : HIGH);

      // Next time do the opposite
      ledStateHigh = !ledStateHigh;

      // Restart the timer
      ledBlinkTime = millis() + ledBlinkDelay;
  }

}

// Bluetooth handlers

// The "central" is the Bluetooth device that is connected to us.
// Its also sometimes call the "client"

// The connect and disconnect handlers don't do anything right now
void connectHandler(BLEDevice central) {
  // Print the central's MAC address:
  Serial.print("Connected to central: ");
  Serial.println(central.address());
}

void disconnectHandler(BLEDevice central) {
  // Print the central's MAC address:
  Serial.print("Disconnected from central: ");
  Serial.println(central.address());
}

void writtenHandler(BLEDevice central, BLECharacteristic characteristic) {
  int newValue = 0;

  // The new value has already been written to the characteristic,
  // we just need to retrieve it and do something with it.
  characteristic.readValue(newValue);
  Serial.printf("mwrittenHandler: %d\n", newValue);

  // Make sure its nothing crazy...
  if (newValue < 100 || newValue > 10000) {
    // Ignore, overwrite the new value, and disconnect the central
    // for being a jerk. TODO there may be a way to return an error
    // code or something here instead.
    characteristic.writeValue(ledBlinkDelay);
    central.disconnect();  // need to call this from this method!!
  }
  else {
    // Legit, save it
    ledBlinkDelay = newValue;
  }
}

void readHandler(BLEDevice central, BLECharacteristic characteristic) {
  // This is kind of useless, because in this example the value in
  // the characteristic and the actual value of ledBlinkDelay should
  // already always be synchronized. But in a more complex application,
  // something else might modify ledBlinkDelay and this is how we would
  // communicate that back to the central.
  if (characteristic.writeValue(ledBlinkDelay))
    Serial.println("readHandler success");
  else {
    Serial.println("readHandler failure");
    central.disconnect();  // need to call this from this method!!
    //TODO return error instead?
  }
}


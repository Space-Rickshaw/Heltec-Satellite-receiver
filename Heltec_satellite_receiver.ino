#include <RadioLib.h>
#include <Wire.h>
#include "heltec.h"


#if (ESP8266 || ESP32)
#define ISR_PREFIX ICACHE_RAM_ATTR
#else
#define ISR_PREFIX
#endif

#define SATELLITE_HEALTH_PACKET_ID 1
#define SATELLITE_HEALTH_PACKET_SIZE 41

#define UPLINK_COMMAND_RESPONSE_PACKET_ID 3
#define UPLINK_COMMAND_RESPONSE_PACKET_SIZE 24

#define POWER_DATA_PACKET_ID 10
#define POWER_DATA_PACKET_SIZE 36

#define TEMPERATURE_DATA_PACKET_ID 12
#define TEMPERATURE_DATA_PACKET_SIZE 25

#define RADIATION_DATA_PACKET_ID 13
#define RADIATION_DATA_PACKET_SIZE 17

#define SYSTEM_CONFIG_PACKET_ID 14
#define SYSTEM_CONFIG_PACKET_SIZE 45

#define IMU_PACKET_ID 7
#define IMU_PACKET_SIZE 47


#define STORE_AND_FORWARD_MESSAGE_PACKET_ID 2

float radioFrequency = 437.4;
int size = 100;
String str = "";
int size_of_packet = 100;


/* SX1278 has the following connections:
//**********Make sure to connect DIO0 and DIO1 to interrupt pins**************
//For arduino uno, mega use pin 2,3.
//For STM32, ESP32, RP2040 any pin can be used.


// NSS pin:   18
// DIO0 pin:  26
// RESET pin: 14
// DIO1 pin:  35
// SX1278 radio = new Module(53, 2, 48, 3); //ARDUINO MEGA

The following exapmle is for SX1268. 
// NSS pin:   17
// DIO1 pin:  21
// NRST pin:  20
// BUSY pin:  26
SX1268 radio = new Module(17, 21, 20, 26); //Raspberry Pi Pico

*/

// NSS pin:   18
// DIO0 pin:  26
// RESET pin: 14
// DIO1 pin:  35
SX1278 radio = new Module(18, 26, 14, 35);  //heltec LoRa 32


volatile bool receivedFlag = false;

volatile bool enableInterrupt = true;

//Function to enable flag when data received
void setFlag(void) {
  if (!enableInterrupt) {
    return;
  }
  receivedFlag = true;
}


//Function to generate checksum of the command
String XorChecksum(String s) {
  byte b = s.charAt(0);
  for (int i = 1; i < s.length(); i++) {
    b = b ^ s.charAt(i);
  }
  String checksum = String(b, HEX);
  if (checksum.length() == 1) checksum = "0" + checksum;
  return checksum;
}


void setup() {
  Serial.begin(115200);

  //Comment the below if display is not used.
  Heltec.begin(true, false, false);
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();
  Heltec.display->display();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(0, 20, "Booting Up...");
  Heltec.display->display();
  delay(1000);
  // ----------------------------------

  int state = radio.begin(radioFrequency);
  radio.setOutputPower(20);
  radio.setCurrentLimit(140);

  if (state == RADIOLIB_ERR_NONE) {
    //Comment the below if display is not used.
    Heltec.display->clear();
    Heltec.display->setFont(ArialMT_Plain_10);  // Normal 1:1 pixel scale
    Heltec.display->drawString(0, 10, "Radio Booted Up");
    Heltec.display->drawString(0, 20, "");
    Heltec.display->drawString(0, 30, "Radio on " + String(radioFrequency) + "MHz");
    Heltec.display->display();
    delay(3000);
    //-------------------------------------------
  } else {
    //Comment the below if display is not used.

    Heltec.display->clear();

    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 20, "Radio Bootting Failed");
    Heltec.display->drawString(0, 30, "Error Code:- " + String(state));
    Heltec.display->display();
    while (true)
      ;

    //----------------------------------------------------------------
  }

  radio.setDio0Action(setFlag, RISING);

  //Comment the below if display is not used.
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_10);  // Normal 1:1 pixel scale
  Heltec.display->drawString(0, 20, "Waiting For Signal");
  Heltec.display->display();
  //----------------------------------------------------------

  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    //Serial.println(F("success!"));
  } else {
    //Serial.print(F("failed, code "));
    //Serial.println(state);

    //Comment the below if display is not used.
    Heltec.display->clear();

    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 20, "Receive Mode Failed");
    Heltec.display->drawString(0, 30, "Error Code:- " + String(state));
    Heltec.display->display();
    //------------------------------------------------------------------
    while (true)
      ;
  }
}




void loop() {
  while (Serial.available() > 0) {
    String dataFromSerial = Serial.readStringUntil('\n');
    if (dataFromSerial[0] == '@') {
      radioFrequency = dataFromSerial.substring(1).toFloat();
      int frequencyChangeStatus = radio.setFrequency(radioFrequency);
      if (frequencyChangeStatus == RADIOLIB_ERR_NONE) {
        //Comment the below if display is not used.
        Heltec.display->clear();
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->drawString(0, 10, "Frequency Changed");
        Heltec.display->drawString(0, 20, "");
        Heltec.display->drawString(0, 30, "Radio on " + String(radioFrequency) + "MHz");
        Heltec.display->display();
        //------------------------------------------------------------
      } else {
        //Comment the below if display is not used.
        Heltec.display->clear();
        Heltec.display->setFont(ArialMT_Plain_10);  // Normal 1:1 pixel scale
        Heltec.display->drawString(0, 20, "Frequency failed to update");
        Heltec.display->drawString(0, 30, "Error Code:- " + String(frequencyChangeStatus));
        Heltec.display->display();
        //----------------------------------------------------------
      }

    } else {
      dataFromSerial = dataFromSerial + "|" + XorChecksum(dataFromSerial);
      int st = radio.transmit(dataFromSerial);

      if (st == RADIOLIB_ERR_NONE) {
        //Comment the below if display is not used.
        Heltec.display->clear();
        Heltec.display->setFont(ArialMT_Plain_16);
        Heltec.display->drawString(0, 3, "Transmit  ");
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->drawString(0, 19, dataFromSerial);
        Heltec.display->display();
        //---------------------------------------------------
      } else {
        //Comment the below if display is not used.
        Heltec.display->clear();
        Heltec.display->setFont(ArialMT_Plain_16);
        Heltec.display->drawString(0, 3, "Transmit  ");
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->drawString(0, 19, "Failed");
        Heltec.display->display();
        //----------------------------------------------------
      }
    }
  }

  if (receivedFlag) {
    enableInterrupt = false;
    receivedFlag = false;
    byte byteArr[100];
    str = "";
    // int state = radio.readData(str);
    int state = radio.readData(byteArr, size);

    if (state == RADIOLIB_ERR_NONE) {
      size_of_packet = 100;
      byte packet_type = byteArr[8];

      if (packet_type == SATELLITE_HEALTH_PACKET_ID) {
        size_of_packet = SATELLITE_HEALTH_PACKET_SIZE;
      } else if (packet_type == UPLINK_COMMAND_RESPONSE_PACKET_ID) {
        size_of_packet = UPLINK_COMMAND_RESPONSE_PACKET_SIZE;
      } else if (packet_type == POWER_DATA_PACKET_ID) {
        size_of_packet = POWER_DATA_PACKET_SIZE;
      } else if (packet_type == TEMPERATURE_DATA_PACKET_ID) {
        size_of_packet = TEMPERATURE_DATA_PACKET_SIZE;
      } else if (packet_type == RADIATION_DATA_PACKET_ID) {
        size_of_packet = RADIATION_DATA_PACKET_SIZE;
      }else if (packet_type == SYSTEM_CONFIG_PACKET_ID) {
        size_of_packet = SYSTEM_CONFIG_PACKET_SIZE;
      }else if(packet_type == STORE_AND_FORWARD_MESSAGE_PACKET_ID){
        size_of_packet = (byteArr[11]) + 12;
      }
      for (int i = 0; i < size_of_packet; i++) {
        Serial.print(byteArr[i]);
         
        // if (byteArr[i] < 16) {
        //   str += "0" + (String(byteArr[i], HEX));
        // } else {
        //   str += (String(byteArr[i], HEX));
        // }
        if (i < size_of_packet) {
          Serial.print(" ");
          // str += " ";
        }
      }

      Serial.print('\n');
      // Serial.write('\n');
      // Heltec.display->clear();
      // // Heltec.display->setFont(ArialMT_Plain_10);  // Normal 1:1 pixel scale
      // Heltec.display->drawString(0, 0, "Received " + String(size_of_packet) + " bytes");
      // int lines_to_display = ceil(((float)str.length() / 24));
      // for (int i = 0; i < lines_to_display; i++) {
      //   unsigned int end_index = (i*24)+24;
      //   if(str.length()< end_index){
      //     end_index = str.length();
      //   }
      //   Heltec.display->drawString(0, (10*(i+1)), str.substring(i*24, end_index));
      // }
      // // Heltec.display->drawStringMaxWidth(0, 10, 120,  str);


      // // // Heltec.display->drawString(0, 10, str);
      // Heltec.display->display();
    }
    radio.startReceive();
    enableInterrupt = true;
  }
}

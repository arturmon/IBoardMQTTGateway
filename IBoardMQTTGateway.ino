/* 				MyMQTT Broker Gateway 0.1b

Created by Daniel Wiegert <daniel.wiegert@gmail.com>
Based on MySensors Ethernet Gateway by Henrik Ekblad <henrik.ekblad@gmail.com>
http://www.mysensors.org

Requires MySensors lib 1.4b

* Change below; TCP_IP, TCP_PORT, TCP_MAC
This will listen on your selected TCP_IP:TCP_PORT below, Please change TCP_MAC your liking also.
*1 -> NOTE: Keep first byte at x2, x6, xA or xE (replace x with any hex value) for using Local Ranges.

*2 You can use standard pin set-up as MySensors recommends or if you own a IBOARD you may change
	the radio-pins below if you hardware mod your iBoard. see [URL BELOW] for more details.
	http://forum.mysensors.org/topic/224/iboard-cheap-single-board-ethernet-arduino-with-radio/5

* Don't forget to look at the definitions in libraries\MySensors\MyMQTT.h!

	define TCPDUMP and connect serial interface if you have problems, please write on
	http://forum.mysensors.org/ and explain your problem, include serial output. Don't forget to
	turn on DEBUG in libraries\MySensors\MyConfig.h also.

	MQTT_FIRST_SENSORID is for 'DHCP' server in MyMQTT. You may limit the ID's with FIRST and LAST definition.
	If you want your manually configured below 20 set MQTT_FIRST_SENSORID to 20.
	To disable: set MQTT_FIRST_SENSORID to 255.

	MQTT_BROKER_PREFIX is the leading prefix for your nodes. This can be only one char if like.

	MQTT_SEND_SUBSCRIPTION is if you want the MyMQTT to send a empty payload message to your nodes.
	This can be useful if you want to send latest state back to the MQTT client. Just check if incoming
	message has any length or not.
	Example: if (msg.type==V_LIGHT && strlen(msg.getString())>0) otherwise the code might do strange things.

* Address-layout is : [MQTT_BROKER_PREFIX]/[NodeID]/[SensorID]/V_[SensorType]
	NodeID and SensorID is uint8 (0-255) number.
	Last segment is translation of the sensor type, look inside MyMQTT.cpp for the definitions.
	User can change this to their needs. We have also left some space for custom types.

Special: (sensor 255 reserved for special commands)
You can receive a node sketch name with MyMQTT/20/255/V_Sketch_name (or version with _version)

To-do:
Special commands : clear or set EEPROM Values, Send REBOOT and Receive reboot for MyMQTT itself.
Be able to send ACK so client returns the data being sent.
... Please come with ideas!
What to do with publish messages.

Test in more MQTT clients, So far tested in openhab and MyMQTT for Android (Not my creation)
- http://www.openhab.org/
- https://play.google.com/store/apps/details?id=at.tripwire.mqtt.client&hl=en
... Please notify me if you use this broker with other software.


* How to set-up Openhab and MQTTGateway:
http://forum.mysensors.org/topic/303/mqtt-broker-gateway

*/

#include <DigitalIO.h>
#include <SPI.h>
#include <MySensor.h>
#include <MyMQTT.h>
#include <Ethernet.h>
/*
//MISO(D5), MOSI(D5) and SCK(D7)
#define RADIO_CE_PIN        3  // radio chip enable
#define RADIO_SPI_SS_PIN    8  // radio SPI serial selectThis will also require changing the LED pins to free ones like so:
#define RADIO_ERROR_LED_PIN 7  // Error led pin
#define RADIO_RX_LED_PIN    6  // Receive led pin
#define RADIO_TX_LED_PIN    9  // the PCB, on board LED
*/

//#define USE_DHCP                                              //use only Iboard Pro

#define INCLUSION_MODE_TIME 1                                   // Number of minutes inclusion mode is enabled
#define INCLUSION_MODE_PIN  14                                  //A0 // Digital pin used for inclusion mode button

#define RADIO_CE_PIN        3 // radio chip enable
#define RADIO_SPI_SS_PIN    8  // radio SPI serial select
#define RADIO_ERROR_LED_PIN 15 //A1 // Error led pin
#define RADIO_RX_LED_PIN    16 //A2 // Receive led pin
#define RADIO_TX_LED_PIN    17 //A3 // the PCB, on board LED


#define TCP_PORT 1883						// Set your MQTT Broker Listening port.
IPAddress TCP_IP ( 192, 168, 1, 254 );				// Configure your static ip-address here
#ifdef USE_DHCP
IPAddress TCP_GATEWAY (192, 168, 1, 1);
IPAddress TCP_SUBNET (255, 255, 0, 0);
#endif
byte TCP_MAC[] = { 0x02, 0xDE, 0xAD, 0x00, 0x00, 0x42 };	// Mac-address - You should change this! see note *2 above!

//////////////////////////////////////////////////////////////////

EthernetServer server = EthernetServer(TCP_PORT);
EthernetClient *currentClient = NULL;
MyMQTT gw(RADIO_CE_PIN, RADIO_SPI_SS_PIN);


void processEthernetMessages() {
  char inputString[MQTT_MAX_PACKET_SIZE] = "";
  byte inputSize = 0;
  byte readCnt = 0;
  byte length = 0;

  EthernetClient client = server.available();
  if (client) {
    while (client.available()) {
      // Save the current client we are talking with
      currentClient = &client;
      byte inByte = client.read();
      readCnt++;

      if (inputSize < MQTT_MAX_PACKET_SIZE) {
        inputString[inputSize] = (char)inByte;
        inputSize++;
      }

      if (readCnt == 2) {
        length = (inByte & 127) * 1;
      }
      if (readCnt == (length+2)) {
        break;
      }
    }
#ifdef TCPDUMP
    Serial.print("<<");
    char buf[4];
    for (byte a=0; a<inputSize; a++) { sprintf(buf, "%02X ", (byte)inputString[a]); Serial.print(buf); } Serial.println();
#endif
    gw.processMQTTMessage(inputString, inputSize);
    currentClient = NULL;
  }
}

void writeEthernet(const char *writeBuffer, byte *writeSize) {
#ifdef TCPDUMP
  Serial.print(">>");
  char buf[4];
  for (byte a=0; a<*writeSize; a++) { sprintf(buf,"%02X ",(byte)writeBuffer[a]); Serial.print(buf); } Serial.println();
#endif
  // Check whether to respond to a single client or write to all
  if (currentClient != NULL)
    currentClient->write((const byte *)writeBuffer, *writeSize);
  else
    server.write((const byte *)writeBuffer, *writeSize);
}

int main(void) {
  init();
  #ifdef USE_DHCP 
   if (Ethernet.begin(TCP_MAC) == 0) {
    // initialize the ethernet device not using DHCP:
    Ethernet.begin(TCP_MAC, TCP_IP, TCP_GATEWAY, TCP_SUBNET);
  } 
   #else
  Ethernet.begin(TCP_MAC, TCP_IP);
 #endif  
  delay(1000);   // Wait for Ethernet to get configured.
  gw.begin(RF24_PA_LEVEL_GW, RF24_CHANNEL, RF24_DATARATE, writeEthernet, RADIO_RX_LED_PIN, RADIO_TX_LED_PIN, RADIO_ERROR_LED_PIN);
  server.begin();
  while (1) {
    processEthernetMessages();
    gw.processRadioMessage();
  }
}

#include <SoftwareSerial.h>

#include <EEPROM.h>
#include "BGLib.h"

//Remembring Host Device - Arduino with BLE112
#define button_pin  1
#define button_default_state HIGH
#define red_LED_pin 2
#define red_LED_on_state HIGH
#define green_LED_pin 3
#define green_LED_on_state HIGH
#define speaker_pin 4
#define device_memory_address 1 //declares array of device IDs
#define max_devices 10 //NOT 0 indexed
#define device_scan_time 15
#define debug 1

SoftwareSerial bleSerialPort(2, 3);
BGLib ble112((HardwareSerial *)&bleSerialPort);
//SoftwareSerial bleSerialPort = Serial;

char device_IDs[max_devices][13]; //12-char address + \0

void setup() {  
  //  HappyBeep();
  Serial.begin(38400);
  while(!Serial) {
  } //silly port needs a little time to init
  Serial.println("Hello");
#ifdef DEBUG
  Serial.println("Getting started.....");
#endif
  //initialize blank array to hold list of devices, in case we need it
  for(int x = 0; x < max_devices; x++){ //Create blank array
    memcpy(device_IDs[x],"000000000000\0",13);
  }
  memcpy(device_IDs[0],"9D476CE5C578",12);

  bleSerialPort.begin(38400);
  Serial.println("Srarting BLE port");

  pinMode(button_pin, INPUT);
  pinMode(red_LED_pin, OUTPUT);
  pinMode(green_LED_pin, OUTPUT);
  pinMode(speaker_pin, OUTPUT);
  digitalWrite(red_LED_pin, !red_LED_on_state);
  digitalWrite(green_LED_pin, !red_LED_on_state);
  digitalWrite(speaker_pin, LOW);
  Serial.println("Setting outputs");  

  if(!(digitalRead(button_pin) == button_default_state)){ //Run when the button is pushed - SETUP
    Serial.println("First Run");
    PollForDevices();
    WriteDeviceListToEEPROM();
    //TODO: beep once for each device found
    HappyBeep();



  }
  else {
    Serial.println("Checkin");
    ReadDeviceListFromEEPROM();
    if (VerifyDevices()) {
#ifdef DEBUG
      Serial.println("All devices found");
#endif
      HappyBeep();
    } 
    else {
      //do unhappy beeep
#ifdef DEBUG
      Serial.println("Not all devices found"); 
#endif
      AngryBeep();  
    }

  }


}
void loop() { 
  Serial.println("Doing nothing");
  delay(100000);
}

void PollForDevices() {
  Serial.println("Polling for devices");
  ble112.ble_evt_gap_scan_response = PollForDevicesReceivedResponse;
  StartScan();
  uint16_t now = millis();
  while (millis() < now + device_scan_time * 1000) {
    ble112.checkActivity(1000); 
  }

  StopScan();
}
void PollForDevicesReceivedResponse(const ble_msg_gap_scan_response_evt_t *msg) {
  Serial.println("Poll for received response");
  //get the address, convert from binary to char array
  char current_address[13];
  current_address[12] = 0x00;
  for (uint8_t i = 0; i < 6; i++) {
    current_address[i*2] = hexDigit(msg->sender.addr[i] / 0x10);
    current_address[i*2+1] = hexDigit(msg->sender.addr[i] % 0x10);
  }
#ifdef DEBUG
  Serial.print("Found: ");
  Serial.println(current_address);    
#endif
  Serial.println(current_address);   
  for (int i=0;i<max_devices;i++) {
    if (strcmp(current_address,device_IDs[i]) == 0)
      return; //already in the list
    if (strcmp(device_IDs[i],"000000000000\0") == 0) {
      memcpy(device_IDs[i],current_address,12);    
#ifdef DEBUG
      Serial.println("   added device to list");
#endif 
      return;
    }
  }
}


void WriteDeviceListToEEPROM() {
  Serial.println("Writing devices to EEPROM)");
  for (int i=0;i<max_devices;i++) { //device
    for(int j=0;j<12;j++) {  //byte
      EEPROM.write(device_memory_address + j + i*12,device_IDs[i][j]);
    }
    Serial.println("Writing");
  }
  Serial.println("done writing");
}

void ReadDeviceListFromEEPROM() {
  for (int i=0;i<max_devices;i++) { //device
    for(int j=0;i<12;j++) {  //byte
      device_IDs[i][j] = EEPROM.read(device_memory_address + j + i*12);
    }
  }
}


int VerifyDevices(){  
  Serial.println("Verifying Devices");
  ble112.ble_evt_gap_scan_response = VerifyDevicesReceivedResponse;
  StartScan();
  uint16_t now = millis();
  while (millis() < now + device_scan_time * 1000) {
    ble112.checkActivity(1000); 
    if (CheckAllDevicesFound()) {
      StopScan();
      return true;
    }
  }

  StopScan();
  return CheckAllDevicesFound();

}

int CheckAllDevicesFound() {
  Serial.println("Checking if all devices are found");
  int all_devices_found = 1;
  for (int i=0;i<max_devices;i++) {
    if (strcmp("000000000000\0",device_IDs[i])) {
      all_devices_found = 0;
    }
  }

  return all_devices_found;

}

void VerifyDevicesReceivedResponse(const ble_msg_gap_scan_response_evt_t *msg) {

  //get the address, convert from binary to char array
  char current_address[13];
  current_address[12] = 0x00;
  for (uint8_t i = 0; i < 6; i++) {
    current_address[i*2] = hexDigit(msg->sender.addr[i] / 0x10);
    current_address[i*2+1] = hexDigit(msg->sender.addr[i] % 0x10);
  }
#ifdef DEBUG
  Serial.print("Found: ");
  Serial.println(current_address);    
#endif

  for (int i=0;i<max_devices;i++) {
    if (strcmp(current_address,device_IDs[i]) == 0) {
      memcpy(device_IDs[i],"000000000000",12);    
#ifdef DEBUG
      Serial.print("  Found and removed device from list");
#endif 
    }
  }
}

void StartScan() {
  int status;
  Serial.println("Starting scan");
  ble112.ble_cmd_gap_set_scan_parameters(0xC8, 0xC8, 1);
#ifdef DEBUG
  Serial.println("gap_set_scan_parameters");
#endif
  while ((status = ble112.checkActivity(1000)));

  ble112.ble_cmd_gap_discover(BGLIB_GAP_DISCOVER_GENERIC);
#ifdef DEBUG
  Serial.println("gap_cmd_discover");
#endif
  while ((status = ble112.checkActivity(1000)));
}

void StopScan() {
  Serial.println("Stopping Scan");
  ble112.ble_cmd_gap_end_procedure();
#ifdef DEBUG
  Serial.println("gap_end_procedure");
#endif
  Serial.println("Stopping Scan");
}


void HappyBeep(){//happy beep and LED on
  Serial.println("Happy");
  digitalWrite(green_LED_pin, green_LED_on_state);
  tone(speaker_pin, 400, 350); //-Make a single "chirp" on the buzzer (300ms or so)
  delay(350);
  tone(speaker_pin, 800, 150); //Makes chirp happy sounding
  delay(150);
  noTone(speaker_pin);  
  loop();
}

void AngryBeep(){
  Serial.println("Angry");
  attachInterrupt(button_pin, loop, CHANGE);
  int num_of_blinks = 7;  //Blink 5 times (interrupts should trigger first)  
  for(int x = 1; x <= num_of_blinks; x++){//-Blink the buzzer AND red LED off and on (it's annoying, so 500ms on, 1000ms off, repeated)
    tone(speaker_pin, 150, 700);
    digitalWrite(red_LED_pin, red_LED_on_state);
    delay(700);
    noTone(speaker_pin);    
    digitalWrite(red_LED_pin, !red_LED_on_state);
    delay(700);
  }
  digitalWrite(green_LED_pin, red_LED_on_state);
  loop();
}




char hexDigit(unsigned n)
{
  Serial.println("Converting");
  if (n < 10) {
    return n + '0';
  } 
  else {
    return (n - 10) + 'A';
  }
}





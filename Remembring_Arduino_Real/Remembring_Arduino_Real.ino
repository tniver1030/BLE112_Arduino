#include <EEPROM.h>
#include "BGLib.h"
//#include <SoftwareSerial.h>
#include "AltSoftSerial.h"
//Remembring Host Device - Arduino with BLE112
//warning: AltSoftSerial hard-coded to use pins 9,8 on Uno.

#define button_pin  4  
#define button_default_state HIGH //High will run setup mode with no button plugged in
#define red_LED_pin 6
#define red_LED_on_state HIGH
#define green_LED_pin 5
#define green_LED_on_state HIGH
#define speaker_pin 3
#define device_memory_address 0 //offset to store data in memory
#define max_devices 10
#define device_scan_time 15
#define reset_pin 7
#define DEBUG
#define BLE_Baud 9600

int ble112_awake = 0;
//HardwareSerial bleSerialPort = Serial1; //already defined by ProMicro
//SoftwareSerial bleSerialPort(6,7); // RX, TX
AltSoftSerial bleSerialPort;
BGLib ble112((HardwareSerial *)&bleSerialPort);
char device_IDs[max_devices][13]; //12-char address + \0
void setup() {    
  pinMode(reset_pin,OUTPUT);
  digitalWrite(reset_pin,HIGH);
  #ifdef DEBUG  
  Serial.begin(38400);
  while(!Serial);
  Serial.println("Debug is on");
  #endif
  
  bleSerialPort.begin(BLE_Baud); //Baud Rate of my device is 9600
  ble112.ble_evt_system_boot = my_evt_system_boot;
  ble112.ble_rsp_gap_discover = my_rsp_gap_discover;
  
  //initialize blank array to hold list of devices
  for(int x = 0; x < max_devices; x++){
    memcpy(device_IDs[x],"000000000000\0",13);
  }
  
  #ifdef DEBUG
  //memcpy(device_IDs[0],"9D476CE5C578",12); //Can be useful for when no physical device is avail for testing
  #endif

  //Set pinModes
  pinMode(button_pin, INPUT);
  pinMode(red_LED_pin, OUTPUT);
  pinMode(green_LED_pin, OUTPUT);
  pinMode(speaker_pin, OUTPUT);
  //Set default pin states
  digitalWrite(red_LED_pin, !red_LED_on_state);
  digitalWrite(green_LED_pin, !green_LED_on_state);
  digitalWrite(speaker_pin, LOW);

  ResetBLE112(); //power cycle, mostly needed for development

  if(!(digitalRead(button_pin) == button_default_state)){ //Run when the button is pushed - SETUP
    #ifdef DEBUG
    Serial.println("Running setup");
    #endif
    PollForDevices();  
    WriteDeviceListToEEPROM();
    //TODO: beep once for each device found
    HappyBeep();     
  }
  else {  //Run when button is not pushed, Checking run 
    #ifdef DEBUG
    Serial.println("Running normally (Check for devices)");
    #endif
    ReadDeviceListFromEEPROM();
    if (VerifyDevices()) {  //Verify Devices runs, and returns true if all devices are found
      #ifdef DEBUG
      Serial.println("All devices found");
      #endif
      HappyBeep();
    } 
    else {//Verify Devices returned false, all devices were nto found       
      #ifdef DEBUG
      Serial.println("Not all devices found\nThis is the array of missing devices:/nIgnore all 0s"); 
      for (int i = 0; i <max_devices; i++){
        Serial.println(device_IDs[i]);
      }
      #endif
      AngryBeep();  
    }
  }
}
void PollForDevices() {
  ble112.ble_evt_gap_scan_response = PollForDevicesReceivedResponse; //Callback for when a device is found
  StartScan();    //Starts scanning
  uint16_t now = millis(); //Makes variable with start time
  while (millis() < now + device_scan_time * 1000) { //Figures out when it has run more than the alloted time (in seconds)
     ble112.checkActivity(1000); //Checks for 1 second
     //Serial.println("Waiting");
  }
  StopScan(); //Stops the scan
}
void PollForDevicesReceivedResponse(const ble_msg_gap_scan_response_evt_t *msg) { //Runs when device is found
  //Serial.println("PollForDevicesReceivedResponse");
  //get the address, convert from binary to char array
  char current_address[13];  //Creates array to store the address of incoming BT Module
  current_address[12] = 0x00;  
  for (uint8_t i = 0; i < 6; i++) { //Converts address to readable value
        current_address[i*2] = hexDigit(msg->sender.addr[i] / 0x10);
        current_address[i*2+1] = hexDigit(msg->sender.addr[i] % 0x10);
  }
  #ifdef DEBUG
  Serial.print("PDRR - Found: ");
  Serial.println(current_address);       
  #endif
  for (int i=0;i<max_devices;i++) {//Loops through device number
    if (strcmp(current_address,device_IDs[i]) == 0){ // If the address of the BT and the device in the list is already found
      #ifdef DEBUG
      Serial.println("Already in the list");
      #endif
      return; //already in the list
    }
    else if (strcmp(device_IDs[i],"000000000000\0") == 0) { //If the above was not found, but an empty slot was
      memcpy(device_IDs[i],current_address,12); //Write address to memory
      #ifdef DEBUG
      Serial.println("Added device to list");
      #endif 
      return;
    }
  }
}


void WriteDeviceListToEEPROM() {
  for (int i=0;i<max_devices;i++) { //device
     #ifdef DEBUG
     //Below used to verify it wrote the corerct device
     Serial.println("Wrote Device:");
     for(int j=0;j<12;j++) {  //byte
       Serial.print(device_IDs[i][j]);
     }
     Serial.print("\n");
     #endif
     for(int j=0;j<12;j++) {  //byte     
       EEPROM.write(device_memory_address + j + i*12, device_IDs[i][j]);//Start address, + num bit we're on + device length
     }
     //Serial.println("Done Writing");     
     delay(100);
  }
}

void ReadDeviceListFromEEPROM() {
  #ifdef DEBUG
  Serial.println("Reading Devices from EEPROM"); 
  #endif
  for (int i=0;i<max_devices;i++) { //device
     for(int j=0;j<12;j++) {  //byte
       device_IDs[i][j] = EEPROM.read(device_memory_address + j + i*12);//Grabs value of EEPROM ... Start address, + num bit we're on + device length
     }
  }
  #ifdef DEBUG
  Serial.println("Done Reading Devices");
  #endif
}

int CheckAllDevicesFound() {  //retruns whether everything was found
  int all_devices_found = 1;
  for (int i=0;i<max_devices;i++) {
    if (strcmp("000000000000\0",device_IDs[i])) { //If it gets a value, return 0
      all_devices_found = 0;
    }
   }
  return all_devices_found; 
}

void ResetBLE112() { //Used for development
   #ifdef DEBUG
   Serial.println("Resetting BLE112");
   #endif
   ble112_awake = 0; 
   digitalWrite(reset_pin, LOW); //Resets
   delay(2);
   digitalWrite(reset_pin,HIGH);
   while (!ble112_awake) { //Wait for a boot message to be received 
    ble112.checkActivity(1000);
   }
   #ifdef DEBUG
   Serial.println("Reset complete");
   #endif
  
}

int VerifyDevices(){  
  ble112.ble_evt_gap_scan_response = VerifyDevicesReceivedResponse; //Callback for when device is found
  StartScan();  //Starts scanning
  uint16_t now = millis();  //variable to hold time before scan starts
  while (millis() < now + device_scan_time * 1000) { //Checks to make sure enough time has passed
     ble112.checkActivity(1000); 
     if (CheckAllDevicesFound()) { //Bounces out when all devices are found
      StopScan();
      return true;
     }
  }
  StopScan();
  return CheckAllDevicesFound(); //Returns whether or not all devices were found

}

void VerifyDevicesReceivedResponse(const ble_msg_gap_scan_response_evt_t *msg) {
  //get the address, convert from binary to char array
  char current_address[13];
  current_address[12] = 0x00;
  for (uint8_t i = 0; i < 6; i++) {
        current_address[i*2] = hexDigit(msg->sender.addr[i] / 0x10); //Converts
        current_address[i*2+1] = hexDigit(msg->sender.addr[i] % 0x10);
  }
  #ifdef DEBUG
  Serial.print("VDRR - Found: ");
  Serial.println(current_address);    
  #endif
  for (int i=0;i<max_devices;i++) {
    if (strcmp(current_address,device_IDs[i]) == 0) { //Checks if the device is on the list
      memcpy(device_IDs[i],"000000000000\0",12);    //If it is, replace it with empty chars
      #ifdef DEBUG
      Serial.print("Found and removed device from list \n");
      #endif 
      QuickBeep();
    }
  }
}

void StartScan() {
  int status;
  ble112.ble_cmd_gap_set_scan_parameters(0xC8, 0xC8, 1);
  #ifdef DEBUG
  Serial.println("Setting GAP scan parameters");
  #endif
  while ((status = ble112.checkActivity(1000)));
  ble112.ble_cmd_gap_discover(BGLIB_GAP_DISCOVER_GENERIC);
  #ifdef DEBUG
  Serial.println("Searching for devices");
  #endif
  while ((status = ble112.checkActivity(1000)));
}

void StopScan() {
  ble112.ble_cmd_gap_end_procedure();
  #ifdef DEBUG
  Serial.println("Done searching for devices");
  #endif
}


void HappyBeep(){//happy beep and LED on
  #ifdef DEBUG
  Serial.println("HappyBeep");
  #endif
  digitalWrite(red_LED_pin, !red_LED_on_state);
  digitalWrite(green_LED_pin, green_LED_on_state);
  tone(speaker_pin, 400, 350); //-Make a single "chirp" on the buzzer (300ms or so)
  delay(350);
  tone(speaker_pin, 800, 150); //Makes chirp happy sounding
  delay(150);
  noTone(speaker_pin);  
  loop();
}

void AngryBeep(){
  #ifdef DEBUG
  Serial.println("AngryBeep");
  #endif
  int num_of_blinks = 7;  //Blink 5 times (interrupts should trigger first)  
  for(int x = 1; x <= num_of_blinks; x++){//-Blink the buzzer AND red LED off and on (it's annoying, so 500ms on, 1000ms off, repeated)
      tone(speaker_pin, 150, 700);
      digitalWrite(green_LED_pin, !green_LED_on_state);
      digitalWrite(red_LED_pin, red_LED_on_state);
      if (digitalRead(button_pin) != button_default_state) loop();
      delay(700);
      noTone(speaker_pin);    
      digitalWrite(red_LED_pin, !red_LED_on_state);
      delay(700);
  }
  digitalWrite(red_LED_pin, red_LED_on_state);
  loop();
}

void QuickBeep(){ //Runs when one thing is found (Has to run really quick)
  #ifdef DEBUG
  Serial.println("QuickBeep");
  #endif
  tone(speaker_pin, 800, 150); //Makes chirp happy sounding
  delay(150);
}

char hexDigit(unsigned n) //Converts
{
  if (n < 10) {
    return n + '0';
  } 
  else {
    return (n - 10) + 'A';
  }
}

void my_rsp_system_hello(const ble_msg_system_hello_rsp_t *msg) {
    #ifdef DEBUG
    Serial.println("<--\tsystem_hello");
    #endif
}

void my_evt_system_boot(const ble_msg_system_boot_evt_t *msg) {
    #ifdef DEBUG
      Serial.print("###\tsystem_boot: { ");
      Serial.print("major: "); Serial.print(msg -> major, HEX);
      Serial.print(", minor: "); Serial.print(msg -> minor, HEX);
      Serial.print(", patch: "); Serial.print(msg -> patch, HEX);
      Serial.print(", build: "); Serial.print(msg -> build, HEX);
      Serial.print(", ll_version: "); Serial.print(msg -> ll_version, HEX);
      Serial.print(", protocol_version: "); Serial.print(msg -> protocol_version, HEX);
      Serial.print(", hw: "); Serial.print(msg -> hw, HEX);
      Serial.println(" }");
    #endif
    
    ble112_awake = 1; 
}

void my_rsp_gap_set_scan_parameters(const ble_msg_gap_set_scan_parameters_rsp_t *msg) {
    #ifdef DEBUG
    Serial.print("<--\tgap_set_scan_parameters: { ");
    Serial.print("result: "); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(" }");
    #endif
}

void my_rsp_gap_discover(const ble_msg_gap_discover_rsp_t *msg) {
    #ifdef DEBUG
    Serial.print("<--\tgap_discover: { ");
    Serial.print("result: "); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(" }");
    #endif
}

void my_rsp_gap_end_procedure(const ble_msg_gap_end_procedure_rsp_t *msg) {
    #ifdef DEBUG
    Serial.print("<--\tgap_end_procedure: { ");
    Serial.print("result: "); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(" }");
    #endif
}
void onTimeout() {
    #ifdef DEBUG
    Serial.println("!!!\tTimeout occurred!");
    #endif
}

void loop() { 
  //Do nothing - Everything already ran once
}

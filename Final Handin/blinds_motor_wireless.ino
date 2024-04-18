#include <Stepper.h>
#include <BVSP.h>
#define STEPS 2038 // the number of steps in one revolution of your motor (28BYJ-48)
#include <SoftwareSerial.h>
SoftwareSerial HC12(10,11);

// define stepper motor for raising/lowering blinds - connected to pin 6-9
Stepper stepper(STEPS, 6, 8, 7, 9);
// define stepper motor for opening/closing blinds - connected to pins 2-5
Stepper rotationStepper(STEPS, 2, 4, 3, 5);

// Define no. of rotations of stepper motor for various blinds operations
float raiseBlindsRotations = 5.5;
float lowerBlindsRotations = -4.5;
float closeBlindsRotations = 6.0;
float openBlindsRotations = -6.0;

// Defines the constants that will be passed as parameters to
// the BVSP.begin function
const unsigned long STATUS_REQUEST_TIMEOUT = 1000;
const unsigned long STATUS_REQUEST_INTERVAL = 2000;

// Defines the size of the receive buffer
const int RECEIVE_BUFFER_SIZE = 4;

// variable for tracking the time
unsigned long timerStart = 0;
unsigned long alarmStart = 0;

// wait time for how long blinds should be lowered for
int waitTime = 0;

// Initializes a new global instance of the BVSP class
BVSP bvsp = BVSP();

// buffer for receiving data from BitVoicer defined
// Just a byte array
byte receiveBuffer[RECEIVE_BUFFER_SIZE];

// assume blinds are lowered at boot
boolean blindsLowered = true;

// assume blinds open at boot
boolean blindsClosed = false;

boolean soundEnabled = true;

void setup() {
  // Starts serial communication at 115200 baudrate
  Serial.begin(115200);
  HC12.begin(9600);
  // Sets the Arduino serial port that will be used for
  // communication, how long it will take before a status request
  // times out and how often status requests should be sent to
  // BitVoicer Server.
  bvsp.begin(Serial, STATUS_REQUEST_TIMEOUT,
             STATUS_REQUEST_INTERVAL);

  // Defines the function that will handle the frameReceived
  // event which is when data is received from bitvoicer
  bvsp.frameReceived = BVSP_frameReceived;
  // set up builtin led for output
  pinMode(LED_BUILTIN, OUTPUT);
  // turn on led since blinds are assumed to be down at the start
  digitalWrite(LED_BUILTIN, HIGH);

}

// Function to raise the blinds, currently rotates 1 revolution at 10rpm (6s per revolution)
void raiseBlinds() {
  // Check if blinds are lowered, if blinds are already up do nothing
  if (blindsLowered) {
    digitalWrite(LED_BUILTIN, LOW);
    stepper.setSpeed(10);
    stepper.step(raiseBlindsRotations*STEPS);
    blindsLowered = false;
  }
}

// Function to lower the blinds, currently rotates 1 revolution at 10rpm (6s per revolution)
void lowerBlinds() {
  // Check if blinds are not lowered. If blinds are already down do nothing
  if (!blindsLowered) {
    digitalWrite(LED_BUILTIN, HIGH);
    stepper.setSpeed(10);
    stepper.step(lowerBlindsRotations*STEPS);
    blindsLowered = true;
  }
}
// Function that lowers blinds for certain amount of time. Gets the current time
// and stores it in timerStart
void lowerBlindsTime(){
  digitalWrite(LED_BUILTIN, HIGH);
  stepper.setSpeed(12);
  stepper.step(lowerBlindsRotations*STEPS);
  blindsLowered = true;
  timerStart = millis();
}

// Function that raises blinds for certain amount of time. Sets timerStart to 0
// which stops the code from checking if its time to raise blinds
void raiseBlindsTime(){
  digitalWrite(LED_BUILTIN,LOW);
  stepper.setSpeed(12);
  stepper.step(raiseBlindsRotations*STEPS);
  blindsLowered = false;
  timerStart = 0;
}

// Function to close blinds
void closeBlinds(){
  // Check if blinds are currently open
  if(!blindsClosed){
    // Set speed and rotate stepper
    rotationStepper.setSpeed(12);
    rotationStepper.step(closeBlindsRotations*STEPS);
    blindsClosed = true;
  }
}

// Function to open blinds
void openBlinds(){
  // Check if blinds are currently closed
  if(blindsClosed){
    // Set speed and rotate stepper
    rotationStepper.setSpeed(12);
    rotationStepper.step(openBlindsRotations*STEPS);
    blindsClosed = false;
  }
}


void loop() {
  // Checks if the status request interval has elapsed and if it
  // has, sends a status request to BitVoicer Server
  bvsp.keepAlive();
  // Checks if there is data available at the serial port buffer
  // and processes its content according to the specifications
  // of the BitVoicer Server Protocol
  bvsp.receive();
  // If the timer was started, check if its time to raise the blinds
  if(timerStart != 0){
    if(millis() >= timerStart +  waitTime){
      raiseBlindsTime();
    }
  }

  // Receive data from alarm Arduino
  if(HC12.available()){
      // Convert received byte to int and parse it
      int readByte = (int) HC12.read();
      // If 1 received - raise blinds
      // This is sent by alarm Arduino when alarm starts ringing to let light in
      if(readByte == 1){
        raiseBlinds();
      }
      // If 2 received - lower blinds
      // This is sent by alarm Arduino when alarm is snoozed
      else if(readByte == 2){
        lowerBlinds();
      }
      // If 6 received - close blinds
      // This is sent by alarm Arduino when alarm is snoozed
      else if(readByte == 6){
        closeBlinds();
      }
      // If 7 received - open blinds
      // This is sent by alarm Arduino 1 minute before alarm time
      else if(readByte == 7){
        openBlinds();
      }
  }
}

// Handles the frameReceived event
// BitVoicer supports payloads up to 1023 bytes - payloadSize is passed from
// BitVoicer to Arduino as a parameter here, along with dataType
void BVSP_frameReceived(byte dataType, int payloadSize)
{
  // Checks if the received frame contains binary data
  // 0x01 = Binary data (byte array)
  // Can check for and use other data types here
  if (dataType == DATA_TYPE_BINARY)
  {
    // If 2 bytes were received, process the command.
    // getReceivedBytes returns the no. of bytes copied ot the buffer
    if (bvsp.getReceivedBytes(receiveBuffer, RECEIVE_BUFFER_SIZE) ==
        RECEIVE_BUFFER_SIZE)
    { 
      // Transmit data to alarm Arduino - data format specified in report
      for(int i=0;i<4;i++){
        HC12.write(receiveBuffer[i]);
      }
      Serial.println("receive frame");
      Serial.println(receiveBuffer[0]);
      // If first byte is 1, raise blinds
      if (receiveBuffer[0] == 0x01) {
        raiseBlinds();
      }
      // If first byte is 2, lower blinds
      else if (receiveBuffer[0] == 0x02) {
        lowerBlinds();
      }
      // If first byte is 3, lower the blinds for certain time
      else if(receiveBuffer[0] == 0x03){
        // Retrieve the wait time from the 2nd byte
        waitTime = (int) receiveBuffer[1];
        if(!blindsLowered){
        lowerBlindsTime();
        }
      }
        // If the first byte is 4, alarm time interval mode
          // next 3 bytes correspond to hour min sec respectively
          // hour - 0 to 255, minute - 0-60, second - 0-60
       // If first byte is 5, alarm absolute time mode
        // next 2 bytes correspond to hour and min respectively
        // 24 hour time - hour 0-24, min 0-60

      // If first byte is 6 - close blinds
       else if(receiveBuffer[0] == 0x06){
        // close blinds
        closeBlinds();
       }
       // if first byte is 7 - open blinds
       else if(receiveBuffer[0] == 0x07){
        openBlinds();
      }
    }
  }
}

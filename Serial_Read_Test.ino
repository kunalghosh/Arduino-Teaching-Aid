#include <string.h>

#define CARRIAGE_RETURN 13
#define SPACE 32
#define SERIAL_READ_CHARS 2
#define NUM_BYTES_TO_SHIFT 3
#define BITS_IN_BYTE 8
#define NUM_BITS_IDENTIFY_BUS 2
#define INVALID_BUS_BIT -1

#define BLINK_ON_TIME 250
#define BLINK_OFF_TIME 250

#define DATA_PIN 4
#define LATCH_PIN 5
#define CLOCK_PIN 6

/* SET Bits or CLEAR Bits*/
#define SET 1
#define CLEAR 0 

#define DEBUG
/*    
* Stack Overflow : http://stackoverflow.com/q/1431576
* static : used to prevent these variables conflicting
* with variables defined in another file.
* 
* const char var_name[] : used instead of char *var_name
* to ensure that we can use sizeof() on the variable name.
* however we could have used #define var_name and even in 
* that case we could have used sizeof()
*/

/*BUS Constants*/
static const char BUS_V_1[] = "v1";
static const char BUS_V_2[] = "v2";
static const char BUS_V_3[] = "v3";
static const char BUS_V_4[] = "v4";
static const char BUS_H_1[] = "h1";
static const char BUS_H_2[] = "h2";
static const char BUS_H_3[] = "h3";
static const char BUS_H_4[] = "h4";
static const char BUS_H_5[] = "h5";
static const char BUS_H_6[] = "h6";
static const char BUS_H_7[] = "h7";
static const char BUS_H_8[] = "h8";

/*Serial Commands*/
static const char LIGHT[]        = "LL"; // Light up a particular bus.
static const char TURN_OFF[]     = "TO"; // Turn off a particular bus.
static const char TURN_OFF_ALL[] = "TA"; // Turn off all buses.
static const char BLINK[]        = "B1"; // Blink a single bus.
static const char BLINK_2[]      = "B2"; // Blink two buses one after another.

// +1 is to ensure that the character array can end with a \0 to terminate the string.
char serialData[SERIAL_READ_CHARS+1]={'\0'};

// constants to maintain state of buses 
// All buses ON = 16777215
unsigned long busState = 0;
//const unsigned long busesOff = 0;
  
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  // Setup pins which operate on the shift register.
  pinMode(DATA_PIN,OUTPUT);
  pinMode(LATCH_PIN,OUTPUT);
  pinMode(CLOCK_PIN,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  // serialData was coming out all garbled
  // see the explanation in getSerialData()
  getSerialData(serialData);
  executeCommand(serialData);
  serialData[0]='\0';
}

void executeCommand(char *serialData){
  //check which command it is.
  if(isAvailable(serialData)){
    if(isEqual(serialData,LIGHT)){
      executeLightCommand();
    }else if(isEqual(serialData,TURN_OFF)){
      executeTurnOffCommand();
    }else if(isEqual(serialData,TURN_OFF_ALL)){
      executeTurnOffAllCommand();
    }else if(isEqual(serialData,BLINK)){
      executeBlinkCommand();
    }else if(isEqual(serialData,BLINK_2)){
      executeBlinkTwoCommand();
    }else{
      printDebugMessage("Unknown Command",serialData);
    }
  }
}

int isAvailable(const char* value){
  int len = strlen(value);
  int retVal = 0;
  if(SERIAL_READ_CHARS == len){
    retVal = 1;
  }
  return retVal;
}

bool isEqual(const char* value1, const char* value2){
  return !strcmp(value1, value2);
}

/*
Send 4 characters via serial. 
First Two indicate the command
Second Two indicate which bus to turn on

Ex. Send: TTv1
*/
void executeLightCommand(){
  printDebugMessage("executeLightCommand","Start of function");

  // Read next two characters from serial monitor
  // To figure out which bus to turn ON.
  char bus[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(bus);

  // Get the bits which need to be set for this bus
  int busBits[NUM_BITS_IDENTIFY_BUS] = {-1};
  getBitsCorrespondingToBus(bus, busBits);

  // Update this bit pattern in the busState variable.
  setOrClearBitPattern(&busState, busBits, SET);
  
  //shift the bits out.
  displayOnBoard(busState);
}

/*
Send 4 characters via serial. 
First Two indicate the command
Second Two indicate which bus to turn OFF 

Ex. Send: TOv1 (Turns off the v1 bus and leaves other bus states unaffected.)
*/
void executeTurnOffCommand(){
  printDebugMessage("executeTurnOffCommand","Start of function");

  // Read next two characters from serial monitor
  // To figure out which bus to turn OFF.
  char bus[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(bus);

  // Get the bits which need to be UN-SET for this bus.
  int busBits[NUM_BITS_IDENTIFY_BUS] = {-1};
  getBitsCorrespondingToBus(bus,busBits);

  // Update the bit pattern in the busState Variable.
  setOrClearBitPattern(&busState, busBits, CLEAR);

  //shift the bits out.
  displayOnBoard(busState);
}

/*
Send 2 characters via serial. 
The character pair indicate the command

Ex. Send: TA (Turns off all the buses)
*/
void executeTurnOffAllCommand(){
  printDebugMessage("executeTurnOffAllCommand","Start of function");
  // Set bus State to 0
  busState = 0;
  displayOnBoard(busState);
}

/*
Send 6 characters via serial. 
First Two indicate the command (B1 in this case)
Second Two indicate which bus to blink
Last Two indicate how many times to blink (eg 05 indicates blink 5 times)

Ex. Send: B1v105 (Blink v1 five times)
*/
void executeBlinkCommand(){
  int numBusesToBlink = 1;
  printDebugMessage("executeBlinkCommand","Start of function");

  // Read next two characters from serial monitor
  // To figure out which bus to blink.
  char bus[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(bus);

  // Get the bits which need to be toggled to blink the bus got above. 
  int busBits[NUM_BITS_IDENTIFY_BUS] = {-1};
  getBitsCorrespondingToBus(bus,busBits);

  // Get the next two bits which would identify how many times to blink
  char blinkCountChars[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(blinkCountChars);

  // Put the bus bits into another int array so that blinkBus can be a
  // Generic function which blinks one bus or multiple buses.
  int* busesToBlink[] = {busBits};

  // Blink the bus
  blinkBus(busState, busesToBlink, numBusesToBlink, stringToInt(blinkCountChars));
}

/*
Send 8 characters via serial. 
First Two indicate the command (B2 in this case)
Next Two indicate the first bus which must be blinked.
Next Two indicate the next bus which must be blinked.
Last Two indicate the number of times to blink (eg 05 indicates blink 5 times)

Ex. Send: B2v1v505 (Blink v1 and v5 one after another 5 times)
*/
void executeBlinkTwoCommand(){
  int numBusesToBlink = 2;
  printDebugMessage("executeBlinkTwoCommand","Start of function");  
  // Read next two characters from serial monitor
  // To figure out which bus to blink.
  char bus[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(bus);

  // Get the bits which need to be toggled to blink the bus got above. 
  int busBits1[NUM_BITS_IDENTIFY_BUS] = {-1};
  getBitsCorrespondingToBus(bus,busBits1);
 
  // Read the next two characters from serial monitor
  // To figure out which other bus to blink.
  getSerialData(bus);
  int busBits2[NUM_BITS_IDENTIFY_BUS] = {-1};
  getBitsCorrespondingToBus(bus,busBits2);

  // Put the bus bits into another int array so that blinkBus can be a
  // Generic function which blinks one bus or multiple buses.
  int* busesToBlink[] = {busBits1, busBits2};

  // Get the next two bits which would identify how many times to blink
  char blinkCountChars[SERIAL_READ_CHARS+1]={'\0'};
  getSerialData(blinkCountChars);

  // Blink the bus
  blinkBus(busState, busesToBlink, numBusesToBlink, stringToInt(blinkCountChars));

}

void getSerialData(char* serialData){
  // Read SERIAL_READ_CHARS from the serial
  // When using serial monitor ensure that you select "NO LINE ENDING"
  if(Serial.available() > 0){
    Serial.readBytes(serialData, SERIAL_READ_CHARS);
    printDebugMessage("Serial Read",serialData);
  }
}

void printDebugMessage(const char* messageContext, const char* message){
  #ifdef DEBUG
  Serial.print("DEBUG: ");
  Serial.print(messageContext);
  Serial.print(" - ");
  Serial.print(message);
  Serial.print("\n");
  #endif
}

void displayOnBoard(unsigned long data){
  digitalWrite(LATCH_PIN, LOW);
  for(int counter = 0; counter < NUM_BYTES_TO_SHIFT; counter++){
    shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, data);
    // When LSBFIRST is changed to MSBFIRST
    // Change >> to << in the following statement
    data = data >> BITS_IN_BYTE;
  }
  digitalWrite(LATCH_PIN, HIGH);
}

void getBitsCorrespondingToBus(const char* busConstant, int* returnVal){
  // Assumes returnVal is an integer array requiring two numbers.
  // The two numbers signify the two bit positions to turn on for the
  // corresponding bus.
  // If the busConstant doesn't match any avaiable bus constant then 
  // returnVal is initialized with {-1, -1}
  if(isEqual(busConstant,BUS_V_1)){
    returnVal[0] = 0;
    returnVal[1] = 1;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_V_2)){
    returnVal[0] = 2;
    returnVal[1] = 3;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_V_3)){
    returnVal[0] = 4;
    returnVal[1] = 5;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_V_4)){
    returnVal[0] = 6;
    returnVal[1] = 7;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_1)){
    returnVal[0] = 8;
    returnVal[1] = 9;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_2)){
    returnVal[0] = 10;
    returnVal[1] = 11;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_3)){
    returnVal[0] = 12;
    returnVal[1] = 13;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_4)){
    returnVal[0] = 14;
    returnVal[1] = 15;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_5)){
    returnVal[0] = 16;
    returnVal[1] = 17;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_6)){
    returnVal[0] = 18;
    returnVal[1] = 19;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_7)){
    returnVal[0] = 20;
    returnVal[1] = 21;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else if(isEqual(busConstant,BUS_H_8)){
    returnVal[0] = 22;
    returnVal[1] = 23;
    printDebugMessage("getBitsToSet : Valid Bit", busConstant);
  }else{
    returnVal[0] = INVALID_BUS_BIT;
    returnVal[1] = INVALID_BUS_BIT;
    printDebugMessage("getBitsToSet : Invalid Bit to set", busConstant);
  }
//  if(returnVal[0] != INVALID_BUS_BIT && returnVal[1] != INVALID_BUS_BIT){
//    char temp0[] = {(char)returnVal[0]+48,'\0'};
//    char temp1[] = {(char)returnVal[1]+48,'\0'};
//    printDebugMessage("Bit Set 0", temp0);
//    printDebugMessage("Bit Set 1", temp1);
//  }
}

void setOrClearBitPattern(unsigned long* busState, int* busBits, const int setToOneOrZero){
  for(int index=0; index < NUM_BITS_IDENTIFY_BUS; index++){
    if(isValidBusBit(busBits[index])){
      if(setToOneOrZero == SET){
        bitSet(*busState, busBits[index]);
      } else if(setToOneOrZero == CLEAR) {
        bitClear(*busState, busBits[index]);
      }
    } 
  }
}

int isValidBusBit(int val){
  return(val != INVALID_BUS_BIT);
}

int stringToInt(const char* intString){
  String str = String(intString);
  return(str.toInt());
}


/*
was passing an int** to const int** but was getting
invalid conversion from 'int**' to 'const int**' [-fpermissive]
*/
void blinkBus(unsigned long busState, int** busesToBlink, int numBusesToBlink, int blinkCount){

  //int numBusesToBlink = lenIntArr(busesToBlink);
  Serial.print("Number of buses to blink : ");
  Serial.print(numBusesToBlink);
  Serial.print("\n");

  unsigned long busStateCopy = busState;
  for(int blinked = 0 ; blinked < blinkCount ; blinked++){
    for(int busIdxBlinked = 0; busIdxBlinked < numBusesToBlink; busIdxBlinked++){
      setOrClearBitPattern(&busStateCopy, busesToBlink[busIdxBlinked], SET);
      displayOnBoard(busStateCopy);
      delay(BLINK_ON_TIME);

      setOrClearBitPattern(&busStateCopy, busesToBlink[busIdxBlinked], CLEAR);
      displayOnBoard(busStateCopy);
      delay(BLINK_OFF_TIME);
    }  
  }  
  
  // set the board to the original state.
  displayOnBoard(busState);
}

// int lenIntArr(int** intArray){
//   // This assumes that each entry of intArray e.g. intArray[0] , intArray[1] etc
//   // are of the same dimensions, in this case
//   return(sizeof(intArray)/sizeof(intArray[0]));
// }

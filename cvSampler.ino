#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac;

//pinMap
const int recordButton = 2;
const int cycleButton = 3;
const int cycleLED = 5;
const int recLED = 4;
const int potPin = A2;
const int envIn = A0;
const int trigIn = A1;

//loop vars
bool cycling = false;
bool recording = false;
bool triggered = false;

int recButStateCurrent;
int recButStatePrevious;
int cycleButStateCurrent;
int cycleButStatePrevious;

long potState;
long envInState;
long trigInState;
long cycleButtonTime;

int recVal;
int buffPosition = 0;
unsigned long buffLength;
unsigned long playbackTime;
unsigned long time;
unsigned long startTime = 0;

int buff[299] =
{};

int lookupTime[299]  =
{};

void setup(void) {
  Serial.begin(9600);
  dac.begin(0x62);
  for(int x=0;x<6000;x+=20){
      lookupTime[x/20] = x;
    }
}

long scale(long newMin, long newMax, long oldMin, long oldMax, long oldValue){
  return (((oldValue - oldMin) * (newMax - newMin)) / (oldMax - oldMin)) + newMin;    
}

//outputs the corresponding buffer voltage on the dac, if it reached 
//the corresponding time location it was recorded.
int playbackBuff(){
    if (playbackTime >= lookupTime[buffPosition]){
      dac.setVoltage(buff[buffPosition],false);
      analogWrite(11,buff[buffPosition]);
      buffPosition++;
    }
  }

void loop(){
  
    //updates from time/hardware
    time = millis();
    recButStateCurrent = digitalRead(recordButton);
    cycleButStateCurrent = digitalRead(cycleButton);
    potState = analogRead(potPin);
    envInState = scale(0,1023,0,570,analogRead(envIn));
    trigInState = analogRead(trigIn);

    //check cycle button
    if (cycleButStateCurrent && !cycleButStatePrevious && cycling == 0 && millis()-cycleButtonTime>200){
      cycling = 1;
      digitalWrite(cycleLED,HIGH);
      Serial.println("cycling on");
      cycleButtonTime = millis();
    } else if (cycleButStateCurrent && !cycleButStatePrevious && cycling == 1 && millis()-cycleButtonTime>200){
      cycling = 0;
      digitalWrite(cycleLED,LOW);
      Serial.println("cycling off");
      cycleButtonTime = millis();
    }

    if (recButStateCurrent && !recButStatePrevious && !recording){
      digitalWrite(recLED,HIGH);
      digitalWrite(cycleLED,LOW);
      buffPosition = 0;
      buffLength = 0;
      startTime = time;
      Serial.println("recording");
      recording = true;
      cycling = false;

    }


    //if rec button released
    if (!recButStateCurrent && recording){
      Serial.println("done recording");
      digitalWrite(recLED, LOW);
      recording = false;
    }

    //if a trigger was received, prepare for single buffer playback
    if (trigInState > 980 && !recording && !cycling){
        Serial.println("triggered");
        startTime = time;
        triggered = true;
        playbackTime = 0;
        buffPosition = 0;
    }

    //playback buffer once per trigger
    if (triggered){
      playbackTime = time-startTime;
      if (playbackTime >= buffLength){
          triggered = false;
        }
      playbackBuff();     
    }

    //if cycling mode enabled
    if (cycling){
      playbackTime = time-startTime;
      if (playbackTime >= buffLength){
          playbackTime = 0;
          buffPosition = 0;
          startTime = time;
          Serial.println("cycle complete");
        }
      playbackBuff();
    }
    
    if (recording) {
      buffLength = time-startTime;
      Serial.println(buffLength);
      if (buffLength >= lookupTime[buffPosition]){ //if passed a time marker
          recVal = (abs(potState-envInState)*4);
          if (recVal > 1023) { 
            recVal = 1023; //set corresponding buffer value to envIn plus pot as offset
          }
          buff[buffPosition] = recVal;
          buffPosition++;
        }       
      //stop recording if buffer over 6 seconds
      if (buffLength >= 6000){
        Serial.println("reached max buffer");
        recording = 0;
        digitalWrite(recLED, LOW);
      }             
    }
    
    cycleButStatePrevious = cycleButStateCurrent;
    recButStatePrevious = recButStateCurrent;
  }

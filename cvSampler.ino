#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac1;
Adafruit_MCP4725 dac2;

//pinMap
const int recordButton = 8;
const int cycleButton = 9;
const int cycleLED = 7;
const int recLED = 6;
const int potPin = A2;
const int envIn = A0;
const int trigIn = A1;

//loop vars
bool cycling = false;
bool recording = false;
bool triggered = false;
bool recTrig = false;

int recButStateCurrent;
int recButStatePrevious;
int cycleButStateCurrent;
int cycleButStatePrevious;

long potState;
long envInState;
long trigInStateCurrent;
long trigInStatePrevious;
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
  dac1.begin(0x62);
  dac2.begin(0x65);
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
      Serial.println(buff[buffPosition]);
      dac1.setVoltage(buff[buffPosition],false);
      analogWrite(11,(buff[buffPosition]/16)); 
      buffPosition++;
    }
  }

void loop(){
  
    //updates from time/hardware
    time = millis();
    recButStateCurrent = digitalRead(recordButton);
    cycleButStateCurrent = digitalRead(cycleButton);
    potState = analogRead(potPin);
    envInState = scale(0,1023,0,600,analogRead(envIn));
    trigInStateCurrent = analogRead(trigIn);
 
    //dedicated envelope follower output
    dac2.setVoltage(envInState*4,false);

    //if holding down both buttons simultaneously, start recording at trigger 
    if (recButStateCurrent && cycleButStateCurrent){
        digitalWrite(recLED,LOW);
        digitalWrite(cycleLED,LOW);
        cycling = false;
        recording = false;
        if (trigInStateCurrent > 700 && trigInStatePrevious < 20){
          Serial.println("recording triggered");
          digitalWrite(recLED,HIGH);
          digitalWrite(cycleLED,LOW);
          buffPosition = 0;
          buffLength = 0;
          startTime = time;
          Serial.println("recording");
          recording = true;
        }
    }   

    //check cycle button state, set cycling vars accordingly
    if (cycleButStateCurrent && !recButStateCurrent && !cycleButStatePrevious && cycling == 0 && millis()-cycleButtonTime>200){
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

    //output 0 Volts to dac if not cycling or triggered
    if (!cycling && !triggered){
      dac1.setVoltage(0,false);
      analogWrite(11,0); 
    }

    //start recording if rec button held down
    if (recButStateCurrent && !recButStatePrevious && !recording && !cycleButStateCurrent){
      digitalWrite(recLED,HIGH);
      digitalWrite(cycleLED,LOW);
      buffPosition = 0;
      buffLength = 0;
      startTime = time;
      Serial.println("recording");
      recording = true;
      cycling = false;
    }

    //if rec button released, stop recording
    if (!recButStateCurrent && recording){
      Serial.println("done recording");
      digitalWrite(recLED, LOW);
      recording = false;
    }

    //if a trigger was received, prepare for single buffer playback
    if (trigInStateCurrent > 700 && trigInStatePrevious < 20 && !recording && !cycling){
        Serial.println("triggered");
        startTime = time;
        triggered = true;
        playbackTime = 0;
        buffPosition = 0;
    }

    //playback buffer once per trigger
    if (triggered){
      playbackTime = time-startTime;
      if (playbackTime >= buffLength){ //if buffer play complete, stop
          triggered = false;
        }
      playbackBuff();     
    }

    //if cycling mode enabled
    if (cycling){
      playbackTime = time-startTime;
      if (playbackTime >= buffLength){ //if buffer play complete, reset and play again
          playbackTime = 0;
          buffPosition = 0;
          startTime = time;
          Serial.println("cycle complete");
        }
      playbackBuff(); 
    }

    //records to the buffer 
    if (recording) {
      buffLength = time-startTime;
      Serial.println(buffLength);
      if (buffLength >= lookupTime[buffPosition]){ //if passed a time marker
          recVal = (abs(potState-envInState)*4);
          if (recVal > 4095) { 
            recVal = 4095; //set corresponding buffer value to envIn plus pot as offset
          } else if (recVal < 200) {
            recVal = 0;  
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
    trigInStatePrevious = trigInStateCurrent;
}

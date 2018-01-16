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

int buff[599] =
{};

void setup(void) {
  Serial.begin(9600);
  dac1.begin(0x62);
  dac2.begin(0x65);
}

long scale(long newMin, long newMax, long oldMin, long oldMax, long oldValue){
  return (((oldValue - oldMin) * (newMax - newMin)) / (oldMax - oldMin)) + newMin;    
}

//outputs the corresponding buffer voltage on the dac, if it reached 
//the corresponding time location it was recorded.
int playbackBuff(){
    if (playbackTime >= buffPosition*10){
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

    //output the input to dac if not cycling or triggered
    if (!cycling && !triggered){
      dac1.setVoltage((abs(potState-envInState)*4),false);
      analogWrite(11,abs(potState-envInState)/4); 
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
    
    if (recButStateCurrent && cycleButStateCurrent){
       cycling = false;  
       digitalWrite(cycleLED,LOW);        
    }

    //if a trigger was received, check if both buttons are pressed
    //if they are, begin recording at trigger time. otherwise, trigger
    //prepares for a single buffer playback
    if (trigInStateCurrent > 650 && trigInStatePrevious < 50){
        if(recButStateCurrent && cycleButStateCurrent){
            cycling = false;
            Serial.println("recording triggered");
            digitalWrite(recLED,HIGH);
            digitalWrite(cycleLED,LOW);
            buffPosition = 0;
            buffLength = 0;
            startTime = time;
            Serial.println("recording");
            recording = true;           
          } else if (!recording && !cycling){
            Serial.println("playback triggered");
            startTime = time;
            triggered = true;
            playbackTime = 0;
            buffPosition = 0;
          }
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
      if (buffLength >= buffPosition*10){ //if passed a time marker
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

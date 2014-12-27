/*Turn on midi with piano key press. Press causes the attached pin to go high by external pullup
version 2 has an octave switch and 
*/
#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3); // RX, TX
// constants won't change. Used here to 
// set pin numbers:
const int C1 =   5;      // the pin connected to the C1 key
const int D1 =   6;
const int E1 =   7;
const int F1 =   8;
const int G1 =   9;
const int An1 = 10;
const int Bn1 = 11;
const int C2 =  12;
const int octShift = 13;

const int Cs = A0;
const int Ds = A1;
const int Fs = A2;
const int Gs = A3;
const int As = A4;
const int voiceSel = A5;



// Variables will change:
int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated
 // byte ledPin = 13; //MIDI traffic inidicator
volatile unsigned long resetPiano = 0;
// the follow variables is a long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long interval = 1000;           // interval at which to blink (milliseconds)

void setup() 
{
  // set the digital pin as output:
  pinMode(C1, INPUT);      
  pinMode(D1, INPUT);    
  pinMode(E1, INPUT);    
  pinMode(F1, INPUT);    
  pinMode(G1, INPUT);    
  pinMode(An1, INPUT);
  pinMode(Bn1, INPUT);
  pinMode(C2, INPUT);
  pinMode(Cs ,INPUT);
  pinMode(Ds, INPUT);
  pinMode(Fs, INPUT);
  pinMode(Gs, INPUT);
  pinMode(As, INPUT);
  pinMode(voiceSel, INPUT);
  pinMode(octShift, INPUT);
  
  Serial.begin(9600);   
  Serial.println("noses");


  byte note = 0; //The MIDI note value to be played
  byte resetMIDI = 4; //Tied to VS1053 Reset line

  

  //Setup soft serial for MIDI control
  mySerial.begin(31250);

  //Reset the VS1053
  pinMode(resetMIDI, OUTPUT);
  digitalWrite(resetMIDI, LOW);
  delay(100);
  digitalWrite(resetMIDI, HIGH);
  delay(100);
  int  instrument = 0x01;
  talkMIDI(0xB0, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127) 
  talkMIDI(0xB0, 0, 0x00); //Default bank GM1
  talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command
  //the instrument number will be in a switch tree once the selector switch is implemented

}



void loop()
{
 //int checknote(int key int previousState int map[]);
  //int lastState = LOW;
  //int state;
  int key[13] ={C1, D1, E1, F1, G1, An1, Bn1, C2, Cs, Ds, Fs, Gs, As };
  int kmap[13] ={24, 26, 28, 29, 31, 33, 35, 36, 25, 27, 30, 32, 34 };
  int kmapOffset = 0;
  int lastState[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int voiceOffset = 0;
  int octaveShift = 0;
  int voiceSelect = 0;
  int prevVoiceSelect = 0;
  int instrumentVec[5] = {0x01, 0x08, 0x0A, 0x39, 0x47};
  unsigned long voiceTimer = 0;
  unsigned long voiceTime = 0;
  int buttonPressDwell = 1000;
  unsigned long pianoResetCount = 300;
 while (1) {
   resetPiano++;
  // Serial.println(resetPiano);
   //check for voice button push and update the instrument if true and time has elapsed
 voiceOffset = digitalRead(voiceSel);
 voiceTime =  millis() - voiceTimer;  //update voice timer time
 if((voiceOffset==0)&& (voiceTime > buttonPressDwell)){  //run if the button is pushed and more than  dwell ms past since last push
   voiceSelect = (voiceSelect + 1) % 5;      //update the midi instrument or voice mod wrap around
   voiceTimer = millis();   //reset the voice timer
 }
 if(resetPiano > pianoResetCount) {
   voiceSelect = 0;
   Serial.println("reset to piano");
 }
 if(voiceSelect != prevVoiceSelect){
   talkMIDI(0xC0, instrumentVec[voiceSelect], 0); //Set instrument number. 0xC0 is a 1 data byte command
   prevVoiceSelect= voiceSelect;  //update previous voice select
 }
   
   //check for octave shift state. If true switch up an octave. if false switch down
 octaveShift = digitalRead(octShift);
 if (octaveShift == 1){
    kmapOffset = 12; //offset keymap by one octave if true
    }
 else{
     kmapOffset = 0;
    }
    
 //poll all keys and update
 for (int i = 0; i<13; i++){
   lastState[i] = checknote(key[i], lastState[i], kmap[i], kmapOffset);
   delay(10);
 }
  }

}

// functions
//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15

void resetThePianoCounter(void)  {
  resetPiano = 0;
}
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
    resetThePianoCounter();
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

//Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
void talkMIDI(byte cmd, byte data1, byte data2) {
  //digitalWrite(ledPin, HIGH);
  mySerial.write(cmd);
  mySerial.write(data1);

  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);

  //digitalWrite(ledPin, LOW);
}

//checks note status and decided what to do
/*returns state of key, inputs key IE C1, previousState and array map of key note map
this will work best if the map gets updated by an interrupt since it will react quicker to the map change*/
int checknote(int key, int previousState, int kmapp, int kmappOffset){
  int currentState;
  int action;
  currentState =digitalRead(key);
  currentState = int(currentState);
  previousState = int(previousState);
  action = currentState - previousState;
  kmapp = kmapp + kmappOffset;
  switch(action) {
    case 1:
    noteOn(0, kmapp, 60);
    break;
    case -1:
    noteOff(0, kmapp, 60);
    break;
    case 0:
    //do nothing
    break;
    default:
    //do nothing
    break;
  }
  return currentState;
    
}

 /*state = digitalRead(C1);   // read the input pin
  Serial.println(state);
  int note = 60;
  if (state == HIGH && lastState == LOW){
    noteOn(0, note, 60);
    Serial.println("noteon!");
  }
  else if (state == HIGH && lastState == HIGH){
  }
 else if (state == LOW &&lastState == HIGH) { 
      noteOff(0,note, 60);
      Serial.println("noteoff!");
    }
    else {
    }
    lastState = state;
    delay(40);
 */

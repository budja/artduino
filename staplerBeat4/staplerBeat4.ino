#include <SoftwareSerial.h>
#define USE_FAST_DIGITAL_IO
#include <Bounce.h>
#include <FlexiTimer2.h>

SoftwareSerial mySerial(2, 3); // RX, TX

const int potInput = A0;
const int adjustedPotRange = 860;
int potValue = 0;
int shortDelay = 0;
int longDelay = 0;
  byte note = 0;
  byte resetMIDI = 4;
volatile int beatSelector = 0; // determines which beat is used - changed by interrupt
volatile long lastPress = 0;
unsigned long shutoffTime = 0;
unsigned long shutoffMinutes = 1;
int lastLeverPosition = 0;

#define BEAT_SELECTOR_PIN 2

// Instantiate a Bounce object
Bounce debouncer = Bounce(); 

byte *current_beat = NULL;
volatile int current_delay;
volatile boolean currently_delaying;
volatile byte stepCount = 0;
const byte ON = 0;
const byte OFF = 1;
const byte DELAY = 2;
const byte STOP = 3;

void PlayTimer()
{
  if (current_beat == NULL) return;
  byte *curBeat = (byte *)&(current_beat[stepCount]);
  
  if (currently_delaying){
    current_delay--;
    if (current_delay > 0) {
      return;
    }
    else {
      currently_delaying = false;
      if (((int)curBeat[0]) == STOP) {
        current_beat = NULL;
        stepCount = 0;
        return;
      }
      stepCount += 3;
    }
  }
  while ((int)curBeat[0] < 3) {
    switch ((int)curBeat[0])
    {
      case ON:
        noteOn(0, curBeat[1], curBeat[2]);
        stepCount += 3;
        break;
      case OFF:
        noteOff(0, curBeat[1], curBeat[2]);
        stepCount += 3;
        break;
      case DELAY:
        currently_delaying = true;
        current_delay = curBeat[1];
        break;
      case STOP:
        currently_delaying = true;
        current_delay = potValue;
        break;
    }
  }
}

byte *beat1_data = {ON, 36, 60, DELAY, 20, 0, ON, 36, 60, DELAY, 50, 0, ON, 51, 60, DELAY, 20, 0, OFF, 51, 60, STOP, 0, 0};

void setup(void) 
{
  
  pinMode(potInput, INPUT);
  pinMode(BEAT_SELECTOR_PIN, INPUT_PULLUP);

    // After setting up the button, setup the Bounce instance :
  debouncer.attach(BEAT_SELECTOR_PIN);
  debouncer.interval(5); // interval in ms

  Serial.begin(9600);   
  Serial.print("sizeof(byte *) = ");
  Serial.println(sizeof(byte *), DEC);
  Serial.print("sizeof(byte) = ");
  Serial.println(sizeof(byte), DEC);
  shutoffTime = shutoffMinutes * 1000 * 20;  // 1 minutes in milliseconds


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
  //talkMIDI(0xB0, 0, 0x00); //Default bank GM1
  talkMIDI(0xB0, 0, 0x78); //Bank select drums
  talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command
  //the instrument number will be in a switch tree once the selector switch is implemented  

  FlexiTimer2::set(500, .001, PlayTimer); // call every 500 1ms "ticks"
  // FlexiTimer2::set(500, flash); // MsTimer2 style is also supported
  FlexiTimer2::start();
  current_beat = beat1_data;
  // setting up interrupts
  sei();                    // enable interrupts
  EIMSK |= (1 << INT0);     // pin 2
  EICRA |= (1 << ISC01);
}

int oldBeatSelectorButtonValue = 0;

void loop(void)
{
  
  potValue = analogRead(potInput);
  //Serial.println(potValue);
  potValue = (potValue * 2) - 900; // Usually in the range 470-900 so adjust to 40 - 900
  // talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command
  Serial.print("beatSelector is now ");
  Serial.println(beatSelector, DEC);
  switch (beatSelector) {
    case 1:
      //current_beat = (byte *)beat1_data;
      //beat1(potValue);
      break;
    case 2:
      beat2(potValue);
      break;
    case 3:
      moarCowbell(potValue);
      break;
    case 4:
      beat4(potValue);
      break;
    case 5:
      beat5(potValue);
      break;
    case 6:
      beat6(potValue);
      break;
    default:
      noBeats();
  }
  if (abs(lastLeverPosition - potValue) >= 5) {
    lastPress = millis();
  }
  if ((millis() - lastPress) > shutoffTime) {
    beatSelector = 0;
  }
  lastLeverPosition = potValue;
  //Serial.println(potValue);             // debug value
  //Serial.println(lastPress);
  //Serial.println(millis());
}

// functions
void noBeats(void) {
  delay(50);
}
void beat1(int potValue) {
<<<<<<< HEAD
  int stepCount = 0;
  while ((int)beat1_data[stepCount][0] < 3) {
    switch ((int)beat1_data[stepCount][0])
    {
      case 0:
        Serial.print("noteOn(0, ");
        Serial.print(beat1_data[stepCount][1], DEC);
        Serial.print(", ");
        Serial.print(beat1_data[stepCount][2], DEC);
        Serial.println(");");
        noteOn(0, beat1_data[stepCount][1], beat1_data[stepCount][2]);
        break;
      case 1:
        Serial.print("noteOff(0, ");
        Serial.print(beat1_data[stepCount][1], DEC);
        Serial.print(", ");
        Serial.print(beat1_data[stepCount][2], DEC);
        Serial.println(");");
        noteOff(0, beat1_data[stepCount][1], beat1_data[stepCount][2]);
        break;
      case 2:
        Serial.print("delay(");
        Serial.print(beat1_data[stepCount][1], DEC);
        Serial.println(");");
        delay(beat1_data[stepCount][1]);
        break;
      case 3:
        Serial.println("done");
        break;
    }
    stepCount++;
  }
  Serial.println("Exited loop");
#if 0    
=======
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
>>>>>>> origin/master
  noteOn(0,36,60);  // Bass Drum 1
  delay(shortDelay);
  noteOn(0,36,60);
  delay(longDelay);
  //noteOff(0,36,60);
  noteOn(0,51,60);  // Ride Cymbal 1
  delay(shortDelay);
  noteOff(0,51,60);
#endif
  delay(potValue);
}

void beat2(int potValue) {
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
  noteOn(0,44,60); // Pedal Hi-hat
  delay(shortDelay);
  noteOn(0,44,60);
  delay(longDelay);
  //noteOff(0,36,60);
  noteOn(0,45,60); // Low Tom
  delay(shortDelay);
  noteOff(0,45,60);
  delay(potValue);
}

void moarCowbell(int potValue) {
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  noteOn(0,56,60); // Cowbell
  delay(shortDelay);
  noteOff(0,56,60);
  delay(potValue);
}

void beat4(int potValue) {
  // A little more swing
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
  noteOn(0,45,60); // Open Hi-hat
  delay(longDelay);
  noteOff(0,45,60);
  delay(potValue);

  noteOn(0,44,60); // Pedal Hi-hat
  delay(longDelay);
  noteOff(0,45,60);
  delay(potValue / 2);
  noteOn(0,44,60); // Pedal Hi-hat
  delay(shortDelay);
  noteOff(0,45,60);
  delay(potValue);
}

void beat5(int potValue) {
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
  noteOn(0,69,60); // Cabasa
  delay(potValue);
  noteOff(0,69,60);
  delay(potValue);

  noteOn(0,70,60); // Maracas
  delay(longDelay);
  noteOff(0,70,60);
  delay(potValue / 2);
  noteOn(0,70,60);
  delay(shortDelay);
  noteOff(0,70,60);
  delay(potValue);
}

void beat6(int potValue) {
  // A little bit of everything
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
  noteOn(0,67,60); // High Agogo
  delay(longDelay);
  noteOff(0,67,60);
  delay(potValue);

  noteOn(0,68,60); // Low Agogo
  delay(35);
  noteOff(0,68,60);
  delay(potValue / 2);
  
  noteOn(0,68,60); // Low Agogo
  delay(shortDelay);
  noteOff(0,68,60);
  delay(potValue / 2);
  
  noteOn(0,58,60); // Vibra Slap
  delay(longDelay);
  noteOff(0,58,60);
  delay(potValue);
}

ISR(INT0_vect) {
<<<<<<< HEAD
  debouncer.update();
  // Get the updated value :
  int beatSelectorButtonValue = debouncer.read();
=======
  if (millis() - lastPress > 200) {
    beatSelector++;
  }
  lastPress = millis();
  //Serial.println(beatSelector);
  if (beatSelector == 7) {       // this num should be 1 greater than the highest beat function num
    beatSelector = 0;            // loop back to zero when no beats are left
  }
}
>>>>>>> origin/master

  if ((oldBeatSelectorButtonValue == 0) && (beatSelectorButtonValue == 1)) beatSelector = (beatSelector + 1) % 7;
  oldBeatSelectorButtonValue = beatSelectorButtonValue;
}

//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

//Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
void talkMIDI(byte cmd, byte data1, byte data2) {
  mySerial.write(cmd);
  mySerial.write(data1);

  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);
}


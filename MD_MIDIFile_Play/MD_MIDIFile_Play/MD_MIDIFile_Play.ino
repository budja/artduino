// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//	SD card interface - change SD_SELECT for SPI comms
//	3 LEDs (optional) - to display current status and beat. 
//						Change pin definitions for specific hardware setup - defined below.


#define	USE_MIDI	1

#if USE_MIDI // set up for direct MIDI serial output

#define	DEBUG(x)
#define	DEBUGX(x)
#define	SERIAL_RATE	31250

#else // don't use MIDI to allow printing debug statements

#define	DEBUG(x)	Serial.print(x)
#define	DEBUGX(x)	Serial.print(x, HEX)
#define	SERIAL_RATE	57600

#endif // USE_MIDI

#include <SoftwareSerial.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>

SoftwareSerial mySerial(5, 6); // RX, TX

// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  8

#define RESET_MIDI 4  // MIDI shield reset pin

#define	WAIT_DELAY	2000	// ms

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
char *tuneList[] = 
{
	"SIXTFOUR.MID",
	"DAYDREAM.MID",
	"SOMETHIN.MID",
	"CAROLINE.MID",
	"PEANUTS.MID",
	"TAKEFIVE.MID",
	"AULDLANG.MID",
	"HERECOME.MID",
};

SdFat	SD;
MD_MIDIFile SMF;
volatile unsigned long idleAfterMillis = 0, patternModeExpirationMillis = 0;
const int potInput = A0;
const int adjustedPotRange = 860;
volatile int beatSelector = 0; // determines which beat is used - changed by interrupt
volatile long lastPress = 0;
int shortDelay = 0;
int longDelay = 0;

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
  if( (cmd & 0xF0) <= 0xB0 || (cmd & 0xF0) == 0xE0)
    mySerial.write(data2);
}

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
        talkMIDI(pev->data[0] | pev->channel, pev->data[1], pev->data[2]);
#endif
  DEBUG("\nM T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
	DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nS T");
  DEBUG(pev->track);
  DEBUG(": Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
	DEBUG(' ');
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
	midi_event	ev;

	// All sound off
	// When All Sound Off is received all oscillators will turn off, and their volume
	// envelopes are set to zero as soon as possible.
	ev.size = 0;
	ev.data[ev.size++] = 0xb0;
	ev.data[ev.size++] = 120;
	ev.data[ev.size++] = 0;

	for (ev.channel = 0; ev.channel < 16; ev.channel++){
		midiCallback(&ev);
                talkMIDI(0xB0 | ev.channel, 0x07, 0); //0xB0 is channel message, set channel volume to zero 
        }
}

volatile int blinkRate = 1000;
unsigned long nextBlink;
boolean blinkState = false;
void setup(void)
{
  mySerial.begin(SERIAL_RATE);
  Serial.begin(57600);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  //Serial.print("\n[MidiFile Play List]");

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.print("\nSD init fail!");
    digitalWrite(7, HIGH);
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

    //Reset the VS1053
  pinMode(RESET_MIDI, OUTPUT);
  digitalWrite(RESET_MIDI, LOW);
  delay(100);
  digitalWrite(RESET_MIDI, HIGH);
  delay(100);

  nextBlink = millis() + blinkRate;
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(1, staplerActivity, CHANGE);
  attachInterrupt(0, patternChangeButtonPressed, CHANGE);
  idleAfterMillis = 0;
}

int nextSong = 0;
void loop(void)
{
  //Serial.print("idleAfterMillis = ");
  //Serial.println(idleAfterMillis, DEC);
  if (millis() > nextBlink) {
    nextBlink = millis() + blinkRate;
    blinkState = !blinkState;
    if (blinkState) digitalWrite(7, HIGH);
    else digitalWrite(7, LOW);
    Serial.print("\nBeat selector is currently: ");
    Serial.print(beatSelector);
  }
  if (millis() < patternModeExpirationMillis)
  {
    Serial.print("\nIn pattern mode");
    beat1(500);
    return;
  }
  
  if (millis() > idleAfterMillis) {
    digitalWrite(7, LOW);
    play(nextSong);
    staplerActivity();
    nextSong = (nextSong + 1) % 8;
    blinkRate = 1000;
  }
}

void play(int index)
{
  boolean isClosed = false;
  int err;
  SMF.setFilename(tuneList[index]);
  err = SMF.load();
  if (err != -1)
  {
    Serial.print("\nSMF load Error: ");
    Serial.print(err);
    digitalWrite(7, HIGH);
    while (true);
  }
  else
  {
    // play the file
    while (!SMF.isEOF())
    {
      SMF.getNextEvent();
      if (millis() < idleAfterMillis)
      {
        SMF.close();
        midiSilence();
        isClosed = true;
        break;
      }
    }
    if (!isClosed) {
      // done with this one
      SMF.close();
      midiSilence();
    }
  }
}

#if 1
void beat1(int potValue) {
  shortDelay = 10.0 * potValue / adjustedPotRange + 10;
  longDelay = 40.0 * potValue / adjustedPotRange + 10;
  noteOn(0,36,60);  // Bass Drum 1
  delay(shortDelay);
  noteOn(0,36,60);
  delay(longDelay);
  //noteOff(0,36,60);
  noteOn(0,51,60);  // Ride Cymbal 1
  delay(shortDelay);
  noteOff(0,51,60);
  delay(potValue);
}
#endif

void staplerActivity()
{
  idleAfterMillis = millis() + 30000;
  blinkRate = 500;
}

void patternChangeButtonPressed()
{
  patternModeExpirationMillis = millis() + 30000;
  staplerActivity();
  if (millis() - lastPress > 200) {
    beatSelector++;
  }
  beatSelector %= 7;
  blinkRate = 250;
}

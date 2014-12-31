// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//	SD card interface - change SD_SELECT for SPI comms
//	3 LEDs (optional) - to display current status and beat. 
//						Change pin definitions for specific hardware setup - defined below.


#include <SoftwareSerial.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>

SoftwareSerial mySerial(2, 3); // RX, TX
#define	USE_MIDI	1

#if USE_MIDI // set up for direct MIDI serial output

#define	DEBUG(x)
#define	DEBUGX(x)
#define	SERIAL_RATE	31250

#else // don't use MIDI to allow printing debug statements

#define	DEBUG(x)	Serial.print(x)
#define	DEBUGX(x)	Serial.print(x, HEX)

#endif // USE_MIDI

#define	SERIAL_RATE	57600


// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  8

// LED definitions for user indicators
#define	READY_LED		  7	// when finished
#define SMF_ERROR_LED	6	// SMF error
#define SD_ERROR_LED	5	// SD error
#define	BEAT_LED		  6	// toggles to the 'beat'

#define	WAIT_DELAY	2000	// ms

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

#define RESET_MIDI 4

SdFat	SD;
MD_MIDIFile SMF;

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

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
	if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
	{
		mySerial.write(pev->data[0] | pev->channel);
		mySerial.write(&pev->data[1], pev->size-1);
	}
	else
		mySerial.write(pev->data, pev->size);
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

	for (ev.channel = 0; ev.channel < 16; ev.channel++)
		midiCallback(&ev);
}

File root;
void setup(void)
{
  // Set up LED pins
  pinMode(READY_LED, OUTPUT);
  pinMode(SD_ERROR_LED, OUTPUT);
  pinMode(SMF_ERROR_LED, OUTPUT);

  Serial.begin(SERIAL_RATE);

//Setup soft serial for MIDI control
  mySerial.begin(31250);

  DEBUG("\n[MidiFile Play List]");

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    DEBUG("\nSD init fail!");
    digitalWrite(SD_ERROR_LED, HIGH);
    while (true) ;
  }
  root = SD.open("/");

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  digitalWrite(READY_LED, HIGH);
  
    //Reset the VS1053
  pinMode(RESET_MIDI, OUTPUT);
  digitalWrite(RESET_MIDI, LOW);
  delay(100);
  digitalWrite(RESET_MIDI, HIGH);
  delay(100);
  int  instrument = 0x01;
  talkMIDI(0xB0, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127) 

}

void tickMetronome(void)
// flash a LED to the beat
{
	static uint32_t	lastBeatTime = 0;
	static boolean	inBeat = false;
	uint16_t	beatTime;

	beatTime = 60000/SMF.getTempo();		// msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
	if (!inBeat)
	{
		if ((millis() - lastBeatTime) >= beatTime)
		{
			lastBeatTime = millis();
			digitalWrite(BEAT_LED, HIGH);
			inBeat = true;
		}
	}
	else
	{
		if ((millis() - lastBeatTime) >= 100)	// keep the flash on for 100ms only
		{
			digitalWrite(BEAT_LED, LOW);
			inBeat = false;
		}
	}

}

char *getNextMidiFileInDirectory(File dir) {
  char result[13];
   while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       return NULL;
     }
     if (entry.nameEndsWith(".MID")) {
       strncpy(result, entry.name(), 13);
       entry.close();
       return result;
     }
   }
}

boolean once = false;
void loop(void)
{
    int  err;
    char *nextMidiFile;
	
	  // reset LEDs
	  digitalWrite(READY_LED, LOW);
	  digitalWrite(SD_ERROR_LED, LOW);

	  // use the next file name and play it
          nextMidiFile = getNextMidiFileInDirectory(root);
    Serial.println("\nFile: ");
    Serial.println(nextMidiFile);
          if (nextMidiFile == NULL)
          {
            if (!once) Serial.println("No more MIDI files");
            while (true);
            //once = true;
            //return;
          }
	  SMF.setFilename(nextMidiFile);
	  err = SMF.load();
	  if (err != -1)
	  {
		DEBUG("\nSMF load Error ");
		DEBUG(err);
		digitalWrite(SMF_ERROR_LED, HIGH);
		delay(WAIT_DELAY);
	  }
	  else
	  {
		// play the file
		while (!SMF.isEOF())
		{
			if (SMF.getNextEvent())
			tickMetronome();
		}

		// done with this one
		SMF.close();
		midiSilence();

		// signal finish LED with a dignified pause
		digitalWrite(READY_LED, HIGH);
		delay(WAIT_DELAY);
	  }
}

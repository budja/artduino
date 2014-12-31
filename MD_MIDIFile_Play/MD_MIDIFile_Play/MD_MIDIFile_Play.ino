#define JUKEBOX_VOLUME_DIVISOR 4
#define SECONDS_BETWEEN_JUKEBOX_SONGS 300
#define BEATBOX_ACTIVE_SECONDS 30

#define	MIDI_SERIAL_RATE	31250

#include <SoftwareSerial.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>

SoftwareSerial midiSerial(5, 6); // RX, TX

// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Sparkfun SD shield uses pin 8
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  8

#define RESET_MIDI 4  // MIDI shield reset pin

#define	WAIT_DELAY	2000	// ms

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
prog_char  killing[] PROGMEM = "KILLING.MID";
prog_char sixtfour[] PROGMEM = "SIXTFOUR.MID";
prog_char daydream[] PROGMEM = "DAYDREAM.MID";
prog_char somethin[] PROGMEM = "SOMETHIN.MID";
prog_char caroline[] PROGMEM = "CAROLINE.MID";
prog_char  peanuts[] PROGMEM = "PEANUTS.MID";
prog_char takefive[] PROGMEM = "TAKEFIVE.MID";
prog_char auldlang[] PROGMEM = "AULDLANG.MID";
prog_char herecome[] PROGMEM = "HERECOME.MID";

PROGMEM const char *tuneList[] = {
  somethin,
  sixtfour,
  killing,
  daydream,
  caroline,
  peanuts,
  takefive,
  auldlang,
  herecome
};

SdFat	SD;
MD_MIDIFile SMF;
volatile unsigned long idleAfterMillis = 0, beatboxModeExpirationMillis = 0;
const int beatboxRatePot = A0;
const int adjustedPotRange = 860;
int beatboxRate = 500;

volatile int beatSelector = 0; // determines which beat is used - changed by interrupt
volatile long lastPress = 0;
int shortDelay = 0;
int longDelay = 0;
String elapsedTimeString;

#define QUIET_MODE 0
#define BEATBOX_MODE 1
#define JUKEBOX_MODE 2

int oldMode = -1;
volatile int currentMode = QUIET_MODE;
int nextSongIndex = 0;

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
  
  if (((cmd & 0xF0) == 0xB0) && (data1 == 0x07) && (currentMode == JUKEBOX_MODE)) {
    Serial.print(F("\nScaling volume on channel: "));
    Serial.print(cmd & 0x0F, DEC);
    Serial.print(F(" from: "));
    Serial.print(data2, DEC);
    data2 /= JUKEBOX_VOLUME_DIVISOR;
    Serial.print(F(" to: "));
    Serial.print(data2, DEC);
  }
  midiSerial.write(cmd);
  midiSerial.write(data1);

  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  if ((cmd & 0xF0) <= 0xB0 || (cmd & 0xF0) == 0xE0)
    midiSerial.write(data2);
}

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  talkMIDI(pev->data[0] | pev->channel, pev->data[1], pev->data[2]);
}

// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
void sysexCallback(sysex_event *pev) {}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  for (byte channel = 0; channel < 16; channel++){
    talkMIDI(0xB0 | channel, 0x07, 0); //Channel volume to zero
    talkMIDI(0xB0 | channel, 120, 0); //All sound off
    talkMIDI(0xB0 | channel, 121, 0); //Reset All Controllers
  }
}

void midiReset(void)
{
  //Reset the VS1053
  pinMode(RESET_MIDI, OUTPUT);
  digitalWrite(RESET_MIDI, LOW);
  delay(100);
  digitalWrite(RESET_MIDI, HIGH);
  delay(100);
}

void setup(void)
{
  midiSerial.begin(MIDI_SERIAL_RATE);
  Serial.begin(57600);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  //Serial.print("\n[MidiFile Play List]");

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.print(F("\nSD init fail!"));
    digitalWrite(7, HIGH);
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  midiReset();

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(1, staplerActivity, CHANGE);
  attachInterrupt(0, patternChangeButtonPressed, CHANGE);
  staplerActivity();
  beatboxModeExpirationMillis = 0;
  currentMode = QUIET_MODE;
}

int oldBeatSelector;
void loop(void)
{
  switch (currentMode)
  {
    case QUIET_MODE:
      if (oldMode != currentMode) {
        Serial.print(F("\nEntering quiet mode. Uptime: "));
        formatElapsedTime(0);
        Serial.print(elapsedTimeString);
        midiSilence();
        oldMode = currentMode;
      }
      if (millis() > idleAfterMillis) currentMode = JUKEBOX_MODE;
      break;
    case BEATBOX_MODE:
      if (oldMode != currentMode) {
        Serial.print(F("\nEntering beatbox mode. Uptime: "));
        formatElapsedTime(0);
        Serial.print(elapsedTimeString);
        midiSilence();
        talkMIDI(0xB0, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127) 
        //talkMIDI(0xB0, 0, 0x00); //Default bank GM1
        talkMIDI(0xB0, 0, 0x78); //Bank select drums
        talkMIDI(0xC0, 1, 0); //Set instrument number. 0xC0 is a 1 data byte command
        oldMode = currentMode;
      }
      if (oldBeatSelector != beatSelector) {
        Serial.print(F("\nBeat selector changed to: "));
        Serial.print(beatSelector);
        Serial.print(F(" at: "));
        formatElapsedTime(0);
        Serial.print(elapsedTimeString);
        oldBeatSelector = beatSelector;
      }
      beatboxRate = analogRead(beatboxRatePot);
      beatboxRate = (beatboxRate * 2) - 900;
      switch (beatSelector) {
        case 1:
          beat1(beatboxRate);
          break;
        case 2:
          beat2(beatboxRate);
          break;
        case 3:
          moarCowbell(beatboxRate);
          break;
        case 4:
          beat4(beatboxRate);
          break;
        case 5:
          beat5(beatboxRate);
          break;
        case 6:
          beat6(beatboxRate);
          break;
        default:
          break;
      }
      if (millis() > beatboxModeExpirationMillis) currentMode = QUIET_MODE;
      break;
    case JUKEBOX_MODE:
      if (oldMode != currentMode) {
        Serial.print(F("\nEntering jukebox mode. Uptime: "));
        formatElapsedTime(0);
        Serial.print(elapsedTimeString);
        midiReset();
        oldMode = currentMode;
      }
      digitalWrite(7, LOW);  // turn off the LED
      play(nextSongIndex);
      staplerActivity();
      nextSongIndex = (nextSongIndex + 1) % ARRAY_SIZE(tuneList);
      break;
  }
}

void play(int index)
{
  char curSongName[16];
  unsigned long startMillis = millis();
  strcpy_P(curSongName, (char*)pgm_read_word(&(tuneList[index])));
  Serial.print(F("\nStarting to play file at index: "));
  Serial.print(index, DEC);
  Serial.print(F(" {"));
  Serial.print(curSongName);
  Serial.print(F("}"));
  SMF.setFilename(curSongName);
  int err = SMF.load();
  if (err != -1)
  {
    Serial.print(F("\nSMF load Error: "));
    Serial.print(err);
    digitalWrite(7, HIGH);
    while (true);
  }
  else
  {
    unsigned long startMillis = millis();
    // play the file
    while (!SMF.isEOF())
    {
      SMF.getNextEvent();
      if (millis() < idleAfterMillis)
      {
        SMF.close();
        midiSilence();
        Serial.print(F("\nCanceled playback of file at index: "));
        Serial.print(index, DEC);
        Serial.print(F(" {"));
        Serial.print(curSongName);
        Serial.print(F("} Elapsed time: "));
        formatElapsedTime(startMillis);
        Serial.print(elapsedTimeString);
        if (currentMode != BEATBOX_MODE) currentMode = QUIET_MODE;
        return;
      }
    }
    // done with this one
    SMF.close();
  }
  Serial.print(F("\nFinished playing file at index: "));
  Serial.print(index, DEC);
  Serial.print(F(" {"));
  Serial.print(curSongName);
  Serial.print(F("} Elapsed time: "));
  formatElapsedTime(startMillis);
  Serial.print(elapsedTimeString);
  currentMode = QUIET_MODE;
}

const unsigned long MILLIS_PER_DAY = 86400000;
const unsigned long MILLIS_PER_HOUR = 3600000;
const unsigned long MILLIS_PER_MINUTE = 60000;
const unsigned long MILLIS_PER_SECOND = 1000;
void formatElapsedTime(unsigned long startMillis)
{
  unsigned long elapsedMillis = millis() - startMillis;
  String daysString = "";
  String hoursString = "";
  String minutesString = "";
  String secondsString = "00.";
  String millisecondsString = "";
  int days = 0, hours = 0, minutes = 0, seconds = 0;
  if (elapsedMillis > MILLIS_PER_DAY) {
    days = elapsedMillis / MILLIS_PER_DAY;
    elapsedMillis -= days * MILLIS_PER_DAY;
    daysString = String(days, DEC) + ":";
    if (daysString.length() == 2) daysString = "0" + daysString;
  }
  if (elapsedMillis > MILLIS_PER_HOUR) {
    hours = elapsedMillis / MILLIS_PER_HOUR;
    elapsedMillis -= hours * MILLIS_PER_HOUR;
    hoursString = String(hours, DEC) + ":";
    if (hoursString.length() == 2) hoursString = "0" + hoursString;
  }
  if (elapsedMillis > MILLIS_PER_MINUTE) {
    minutes = elapsedMillis / MILLIS_PER_MINUTE;
    elapsedMillis -= minutes * MILLIS_PER_MINUTE;
    minutesString = String(minutes, DEC) + ":";
    if (minutesString.length() == 2) minutesString = "0" + minutesString;
  }
  if (elapsedMillis > MILLIS_PER_SECOND) {
    seconds = elapsedMillis / MILLIS_PER_SECOND;
    elapsedMillis -= seconds * MILLIS_PER_SECOND;
    secondsString = String(seconds, DEC) + ".";
    if (secondsString.length() == 2) secondsString = "0" + secondsString;
  }
  millisecondsString = String(elapsedMillis, DEC);
  if (millisecondsString.length() == 2) millisecondsString = "0" + millisecondsString;
  if (millisecondsString.length() == 1) millisecondsString = "00" + millisecondsString;

  elapsedTimeString = daysString + hoursString + minutesString + secondsString + millisecondsString;
}

void beat1(int beatboxRate) {
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  longDelay = 40.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,36,60);  // Bass Drum 1
  delay(shortDelay);
  noteOn(0,36,60);
  delay(longDelay);
  //noteOff(0,36,60);
  noteOn(0,51,60);  // Ride Cymbal 1
  delay(shortDelay);
  noteOff(0,51,60);
  delay(beatboxRate);
}

void beat2(int beatboxRate) {
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  longDelay = 40.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,44,60); // Pedal Hi-hat
  delay(shortDelay);
  noteOn(0,44,60);
  delay(longDelay);
  //noteOff(0,36,60);
  noteOn(0,45,60); // Low Tom
  delay(shortDelay);
  noteOff(0,45,60);
  delay(beatboxRate);
}

void moarCowbell(int beatboxRate) {
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,56,60); // Cowbell
  delay(shortDelay);
  noteOff(0,56,60);
  delay(beatboxRate);
}

void beat4(int beatboxRate) {
  // A little more swing
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  longDelay = 40.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,45,60); // Open Hi-hat
  delay(longDelay);
  noteOff(0,45,60);
  delay(beatboxRate);

  noteOn(0,44,60); // Pedal Hi-hat
  delay(longDelay);
  noteOff(0,45,60);
  delay(beatboxRate / 2);
  noteOn(0,44,60); // Pedal Hi-hat
  delay(shortDelay);
  noteOff(0,45,60);
  delay(beatboxRate);
}

void beat5(int beatboxRate) {
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  longDelay = 40.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,69,60); // Cabasa
  delay(beatboxRate);
  noteOff(0,69,60);
  delay(beatboxRate);

  noteOn(0,70,60); // Maracas
  delay(longDelay);
  noteOff(0,70,60);
  delay(beatboxRate / 2);
  noteOn(0,70,60);
  delay(shortDelay);
  noteOff(0,70,60);
  delay(beatboxRate);
}

void beat6(int beatboxRate) {
  // A little bit of everything
  shortDelay = 10.0 * beatboxRate / adjustedPotRange + 10;
  longDelay = 40.0 * beatboxRate / adjustedPotRange + 10;
  noteOn(0,67,60); // High Agogo
  delay(longDelay);
  noteOff(0,67,60);
  delay(beatboxRate);

  noteOn(0,68,60); // Low Agogo
  delay(35);
  noteOff(0,68,60);
  delay(beatboxRate / 2);
  
  noteOn(0,68,60); // Low Agogo
  delay(shortDelay);
  noteOff(0,68,60);
  delay(beatboxRate / 2);
  
  noteOn(0,58,60); // Vibra Slap
  delay(longDelay);
  noteOff(0,58,60);
  delay(beatboxRate);
}

void staplerActivity()
{
  idleAfterMillis = millis() + (SECONDS_BETWEEN_JUKEBOX_SONGS * 1000);
}

void patternChangeButtonPressed()
{
  beatboxModeExpirationMillis = millis() + (BEATBOX_ACTIVE_SECONDS * 1000);
  staplerActivity();
  if (millis() - lastPress > 200) {
    beatSelector++;
  }
  beatSelector %= 7;
  currentMode = BEATBOX_MODE;
}

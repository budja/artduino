/*
fft_codec.pde
guest openmusiclabs.com 8.18.12
example sketch for running an fft on data collected
with the codecshield.  this will send out 128 bins of
data over the serial port at 115.2kb.  there is a
pure data patch for visualizing the data.

note: be sure to download the latest AudioCodec library
if yours is older than 8.16.12.  there were modifications
made that allow for code outside of the interrupt.
*/

#define SAMPLE_RATE 22 // 22050 Hz
#define ADCS 2 // both ADCs are being used
#define OCTAVE 1 // use the octave output function
#define FFT_N 128 // set to 128 point fft
//#define REORDER 1
//#define WINDOW 1
//#define OCT_NORM 0

#define FORCE_SOFTWARE_SPI
#define FORCE_SOFTWARE_PIN
// include necessary libraries
#include <FastLED.h>
#include <FFT.h>
#include <Wire.h>
#include <SPI.h>
#include <AudioCodec.h>
#include <Bounce.h>
#include <avr/pgmspace.h>
//#include <charset5x5.h>

uint8_t sine[256] = {
  #include <sine256.inc>
};

#define DATA_PIN 6
#define CLOCK_PIN 7
#define BUTTON_PIN A5
#define LED_PIN 13

// How many leds in your strip?
#define NUM_LEDS 50
#define NUM_SPECTRUM_STRIP_LEDS 20

// BEGIN USER SETTINGS
// Set the brightness of the "inactive" LEDs in the sound-reactive scenes (valid values are 0x00 (off) to 0xFF (full brightness, not recommended))
// Default value is 0x20, which is 1/8 of full brightness as set by the MOD0 pot on the codec board (linear brightness scale)
// If this is set to 0x00 (or a very low value) then the LEDs will be OFF when there is no input to the codec board, which will make the
// piano invisible in the dark.
#define INACTIVE_LED_BRIGHTNESS 0x20
//
// END USER SETTINGS

enum displaymode {
  sound,
  snake,
  sound2,
  pong,
};

// Define the array of leds
CRGB leds[NUM_LEDS];
uint8_t scale[NUM_LEDS];
CRGB spectrum_strip_leds[NUM_SPECTRUM_STRIP_LEDS];
uint8_t hue_source[NUM_SPECTRUM_STRIP_LEDS];

// LEDs should be laid out as follows
// Facing the back of the piano, LED 0 should be at the lower left and LED 49 
// at the upper right. Note that LEDs 25-49 are in the same orientation as LEDs 0-24
// In other words, the alternating up/down wiring pattern starts over with LED 25
//
//  4   5  14  15  24  29  30  39  40  49
//  3   6  13  16  23  28  31  38  41  48
//  2   7  12  17  22  27  32  37  42  47
//  1   8  11  18  21  26  33  36  43  46
//  0   9  10  19  20  25  34  35  44  45
//
// For the sides of the piano, use a 5x5 grid, with LED 0 at the lower LEFT corner on one side
// and LED 25 on the lower RIGHT corner on the other side.  Doing it this way will make the display
// more symmetrical but it honestly doesn't matter very much

uint8_t fives[][5] = {// First effect group starts with the middle stripes, grouped vertically upward 
                      // out toward the edges
                      {20, 21, 22, 23, 24}, //{4, 15, 24, 35, 44},
                      {25, 26, 27, 28, 29}, //{5, 14, 25, 34, 45},
                      {19, 18, 17, 16, 15}, //{3, 16, 23, 36, 43},
                      {34, 33, 32, 31, 30}, //{6, 13, 26, 33, 46},
                      {10, 11, 12, 13, 14}, //{2, 17, 22, 37, 42},
                      {35, 36, 37, 38, 39}, //{7, 12, 27, 32, 47},
                      { 9,  8,  7,  6,  5}, //{1, 18, 21, 38, 41},
                      {44, 43, 42, 41, 40}, //{8, 11, 28, 31, 48},
                      { 0,  1,  2,  3,  4}, //{0, 19, 20, 39, 40},
                      {45, 46, 47, 48, 49}, //{9, 10, 29, 30, 49},
                      // Second effect group starts with the bottom stripes, grouped horizontally
                      // outward from the center, then moving up to the top of the grid
                      {20, 19, 10,  9,  0}, //{ 4,  3,  2,  1,  0},
                      {25, 34, 35, 44, 45}, //{ 5,  6,  7,  8,  9},
                      {21, 18, 11,  8,  1}, //{14, 13, 12, 11, 10},
                      {26, 33, 36, 43, 46}, //{15, 16, 17, 18, 19},
                      {22, 17, 12,  7,  2}, //{24, 23, 22, 21, 20},
                      {27, 32, 37, 42, 47}, //{25, 26, 27, 28, 29},
                      {23, 16, 13,  6,  3}, //{34, 33, 32, 31, 30},
                      {28, 31, 38, 41, 48}, //{35, 36, 37, 38, 39},
                      {24, 15, 14,  5,  4}, //{44, 43, 42, 41, 40},
                      {29, 30, 39, 40, 49}, //{45, 46, 47, 48, 49},
                      // Third effect group has the stripes grouped vertically from the bottom
                      // with 0-24 and 25-49 arranged identically
                      { 0,  1,  2,  3,  4},
                      {25, 26, 27, 28, 29},
                      { 9,  8,  7,  6,  5},
                      {34, 33, 32, 31, 30},
                      {10, 11, 12, 13, 14},
                      {35, 36, 37, 38, 39},
                      {19, 18, 17, 16, 15},
                      {44, 43, 42, 41, 40},
                      {20, 21, 22, 23, 24},
                      {45, 46, 47, 48, 49}};
                      
uint8_t tens[5][10] = {{0,  9, 10, 19, 20, 25, 34, 35, 44, 45},  //{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9},
                       {1,  8, 11, 18, 21, 26, 33, 36, 43, 46},  //{19, 18, 17, 16, 15, 14, 13, 12, 11, 10},
                       {2,  7, 12, 17, 22, 27, 32, 37, 42, 47},  //{20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
                       {3,  6, 13, 16, 23, 28, 31, 38, 41, 48},  //{39, 38, 37, 36, 35, 34, 33, 32, 31, 30},
                       {4,  5, 14, 15, 24, 29, 30, 39, 40, 49}}; //{40, 41, 42, 43, 44, 45, 46, 47, 48, 49}};

CLEDGroup LEDGroup(leds);

// create data variables for audio transfer
int left_in = 0x0000;
int left_out = 0x0000;
int right_in = 0x0000;
int right_out = 0x0000;
// create variables for ADC results
// it only has positive values -> unsigned
unsigned int mod0_value = 0;
unsigned int old_mod0_value = 0;
unsigned int mod1_value = 0;
unsigned int old_mod1_value = 0;
volatile boolean ten_ms_tick;

unsigned int count = 0;
volatile byte fft_flag = 1;

volatile displaymode display_mode;

// Instantiate a Bounce object
Bounce debouncer = Bounce(); 

void setup_timer2()
{
  cli();         // disable global interrupts
  TCCR2A = 0;    // set entire TCCR1A register to 0
  TCCR2B = 0;     // same for TCCR1B
  // set compare match register to desired timer count:
  OCR2A = 200;    // Should be 100us @ 16MHz clock rate
  // turn on CTC mode:
  TCCR2B |= (1 << WGM12);
  // Set CS10 for no prescaling
  TCCR2B |= (1 << CS11);
  // enable timer compare interrupt:
  TIMSK2 |= (1 << OCIE2A);
  sei();          // enable global interrupts}
}

uint8_t state = 0;

void setup() {
  Serial.begin(115200); // use serial port
  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
  FastLED.clear(true);
  short msk = TIMSK0;  // Save the timer 0 interrupt mask, because it's disabled by AudioCodec_init();
  AudioCodec_init();
  //setup_adc_interrupt();
  TIMSK0 = msk;        // Resore the timer interrupt mask
  setup_timer2();
  display_mode = sound2;
 
  // Setup the button
  pinMode(BUTTON_PIN,INPUT);
  // Activate internal pull-up
  digitalWrite(BUTTON_PIN,HIGH);
  
  // After setting up the button, setup debouncer
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5);
  
  //Setup the LED
  pinMode(LED_PIN,OUTPUT);
  
  change_state();
}

short ten_seconds;
byte hundred_millisecond_counter;
byte one_second_counter;
byte ten_second_counter;
byte led_count;
uint8_t rainbow_hue;
uint8_t five_base_index = 20;
uint8_t wipe_index;
uint8_t state_after_wipe;

int pong_cur_index = 0;
int pong_prev_index = -1;
int pong_direction = 1;
uint8_t pong_hue = 0;
void ten_millisecond_tick()
{
  uint8_t i, j;
  // Stuff that wants to happen at 100Hz goes here
  int divisor;
  switch (display_mode){
    case sound:
      divisor = 31 - (mod1_value >> 11);
      fill_rainbow(spectrum_strip_leds, NUM_SPECTRUM_STRIP_LEDS, rainbow_hue, 23);
      rainbow_hue++;
      LEDGroup.fill_from_source(tens[0], 10, &spectrum_strip_leds[0], max((fft_oct_out[1] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(tens[1], 10, &spectrum_strip_leds[1], max((fft_oct_out[2] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(tens[2], 10, &spectrum_strip_leds[2], max((fft_oct_out[3] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(tens[3], 10, &spectrum_strip_leds[3], max((fft_oct_out[4] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(tens[4], 10, &spectrum_strip_leds[4], max((fft_oct_out[5] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      break;
    case sound2:
      divisor = 41 - (mod1_value >> 11);
      fill_rainbow(spectrum_strip_leds, NUM_SPECTRUM_STRIP_LEDS, rainbow_hue, 23);
      rainbow_hue++;
      LEDGroup.fill_from_source(fives[five_base_index + 0], 5, &spectrum_strip_leds[0], max((fft_oct_out[1] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 1], 5, &spectrum_strip_leds[0], max((fft_oct_out[1] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 2], 5, &spectrum_strip_leds[1], max((fft_oct_out[2] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 3], 5, &spectrum_strip_leds[1], max((fft_oct_out[2] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 4], 5, &spectrum_strip_leds[2], max((fft_oct_out[3] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 5], 5, &spectrum_strip_leds[2], max((fft_oct_out[3] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 6], 5, &spectrum_strip_leds[3], max((fft_oct_out[4] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 7], 5, &spectrum_strip_leds[3], max((fft_oct_out[4] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 8], 5, &spectrum_strip_leds[4], max((fft_oct_out[5] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      LEDGroup.fill_from_source(fives[five_base_index + 9], 5, &spectrum_strip_leds[4], max((fft_oct_out[5] / divisor) - 2, 0), INACTIVE_LED_BRIGHTNESS);
      break;
    case snake:
      for (i = 0; i < 10; i++){
        for (j = 0; j < 5; j++){
          uint8_t index = j;
          index *= 5;
          index += rainbow_hue;
          leds[fives[five_base_index + i][j]].setHSV(sine[(uint8_t)(index + i * 13)], 0xFF, sine[(uint8_t)(index * 5 + i * 20)]);
        }
      }
      rainbow_hue++;
      break;
  }
  
  hundred_millisecond_counter++;
  if (hundred_millisecond_counter > 10){
    hundred_millisecond_counter = 0;
    one_second_counter++;
    
    // Stuff that wants to happen at 10Hz goes here
    switch (display_mode){
      case sound:
        break;
      case snake:
        break;
      case pong:
        leds[pong_cur_index].setHSV(pong_hue++, 0xFF, 0xFF);
        if (pong_prev_index >= 0){
          leds[pong_prev_index] = CRGB::Black;
        }
        pong_prev_index = pong_cur_index;
        pong_cur_index += pong_direction;
        if (pong_cur_index >= NUM_LEDS){
          // If we've hit the last LED
          pong_cur_index = NUM_LEDS - 1;
          pong_prev_index = -1;
          pong_direction = -1;
        } else if (pong_cur_index < 0) {
          // If we've passed the first LED
          pong_cur_index = 0;
          pong_prev_index = -1;
          pong_direction = 1;
        }
        break;
      default:
        break;
    }
  }
  if (one_second_counter > 10){
    one_second_counter = 0;
    ten_second_counter++;

    // Stuff that wants to happen at 1Hz goes here
    switch (display_mode){
      case sound:
        break;
      case snake:
        break;
      default:
        break;
    }
  }
  if (ten_second_counter > 10){
    ten_second_counter = 0;
    // Stuff that wants to happen at 0.11Hz goes here
    change_state();
  }
  FastLED.show();
}

int oldButtonValue = HIGH;
void change_state()
{
  state++;
  state %= 7;
  switch (state)
  {
    case 0:
      Serial.println("Mode: sound2  index: 0");
      display_mode = sound2;
      five_base_index = 0;
      break;
    case 1:
      Serial.println("Mode: snake  index: 0");
      display_mode = snake;
      five_base_index = 0;
      break;
    case 2:
      Serial.println("Mode: sound2  index: 20");
      display_mode = sound2;
      five_base_index = 20;
      break;
    case 3:
      Serial.println("Mode: snake  index: 10");
      display_mode = snake;
      five_base_index = 10;
      break;
    case 4:
      Serial.println("Mode: sound2  index: 10");
      display_mode = sound2;
      five_base_index = 10;
      break;
    case 5:
      Serial.println("Mode: sound");
      display_mode = sound;
      break;
    case 6:
      Serial.println("Mode: snake  index: 20");
      display_mode = snake;
      five_base_index = 20;
      break;
    default:
      state = 0;
      break;
  }
}

void loop() {
  if (ten_ms_tick)
  {
    if (fft_flag == 0)
    {
      fft_window(); // window the data
      fft_reorder(); // reorder for fft input
      fft_run(); // process fft
      fft_mag_octave(); // take output of fft
      //Serial.write(255); // send out a start byte
      //Serial.write(fft_log_out, FFT_N/2); // send out data bytes
      //rainbowString(23);
      if (mod1_value != old_mod1_value){
        Serial.print("Current mod1_value: ");
        Serial.println(mod1_value >> 11, DEC);
        old_mod1_value = mod1_value;
      }
      //hsv2rgb_raw(CHSV(fft_oct_out[2], 255, 255), hsvColor);
      //fill_solid(leds, NUM_LEDS, hsvColor);
      fft_flag = 1;
    }
    ten_ms_tick = false;
    ten_millisecond_tick();
  }
  // Update the debouncer
  debouncer.update();
   
  // Get the update value
  int buttonValue = debouncer.read();
   
  // Turn on or off the LED
  if ( buttonValue == LOW && buttonValue != oldButtonValue) {
    change_state();
  }
  oldButtonValue = buttonValue;
  //delay(20);
  FastLED.setBrightness(mod0_value >> 8);
}

// timer1 interrupt routine - data collected here
ISR(TIMER1_COMPA_vect) { // store registers (NAKED removed)

  // &'s are necessary on data_in variables
  AudioCodec_data(&left_in, &right_in, left_out, right_out);
  left_out = left_in; // pass audio through
  right_out = right_in;
  // get ADC values
  // & is required before adc variables
  AudioCodec_ADC(&mod0_value, &mod1_value);

  if (fft_flag) { // check if the fft is ready for more data
    fft_input[count] = left_in + right_in; // put real data into even bins
    fft_input[count + 1] = 0; // put zeros in odd bins
    count += 2; // increment to next bin
    if (count >= FFT_N*2) { // check if all bins are full
      fft_flag = 0; // tell the fft to start running
      count = 0; // reset the bin counter
    }
  }
}

volatile byte ten_us_ticks = 0;
// timer2 interrupt routine - drives LED display update, native timer2 interval is expected to be 100us
ISR(TIMER2_COMPA_vect) { // store registers (NAKED removed)
  if (++ten_us_ticks >= 100){
    ten_us_ticks = 0;
    ten_ms_tick = true;
  }
}


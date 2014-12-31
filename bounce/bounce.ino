
/* 
 DESCRIPTION
 ====================
 Simple example of the Bounce library that switches the debug LED when a button is pressed.
 */
// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce-Arduino-Wiring

#include <avr/interrupt.h>

#define USE_FAST_DIGITAL_IO
#include <FastDigitalIO.h>
#include <Bounce.h>

#define BUTTON_PIN 2
#define LED_PIN 13

// Instantiate a Bounce object
Bounce debouncer = Bounce(); 

volatile int bounceCount = 0;
int oldBounceCount = 0;
int oldValue = 0;
void setup() {
  Serial.begin(9600);   
  Serial.println("Starting debounce test. Enabling interrupts.");
  // setting up interrupts
  sei();                    // enable interrupts
  EIMSK |= (1 << INT0);     // pin 2
  EICRA |= (1 << ISC01);

  // Setup the button with an internal pull-up :
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // After setting up the button, setup the Bounce instance :
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5); // interval in ms

  //Setup the LED :
  pinMode(LED_PIN,OUTPUT);

}

void loop() {
  // Update the Bounce instance :
  debouncer.update();

  // Get the updated value :
  int value = debouncer.read();

  if (oldBounceCount != bounceCount){
    Serial.print("Bounce count is now: ");
    Serial.println(bounceCount, DEC);
    oldBounceCount = bounceCount;
  }
  
  if (oldValue != value){
    Serial.print("Value is now: ");
    Serial.println(value, DEC);
    oldValue = value;
  }
  // Turn on or off the LED as determined by the state :
  if ( value == LOW ) {
    digitalWrite(LED_PIN, HIGH );
  } 
  else {
    digitalWrite(LED_PIN, LOW );
  }

}

ISR(INT0_vect) {
  bounceCount++;
}




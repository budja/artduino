#ifndef Bounce_h
#define Bounce_h

#include <inttypes.h>

class Bounce
{

public:
  // Create an instance of the bounce library
  Bounce(); 
  // Attach to a pin (and also sets initial state)
  void attach(int pin);
	// Sets the debounce interval
  void interval(uint16_t interval_millis);
	// Updates the pin
	// Returns 1 if the state changed
	// Returns 0 if the state did not change
  bool update(); 
	// Returns the updated pin state
  bool read();

  
protected:
  unsigned long previous_millis;
  uint16_t interval_millis;
  uint8_t state;
  uint8_t pin;
};

#endif
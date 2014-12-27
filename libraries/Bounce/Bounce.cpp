#include "arduino.h"
#include "Bounce.h"

#define DEBOUNCED_STATE 0
#define UNSTABLE_STATE  1
#define STATE_CHANGED   3

Bounce::Bounce()
: previous_millis(0)
, interval_millis(10)
, state(0)
, pin(0)
{}

void Bounce::attach(int pin) {
 this->pin = pin;
 bool read = digitalRead(pin);
 state = 0;
 if (digitalRead(pin)) {
   state = _BV(DEBOUNCED_STATE) | _BV(UNSTABLE_STATE);
 }
 previous_millis = millis();
}

void Bounce::interval(uint16_t interval_millis)
{
  this->interval_millis = interval_millis;
}

bool Bounce::update()
{
	bool currentState = digitalRead(pin);
    state &= ~_BV(STATE_CHANGED);

	if ( currentState != (bool)(state & _BV(UNSTABLE_STATE)) ) {
		previous_millis = millis();
		state ^= _BV(UNSTABLE_STATE);
	} else 	if ( millis() - previous_millis >= interval_millis ) {
		if ((bool)(state & _BV(DEBOUNCED_STATE)) != currentState) {
			previous_millis = millis();
			state ^= _BV(DEBOUNCED_STATE);
			state |= _BV(STATE_CHANGED);
		}
	}

	return state & _BV(STATE_CHANGED);
}

bool Bounce::read()
{
	return state & _BV(DEBOUNCED_STATE);
}
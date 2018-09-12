#ifndef _RELAYBOARD_H_
#define _RELAYBOARD_H_

#include "mbed.h"
#include "PinNames.h"

class RelayBoard {
public:
  RelayBoard();

  void setOutput(int outputIdx, int value);
  void onTick();

private:
  DigitalOut outputs[8];
  DigitalOut chipSelect[4];

  //Each bit represent the output state of the corresponding relay
  uint8_t states[4];
  //Set if the corresponding state was updated since the last output update
  bool statesDirty[4];
};

#endif
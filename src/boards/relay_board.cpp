#include "relay_board.h"
#include "bitLabCore\src\utils.h"

#define N_STATES 4

RelayBoard::RelayBoard(): outputs({(PC_0), (PC_1), (PB_0), (PA_4), (PA_1), (PA_0), (PC_3), (PC_2)}),
                          chipSelect({(D11),(D12),(D14),(PC_14)}) {
  //Initialize as all dirty and with all outputs at 0
  //They will be all updated on the next call to updateOutputs
  for(int i=0; i<N_STATES; i++) {
    states[i] = 0;
    statesDirty[i] = true;
    chipSelect[i] = 0; //Reset chip select
  }
}

void RelayBoard::setOutput(int outputIdx, int value) {
  if (outputIdx < 0 || outputIdx >= 8*N_STATES) {
    //Undefined output!
    return;
  }
  int stateIdx = outputIdx / 8;
  int stateBit = outputIdx % 8;
  uint8_t bitMask = 1 << stateBit;

  //Critical section
  __disable_irq();
  if (value > 0) {
    states[stateIdx] = states[stateIdx] | bitMask;
  } else {
    states[stateIdx] = states[stateIdx] & (~bitMask);
  }
  statesDirty[stateIdx] = true;
  __enable_irq();
}

void RelayBoard::onTick() {
  for(int i=0; i<N_STATES; i++) {
    //For each dirty state
    if (statesDirty[i]) {
      statesDirty[i] = false;
      //Update the outputs
      uint8_t state = states[i];
      for(int bit=0; bit<8; bit++) {
        outputs[bit] = Utils::bitIsSet(state, bit);
      }
      //And give a chip select pulse
      chipSelect[i] = 1;
      //Delay 4 nops: it's 11.9ns*4 = 47.6ns
      //The 74HC374 has a minimum clock width of 24ns at 4.5v, so it should suffice.
      Utils::nop();
      Utils::nop();
      Utils::nop();
      Utils::nop();
      chipSelect[i] = 0;
    }
  }
}
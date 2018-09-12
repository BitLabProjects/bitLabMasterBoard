#ifndef _TRIACBOARD_H_
#define _TRIACBOARD_H_

#include "mbed.h"
#include "PinNames.h"
#include "config.h"
#include "..\bitLabCore\src\utils.h"

class TriacBoard
{
public:
  TriacBoard();

  void setOutput(int outputIdx, int value, millisec startTime, millisec duration);
  void onTick(millisec time);
  bool getInput50HzIsStable() { return input50HzIsStable; }
  float getMeasured50HzFrequency()
  {
    return ((float)TICKS_PER_SECOND) / (lastZeroCrossDurationInTicks * 2.0f);
  }
  void debugPrintOutputs();

private:
  // show connection to 50Hz external signal (230Vac)
  DigitalOut led_heartbeat;
  DigitalOut outputs[ANALOGOUT_COUNT];

  InterruptIn main_crossover;

  struct OutputState
  {
    int value;
    int from;
    int to;
    millisec startTime;
    millisec duration;

    inline void reset()
    {
      value = 0;
      from = 0;
      to = 0;
      startTime = 0;
      duration = 0;
    }
    inline void set(int newTo, millisec newStartTime, millisec newDuration)
    {
      from = value;
      to = newTo;
      startTime = newStartTime;
      duration = newDuration;
    }
    inline void update(int time)
    {
      if (duration <= 0)
      {
        value = startTime > time ? from : to;
      }
      else
      {
        int delta = to - from;
        float t = (((float)time) - startTime) / duration;
        t = Utils::max(0, Utils::min(t, 1)); //Don't go above 1, so the result is between 'from' and 'to' values
        value = from + (int)(delta * t);
      }
    }
  };
  // percent set for each output
  OutputState states[ANALOGOUT_COUNT];

  bool input50HzIsStable;
  int zeroCrossesCount;
  int ticksSinceZeroCross;
  int lastZeroCrossDurationInTicks;

  void main_crossover_rise();
};

#endif
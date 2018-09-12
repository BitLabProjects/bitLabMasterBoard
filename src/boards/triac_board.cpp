#include "triac_board.h"

#include "..\bitLabCore\src\os\os.h"

TriacBoard::TriacBoard() : led_heartbeat(LED2),
                           outputs({(D2), (D3), (D4), (D5), (D6), (D7), (D8), (D9)}),
                           main_crossover(D10)
{
  input50HzIsStable = 0;
  ticksSinceZeroCross = 0;
  lastZeroCrossDurationInTicks = 0;
  zeroCrossesCount = 0;

  for (int i = 0; i < ANALOGOUT_COUNT; i++) {
    states[i].reset(); 
  }

  if (!SIMULATE_VAC)
    main_crossover.rise(callback(this, &TriacBoard::main_crossover_rise));
}

void TriacBoard::setOutput(int idx, int value, millisec startTime, millisec duration)
{
  __disable_irq();
  states[idx].set(value, startTime, duration);
  __enable_irq();
}

void TriacBoard::onTick(millisec time)
{
  ticksSinceZeroCross += 1;

  //Somehow using a ticker for simulation gives wrong timings...
  if (SIMULATE_VAC) {
    if (ticksSinceZeroCross == NOMINAL_100HZ_TICKS_PER_RISE) {
      main_crossover_rise();
    }
  }

  if (!input50HzIsStable) {
    //No stable input, all outputs to zero
    for (int out = 0; out < ANALOGOUT_COUNT; out++)
    {
      outputs[out] = 0;
    }
    return;
  }

  // set/reset each out based on percent
  for (int out = 0; out < ANALOGOUT_COUNT; out++)
  {
    states[out].update(time);

    int valueToSet;
    int low_ticks = lastZeroCrossDurationInTicks * ((100.0 - (states[out].value)) / 100.0);

    if (SIMULATE_VAC)
    {
      if (ticksSinceZeroCross > low_ticks)
        valueToSet = 1;
      else
        valueToSet = 0;
    }
    else
    {
      // pulse for TRIAC activation
      if ((ticksSinceZeroCross < low_ticks) || (ticksSinceZeroCross > (low_ticks + GATE_TICKS)))
        valueToSet = 0;
      else
        valueToSet = 1;
    }

    outputs[out] = valueToSet;
  }
}

void TriacBoard::debugPrintOutputs() {
  for (int out = 0; out < ANALOGOUT_COUNT; out++)
  {
    Os::debug("#%i=%3i[%3i-%3i], ", out+1, states[out].value, states[out].from, states[out].to);
  }
  Os::debug("\n");
}

void TriacBoard::main_crossover_rise()
{
  lastZeroCrossDurationInTicks = ticksSinceZeroCross;
  ticksSinceZeroCross = 0;

  //NOMINAL_100HZ_TICKS_PER_RISE is twice the nominal 50Hz duration in ticks 
  //twice because we have 100 zero crossing for a 50Hz sinusoidal wave
  //Force all outputs to zero if the last measured duration is more than 20% off than the nominal one
  //This detects the condition where we don't have a stable 50Hz sinusoidal wave
  input50HzIsStable = true || Utils::absDiff(lastZeroCrossDurationInTicks, NOMINAL_100HZ_TICKS_PER_RISE) < NOMINAL_100HZ_TICKS_MAX_DELTA;

  zeroCrossesCount += 1;
  if (zeroCrossesCount == RISE_PER_SECOND)
  {
    zeroCrossesCount = 0;
    led_heartbeat = !led_heartbeat;
  }
}
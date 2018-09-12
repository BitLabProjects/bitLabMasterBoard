#ifndef _TIMELINE_H_
#define _TIMELINE_H_

#include "mbed.h"

struct TimelineEntry
{
  int32_t time;
  int32_t value;
  int32_t duration;

  void clear()
  {
    time = 0;
    duration = 0;
    value = 0;
  }
};

class Timeline
{
public:
  Timeline();

  void create(uint8_t output, int32_t newEntriesCapacity);
  void clear();
  void setOutput(uint8_t output);
  uint8_t getOutput() { return output; }
  void add(int32_t time, int32_t value, int32_t duration);
  const TimelineEntry *getCurrent();
  void moveFirst();
  void moveNext();
  bool isFinished();
  uint8_t getEntriesCount() { return entriesCount; }

private:
  uint8_t output;
  TimelineEntry *entries;
  int32_t entriesCount;
  int32_t entriesCapacity;
  int32_t currentIdx;
};

#endif
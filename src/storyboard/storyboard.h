#ifndef _STORYBOARD_H_
#define _STORYBOARD_H_

#include "timeline.h"
#include "config.h"

class Storyboard
{
public:
  Storyboard();

  void create(int32_t newTimelinesCapacity, millisec totalDuration);
  Timeline* addTimeline(uint8_t output, int32_t newEntriesCapacity);
  Timeline* getTimeline(uint8_t output);
  bool isFinished(millisec currTime);
  millisec getDuration() { return duration; }
  void reset();
  bool getNextTimelineAndEntry(millisec time, uint8_t* output, const TimelineEntry** entry);
  
  int32_t entriesCount(uint8_t output);

  int32_t timelinesCount;

private:
  Timeline *timelines;
  int32_t timelinesCapacity;
  int32_t duration;

  int getTimelineIdx(uint8_t output);
};

#endif
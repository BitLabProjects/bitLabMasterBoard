#include "storyboard.h"

#include "..\bitLabCore\src\utils.h"
#include "..\bitLabCore\src\os\os.h"

Storyboard::Storyboard()
{
  timelines = NULL;
  timelinesCapacity = 0;
  timelinesCount = 0;
}

void Storyboard::create(int32_t newTimelinesCapacity, millisec totalDuration)
{
  if (timelines)
  {
    delete[] timelines;
  }
  timelines = new Timeline[newTimelinesCapacity];
  timelinesCapacity = newTimelinesCapacity;
  duration = totalDuration;
}

Timeline *Storyboard::addTimeline(uint8_t output, int32_t newEntriesCapacity)
{
  if (timelinesCount < timelinesCapacity)
  {
    timelines[timelinesCount].create(output, newEntriesCapacity);
    timelinesCount += 1;
  }
  else
  {
    Os::debug("Error");
  }

  return &timelines[timelinesCount - 1];
}

Timeline *Storyboard::getTimeline(uint8_t output)
{
  return &timelines[getTimelineIdx(output)];
}

bool Storyboard::isFinished(millisec currTime)
{
  return (currTime >= duration);
}

void Storyboard::reset()
{
  for (uint8_t outputIdx = 0; outputIdx < timelinesCapacity; outputIdx++)
  {
    timelines[outputIdx].moveFirst();
  }
}

bool Storyboard::getNextTimelineAndEntry(millisec time, uint8_t* output, const TimelineEntry** entry) 
{
  if (isFinished(time))
  {
    return false;
  }

  //Os::debug("next search: begin...\n");
  //Look at the next entry for each timeline, and use the one with nearest time
  int8_t idxBest = -1;
  millisec timeBest = 0;
  for (int8_t idx = 0; idx < timelinesCount; idx++)
  {
    if (timelines[idx].isFinished())
    {
      //Os::debug("next search: #%i is finished...\n", idx+1);
      continue;
    }
    const TimelineEntry* curr = timelines[idx].getCurrent();
    if (idxBest == -1 || curr->time < timeBest) {
      //Os::debug("next search: #%i is better, %i ms < %i ms...\n", idx+1, curr->time, timeBest);
      idxBest = idx;
      timeBest = curr->time;
    } else {
      //Os::debug("next search: #%i is worse, %i ms >= %i ms...\n", idx+1, curr->time, timeBest);
    }
  }

  if (idxBest == -1) {
    // All timelines finished
    //Os::debug("next search: nothing found\n");
    return false;
  }

  // One found, return it and advance the relative timeline by one
  *output = idxBest + 1;
  *entry = timelines[idxBest].getCurrent();
  timelines[idxBest].moveNext();

  //Os::debug("next search: best is #%i at %i ms\n", *output, (*entry)->time);

  return true;
}

int Storyboard::getTimelineIdx(uint8_t output)
{
  return Utils::max(0, Utils::min(output - 1, timelinesCapacity - 1));
}
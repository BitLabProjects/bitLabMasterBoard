#ifndef _STORYBOARDLOADER_H_
#define _STORYBOARDLOADER_H_

#include "Storyboard.h"
#include "..\bitLabCore\src\json\Json.h"

class StoryboardLoader {
public:
  StoryboardLoader(Storyboard *storyboard, const char *jsonContent);

  bool load();

private:
  Storyboard* storyboard;
  Json json;

  typedef enum
  {
      SLS_Begin = 0,
      SLS_TimelinesArray = 1,
      SLS_Timeline = 2,
      SLS_EntriesArray = 3,
      SLS_Entry = 4,
      SLS_End = 9999,
  } StoryboardLoaderState_t;

  StoryboardLoaderState_t state;
  static const int bufferSize = 32;
  char key[bufferSize];
  char value[bufferSize];
  Timeline tempTimeline;
  TimelineEntry tempTimelineEntry;

  bool readTimelines(int timelinesArray_ti, int timelinesCount);
  bool readEntries(Timeline* timeline, int entriesArray_ti, int entriesCount);
  bool accept(const JsonAccept_t* acceptArg);

  bool tryMatchInteger(const JsonAccept_t *acceptArg,
                       const char *expectedKey,
                       int &result);
};

#endif
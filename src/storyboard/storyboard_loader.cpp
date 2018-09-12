#include "storyboard_loader.h"

#include "..\bitLabCore\src\json\Json.h"
#include "..\bitLabCore\src\os\os.h"

StoryboardLoader::StoryboardLoader(Storyboard *storyboard, const char *jsonContent) : 
  storyboard(storyboard), json(jsonContent, strlen(jsonContent), callback(this, &StoryboardLoader::accept))
{
}

bool StoryboardLoader::load()
{
  int totalDurationTODO = 0;
  storyboard->create(8 + 32, totalDurationTODO);
  state = SLS_Begin;
  tempTimeline.create(1, 100);
  //Os::debug("Parsing...\n");
  json.parse();
  //Os::debug("Parsed\n");
  return true;
}

inline bool streq(const char *str1, const char *str2)
{
  return strcmp(str1, str2) == 0;
}

bool strCpyWithDefaultAndMaxLen(const char *src, int srcLen, char *dst, int dstSize, const char *defaultStr)
{
  if (srcLen <= 0)
  {
    // Must be an end of object or array, inside of another array not yet completed
    strcpy(dst, "-");
  }
  else
  {
    if (srcLen > dstSize - 1) //-1 to take null terminator into account
    {
      return false;
    }
    strncpy(dst, src, srcLen);
    dst[srcLen] = 0; // NULL-terminate the string
  }
  return true;
}

bool StoryboardLoader::tryMatchInteger(const JsonAccept_t *acceptArg,
                                       const char *expectedKey,
                                       int &result)
{
  if ((acceptArg->type == Json_Accept_KeyValue) &&
      (streq(key, expectedKey)) &&
      (!acceptArg->valueIsString))
  {
    result = atoi(value);
    return true;
  }
  return false;
}

bool StoryboardLoader::accept(const JsonAccept_t *acceptArg)
{
  if (!strCpyWithDefaultAndMaxLen(acceptArg->key, acceptArg->keyLength, key, bufferSize, "-"))
    return false;
  if (!strCpyWithDefaultAndMaxLen(acceptArg->value, acceptArg->valueLength, value, bufferSize, "-"))
    return false;

  JsonAcceptType_t acceptType = acceptArg->type;

  /*
  const char *acceptTypeDescr;
  switch (acceptType)
  {
  case Json_Accept_KeyValue:
    acceptTypeDescr = "KeyValue";
    break;
  case Json_Accept_ObjectBegin:
    acceptTypeDescr = "ObjectBegin";
    break;
  case Json_Accept_ObjectEnd:
    acceptTypeDescr = "ObjectEnd";
    break;
  case Json_Accept_ArrayBegin:
    acceptTypeDescr = "ArrayBegin";
    break;
  case Json_Accept_ArrayEnd:
    acceptTypeDescr = "ArrayEnd";
    break;
  default:
    acceptTypeDescr = "<unknown>";
    break;
  }
  Os::debug("%s of \"%s\"\n", acceptTypeDescr, key);
  */

  switch (state)
  {
  case SLS_Begin:
    if (acceptType == Json_Accept_ArrayBegin && streq(key, "timelines"))
    {
      //Os::debug("timelines [\n");
      state = SLS_TimelinesArray;
      return true;
    }
    break;

  case SLS_TimelinesArray:
    if (acceptType == Json_Accept_ObjectBegin && streq(key, "-"))
    {
      //Os::debug("  timeline {\n");
      tempTimeline.clear();
      state = SLS_Timeline;
      return true;
    }
    if (acceptType == Json_Accept_ArrayEnd && streq(key, "timelines"))
    {
      //Os::debug("timelines ]\n");
      state = SLS_End;
      return true;
    }
    break;

  case SLS_Timeline:
    // TODO name
    // TODO outputType
    int outputId;
    if (tryMatchInteger(acceptArg, "outputId", outputId))
    {
      //Os::debug("    outputId=%i\n", outputId);
      tempTimeline.setOutput(outputId);
      return true;
    }

    if (acceptType == Json_Accept_ArrayBegin && streq(key, "entries"))
    {
      //Os::debug("    entries [\n");
      state = SLS_EntriesArray;
      return true;
    }

    if (acceptType == Json_Accept_ObjectEnd && streq(key, "-"))
    {
      // Timeline finished, add it to the storyboard
      //Os::debug("  timeline }\n");

      Timeline *dstTimeline = storyboard->addTimeline(tempTimeline.getOutput(), tempTimeline.getEntriesCount());
      tempTimeline.moveFirst();
      while (!tempTimeline.isFinished())
      {
        const TimelineEntry *entry = tempTimeline.getCurrent();
        dstTimeline->add(entry->time, entry->value, entry->duration);
        tempTimeline.moveNext();
      }

      state = SLS_TimelinesArray;
      return true;
    }

    break;

  case SLS_EntriesArray:
    if (acceptType == Json_Accept_ObjectBegin && streq(key, "-"))
    {
      //Os::debug("      entry {\n");
      tempTimelineEntry.clear();
      state = SLS_Entry;
      return true;
    }
    if (acceptType == Json_Accept_ArrayEnd && streq(key, "entries"))
    {
      //Os::debug("    entries ]\n");
      state = SLS_Timeline;
      return true;
    }
    break;

  case SLS_Entry:
    int time;
    if (tryMatchInteger(acceptArg, "time", time))
    {
      //Os::debug("        time=%i\n", time);
      tempTimelineEntry.time = time;
      return true;
    }

    int duration;
    if (tryMatchInteger(acceptArg, "duration", duration))
    {
      //Os::debug("        duration=%i\n", duration);
      tempTimelineEntry.duration = duration;
      return true;
    }

    int value;
    if (tryMatchInteger(acceptArg, "value", value))
    {
      //Os::debug("        value=%i\n", value);
      tempTimelineEntry.value = value;
      return true;
    }

    if (acceptType == Json_Accept_ObjectEnd && streq(key, "-"))
    {
      // Entry finished, add it
      //Os::debug("      entry }\n");
      tempTimeline.add(tempTimelineEntry.time, tempTimelineEntry.value, tempTimelineEntry.duration);
      state = SLS_EntriesArray;
      return true;
    }
    break;

  case SLS_End:
    break;
  }

  // Everything else is discarded
  return false;
}
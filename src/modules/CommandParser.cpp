#include "CommandParser.h"

#include <cstring>

#include "..\bitLabCore\src\Utils.h"

CommandParser::CommandParser() : tokensCount(0)
{
}

char *strFindSeparatorOrEnd(const char *str, char c1)
{
  while (true)
  {
    char strChar = *str;
    if (strChar == '\0' || strChar == '\n' || strChar == c1)
    {
      return (char *)str;
    }
    str += 1;
  }
}

bool CommandParser::tryParse()
{
  // Make sure there's a null character at end
  line[lineSize - 1] = '\0';

  char *ptrCurr = line;
  tokensCount = 0;
  while (true)
  {
    char *ptrSeparator = strFindSeparatorOrEnd(ptrCurr, ' ');
    if (ptrSeparator > ptrCurr)
    {
      // We found a token, terminated by either the separator ' ', '\n' or '\0'
      if (tokensCount == tokensSize)
      {
        // We matched a token but no more space is left, error
        return false;
      }

      tokens[tokensCount].idxStart = ptrCurr - line;
      tokens[tokensCount].length = ptrSeparator - ptrCurr;
      tokensCount += 1;
    }

    ptrCurr = ptrSeparator;
    if (*ptrSeparator == ' ')
    {
      // The separator was found, skip it and search another token
      ptrCurr += 1;
      continue;
    }
    else
    {
      // End of input, stop parsing
      break;
    }
  }

  return tokensCount > 0;
}

bool CommandParser::isCommand(const char *cmd)
{
  if (tokensCount < 1)
    return false;
  return strncmp(cmd, &line[tokens[0].idxStart], tokens[0].length);
}

bool CommandParser::tryParseUInt32(uint32_t argIdx, uint32_t &value)
{
  if (argIdx < 1 || argIdx >= tokensCount)
    return false;

  return Utils::strTryParse(&line[tokens[0].idxStart], tokens[0].length, value, 10);
}
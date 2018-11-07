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
    bool isSpace = (*ptrSeparator == ' ');
    // Replace the space, the \n or the \0 with a \0, so the token content can be used like a c string
    *ptrSeparator = '\0';
    if (isSpace)
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
  return strncmp(cmd, &line[tokens[0].idxStart], tokens[0].length) == 0;
}

bool CommandParser::tryParseUInt32(uint32_t tokenIdx, uint32_t &value, uint32_t base)
{
  if (tokenIdx >= tokensCount)
    return false;

  return Utils::strTryParse(&line[tokens[tokenIdx].idxStart], tokens[tokenIdx].length, value, base);
}

const char* CommandParser::getTokenString(uint32_t tokenIdx)
{
  if (tokenIdx >= tokensCount)
    return NULL;

  return &line[tokens[tokenIdx].idxStart];
}

uint32_t CommandParser::getTokenLength(uint32_t tokenIdx)
{
  if (tokenIdx >= tokensCount)
    return 0;

  return tokens[tokenIdx].length;
}
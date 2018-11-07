#ifndef _COMMANDPARSER_H_
#define _COMMANDPARSER_H_

#include <cstdint>

struct Token {
  uint32_t idxStart;
  uint32_t length;
};

class CommandParser
{
public:
  CommandParser();

  const static uint32_t lineSize = 256;
  char line[lineSize];

  bool tryParse();
  inline uint32_t getTokensCount() { return tokensCount; }
  inline void getToken(uint32_t i, Token& token) { token = tokens[i]; }

  bool isCommand(const char* cmd);
  // + 1 because the first token is always the command name
  bool argsCountIs(uint32_t argCount) { return tokensCount == argCount + 1; }
  bool tryParseUInt32(uint32_t tokenIdx, uint32_t &value, uint32_t base = 10);
  const char* getTokenString(uint32_t tokenIdx);
  uint32_t getTokenLength(uint32_t tokenIdx);

private:
  const static uint32_t tokensSize = 128;
  Token tokens[tokensSize];
  uint32_t tokensCount;
};

#endif
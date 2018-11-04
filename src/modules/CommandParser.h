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

  const static uint32_t lineSize = 128;
  char line[lineSize];

  bool tryParse();
  inline uint32_t getTokensCount() { return tokensCount; }
  inline void getToken(uint32_t i, Token& token) { token = tokens[i]; }
  bool isCommand(const char* cmd);

private:
  const static uint32_t tokensSize = 64;
  Token tokens[tokensSize];
  uint32_t tokensCount;
};

#endif
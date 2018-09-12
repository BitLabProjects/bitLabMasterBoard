#include "bitLabCore\src\os\bitLabCore.h"
#include "boards\triac_board.h"
#include "boards\relay_board.h"
#include "storyboard\storyboard_player.h"

bitLabCore core;

RelayBoard relay_board;
TriacBoard triac_board;

int main() {
  core.init();
  core.addModule(new StoryboardPlayer(&relay_board, &triac_board));
  core.run();
}

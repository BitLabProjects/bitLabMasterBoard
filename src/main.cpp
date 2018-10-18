#include "bitLabCore\src\os\bitLabCore.h"
#include "bitLabCore\src\net\RingNetwork.h"
#include "boards\triac_board.h"
#include "boards\relay_board.h"
#include "storyboard\storyboard_player.h"
#include "modules\MasterBoard.h"

bitLabCore core;

//RelayBoard relay_board;
//TriacBoard triac_board;

int main() {
  core.init();
  //core.addModule(new StoryboardPlayer(&relay_board, &triac_board));
  core.addModule(new RingNetwork(PA_11, PA_12, core.getHardwareId(), true));
  core.addModule(new MasterBoard());
  core.run();
}

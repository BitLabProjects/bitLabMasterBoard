#include "bitLabCore\src\os\bitLabCore.h"
#include "bitLabCore\src\net\RingNetwork.h"
#include "bitLabCore\src\storyboard\StoryboardPlayer.h"
#include "boards\triac_board.h"
#include "boards\relay_board.h"
#include "modules\MasterBoard.h"

bitLabCore core;
RingNetwork rn(PA_11, PA_12, true);
MasterBoard mb;

int main() {
  core.init();
  core.addModule(&rn);
  core.addModule(&mb);
  core.run();
}

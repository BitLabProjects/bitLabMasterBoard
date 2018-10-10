#ifndef _MASTERBOARD_H_
#define _MASTERBOARD_H_

#include "..\bitLabCore\src\os\bitLabCore.h"
#include "..\bitLabCore\src\net\RingNetwork.h"

class MasterBoard : public CoreModule
{
public:
  MasterBoard();

  // --- CoreModule ---
  const char* getName() { return "MasterBoard"; }
  void init(const bitLabCore*);
  void mainLoop();
  void tick(millisec64 timeDelta);
  // ------------------

private:
  RingNetwork* ringNetwork;
  DigitalOut led;

  void dataPacketReceived(RingPacket*, PTxAction*);
};

#endif
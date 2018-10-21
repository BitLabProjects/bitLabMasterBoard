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
  void tick(millisec timeDelta);
  // ------------------

private:
  RingNetwork* ringNetwork;
  DigitalOut led;

  enum EState {
    WaitAddressAssigned,
    Idle,
    Enumerate_Start,
    Enumerate_WaitHello,
    ToggleLed_Start,
  };
  EState state;
  uint32_t freePacketsCount;

  uint8_t enumeratedAddresses[10];
  uint32_t enumeratedAddressesCount;

  void onPacketReceived(RingPacket*, PTxAction*);
};

#endif
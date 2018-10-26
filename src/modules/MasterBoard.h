#ifndef _MASTERBOARD_H_
#define _MASTERBOARD_H_

#include "..\bitLabCore\src\os\bitLabCore.h"
#include "..\bitLabCore\src\net\RingNetwork.h"

#include "..\bitLabCore\src\storyboard\Storyboard.h"

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
    SendStoryboard_Start,
    SendStoryboard_SendTimelines,
    CheckStoryboard_Start,
    CheckStoryboard_WaitCrc,
    Play_Start,
    Stop_Start,
  };
  EState state;
  void goToState(EState newState);
  void goToStateIdle();
  uint32_t freePacketsCount;

  struct EnumeratedDeviceInfo {
    uint8_t address;
    uint32_t hardwareId;
    uint32_t crcReceived;
  };

  EnumeratedDeviceInfo enumeratedAddresses[10];
  uint32_t enumeratedAddressesCount;

  // data variables for the state machine
  uint32_t currDeviceIdx;
  uint8_t nextTimelineIdxMaybeToSend;

  Storyboard storyboard;

  void onPacketReceived(RingPacket*, PTxAction*);

  void mainLoop_checkForWaitStateTimeout();
  millisec waitStateTimeout;
  bool waitStateTimeoutEnabled;
};

#endif
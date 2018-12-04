#ifndef _MASTERBOARD_H_
#define _MASTERBOARD_H_

#include "..\bitLabCore\src\os\bitLabCore.h"
#include "..\bitLabCore\src\net\RingNetwork.h"

#include "..\bitLabCore\src\storyboard\Storyboard.h"

//#include "..\bitLabCore\src\display\SSD1306.h"

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
  DigitalIn inPlay;
  DigitalIn inStop;
  uint32_t hardwareId;
  const char* clockSourceDescr;

  millisec upTime;
  millisec eachSecondTimeout;
  bool secondElapsed;

  millisec storyboardTime;
  millisec storyboardTimeAtLastGetState;
  bool isPlaying;

  // Serial protocol state
  FILE *openFile;

  enum EState {
    WaitAddressAssigned,
    Idle,
    Enumerate_Start,
    Enumerate_WaitHello,
    ToggleLed_Start,
    SendStoryboard_Start,
    SendStoryboard_SendTimelines,
    ReadState_Start,
    ReadState_WaitCrc,
    Play_Start,
    Stop_Start,
    SetOutput_Start,
  };
  EState state;

  // data variables for the state machine
  uint32_t state_currDeviceIdx;
  uint8_t state_nextTimelineIdxMaybeToSend;
  uint8_t stateArg_OutputId; // setOutput
  uint32_t stateArg_Value; // setOutput

  void goToState(EState newState);
  void goToStateIdle();
  bool tryGoToStateIfIdleAndHasDevices(EState newState, uint32_t currDeviceIdx = 0);
  uint32_t freePacketsCount;

  struct EnumeratedDeviceInfo {
    uint8_t address;
    uint32_t hardwareId;
    uint32_t crcReceived;
    millisec storyboardTime;
  };

  EnumeratedDeviceInfo enumeratedAddresses[10];
  uint32_t enumeratedAddressesCount;
  inline bool isIdleAndHasDevices() { return state == EState::Idle && enumeratedAddressesCount > 0; }
  int32_t findDeviceByHardwareId(uint32_t hardwareId);

  Storyboard storyboard;

  void onPacketReceived(RingPacket*, PTxAction*);

  void mainLoop_checkForWaitStateTimeout();
  void mainLoop_serialProtocol();
  millisec waitStateTimeout;
  bool waitStateTimeoutEnabled;
};

#endif
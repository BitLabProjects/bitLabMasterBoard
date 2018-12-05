#include "MasterBoard.h"
#include "CommandParser.h"

#include "..\bitLabCore\src\utils.h"
#include "..\bitLabCore\src\storyboard\StoryboardLoader.h"

Serial serial(USBTX, USBRX);

MasterBoard::MasterBoard() : led(LED2),
                             lastIsConnected(false),
                             connectionLostCount(0),
                             inPlay(PB_13),
                             inStop(PB_14),
                             inputDebounceTimeout(0),
                             pressedKey(EInputKey::Key_None),
                             pressedKeyTime(0),
                             isDisplayDirty(false),
                             displayState(EDisplayState::Home),
                             selectedCommand(ECommand::Load),
                             i2c(D5, D7),
                             oled(i2c, NC),
                             upTime(0),
                             eachSecondTimeout(1000),
                             secondElapsed(false),
                             storyboardTime(0),
                             isPlaying(false),
                             openFile(NULL),
                             state(EState::WaitAddressAssigned),
                             protocolState(EProtocolState::PS_Idle),
                             state_currDeviceIdx(0),
                             state_nextTimelineIdxMaybeToSend(0),
                             stateArg_OutputId(0),
                             stateArg_Value(0),
                             freePacketsCount(0),
                             enumeratedAddressesCount(0),
                             storyboard(),
                             waitStateTimeout(0),
                             waitStateTimeoutEnabled(false)
{
}

void MasterBoard::init(const bitLabCore *core)
{
  serial.baud(115200);
  serial.puts("Hello!\n");

  oled.clearDisplay();
  oled.printf("Initializing...");
  oled.display();

  //serial.baud(1200);
  ringNetwork = (RingNetwork *)core->findModule("RingNetwork");
  ringNetwork->attachOnPacketReceived(callback(this, &MasterBoard::onPacketReceived));

  hardwareId = core->getHardwareId();
  clockSourceDescr = core->getClockSourceDescr();

  oled.printf("[ok]\r\n");
  oled.printf("hwId=%08X\r\n", hardwareId);
  oled.display();
}

void MasterBoard::mainLoop()
{
  bool newIsConnected = ringNetwork->getIsConnected();
  if (lastIsConnected != newIsConnected)
  {
    if (!newIsConnected)
    {
      connectionLostCount += 1;
    }
    lastIsConnected = newIsConnected;
    isDisplayDirty = true;
  }

  switch (state)
  {
  case EState::WaitAddressAssigned:
    if (ringNetwork->isAddressAssigned())
    {
      serial.printf("Address assigned: %i\n", ringNetwork->getAddress());
      serial.printf("Starting enumeration...\n");
      enumeratedAddressesCount = 0;
      goToState(EState::Enumerating, EProtocolState::Enumerate_Start);
    }
    break;

  case EState::Enumerating:
    if (protocolState == EProtocolState::PS_Idle)
    {
      serial.printf("Enumeration completed, %i found\n", enumeratedAddressesCount);
      // Enumeration is complete
      isDisplayDirty = true;
      state = EState::Idle;
    }
    break;

  case EState::Idle:
    break;

  case EState::BusyWithProtocol:
    break;
  }

  if (secondElapsed)
  {
    secondElapsed = false;
    // Update the display with live stats once a second (packet count)
    if (displayState == EDisplayState::Stats)
    {
      isDisplayDirty = true;
    }
  }

  mainLoop_checkForWaitStateTimeout();

  mainLoop_serialProtocol();

  mainLoop_keyboard();

  printDisplay();
}

void MasterBoard::mainLoop_checkForWaitStateTimeout()
{
  if (waitStateTimeoutEnabled)
  {
    if (waitStateTimeout == 0)
    {
      goToStateIdle();
      serial.puts("Timeout\n");
    }
  }
}

void MasterBoard::mainLoop_serialProtocol()
{
  if (serial.readable())
  {
    CommandParser cp;
    serial.gets(cp.line, cp.lineSize);

    if (!cp.tryParse())
    {
      serial.printf("Invalid command format\n\n");
    }
    else
    {
      //serial.printf("cmd: <%s>x%u", cp.getTokenString(0), cp.getTokenLength(0));
      bool commandIsOk = true;
      // The first token is the command name
      if (cp.isCommand("state"))
      {
        serial.printf("Up time: %u sec\n", upTime / 1000);
        serial.printf("Free packets: %u\n", freePacketsCount);
        serial.printf("Net state: %s\n", ringNetwork->getIsConnected() ? "connected" : "disconnected");
        serial.printf("Enumerated devices: [");
        for (uint32_t i = 0; i <= enumeratedAddressesCount; i++)
        {
          auto me = (i == 0);
          if (i > 0)
            serial.puts(", ");
          serial.printf("addr:%i; hwId:%08X; crc:%08X; time:%i",
                        me ? ringNetwork->getAddress() : enumeratedAddresses[i - 1].address,
                        me ? hardwareId : enumeratedAddresses[i - 1].hardwareId,
                        me ? storyboard.calcCrc32(0) : enumeratedAddresses[i - 1].crcReceived,
                        me ? storyboardTimeAtLastGetState : enumeratedAddresses[i - 1].storyboardTime);
        }
        serial.printf("]\n");
      }
      else if (cp.isCommand("clock"))
      {
        serial.printf("Clock type: %s\n", clockSourceDescr);
      }
      else if (cp.isCommand("toggleLed"))
      {
        commandIsOk = tryGoToStateIfIdleAndHasDevices(EProtocolState::ToggleLed_Start);
      }
      else if (cp.isCommand("load"))
      {
        commandIsOk = command_Load();
      }
      else if (cp.isCommand("upload"))
      {
        commandIsOk = command_Upload();
      }
      else if (cp.isCommand("check"))
      {
        commandIsOk = tryGoToStateIfIdleAndHasDevices(EProtocolState::ReadState_Start);
        if (commandIsOk)
        {
          storyboardTimeAtLastGetState = storyboardTime;
        }
      }
      else if (cp.isCommand("play"))
      {
        commandIsOk = command_Play();
      }
      else if (cp.isCommand("stop"))
      {
        commandIsOk = command_Stop();
      }
      else if (cp.isCommand("setOutput"))
      {
        // Format:
        // setOutput <hardwareId: ui32> <outputId: ui8> <value: [0-4095]>
        commandIsOk = false;
        if (cp.argsCountIs(3))
        {
          uint32_t hardwareId, outputId, value;
          if (cp.tryParseUInt32(1, hardwareId, 16) &&
              cp.tryParseUInt32(2, outputId) &&
              cp.tryParseUInt32(3, value))
          {
            auto deviceIdx = findDeviceByHardwareId(hardwareId);
            if (deviceIdx >= 0)
            {
              stateArg_OutputId = outputId;
              stateArg_Value = value;
              commandIsOk = tryGoToStateIfIdleAndHasDevices(EProtocolState::SetOutput_Start, deviceIdx);
            }
            else
            {
              serial.printf("Could not find device\n");
            }
          }
        }
      }
      else if (cp.isCommand("openFile"))
      {
        // Format:
        // openFile <fileName>
        commandIsOk = false;
        if (cp.argsCountIs(2))
        {
          if (openFile != NULL)
          {
            serial.printf("A file is already open\n");
          }
          else
          {
            auto path = cp.getTokenString(1);
            auto mode = cp.getTokenString(2);
            openFile = fopen(path, mode);
            if (openFile == NULL)
            {
              serial.printf("Can't open file\n");
            }
            else
            {
              commandIsOk = true;
            }
          }
        }
      }
      else if (cp.isCommand("closeFile"))
      {
        commandIsOk = false;
        if (openFile == NULL)
        {
          // Allow closing a file with success when none is open.
          commandIsOk = true;
        }
        else
        {
          fclose(openFile);
          openFile = NULL;
          commandIsOk = true;
        }
      }
      else if (cp.isCommand("writeFile"))
      {
        commandIsOk = false;
        if (openFile == NULL)
        {
          serial.printf("No open file\n");
        }
        else
        {
          const uint8_t buffSize = 183;
          uint8_t buff[buffSize];
          uint32_t buffLength = 0;

          if (!Utils::tryBase64Decode(cp.getTokenString(1), cp.getTokenLength(1),
                                      buff, buffSize, &buffLength))
          {
            serial.printf("Base64 decode failed\n");
          }
          else
          {
            if (fwrite(buff, 1, buffLength, openFile) != buffLength)
            {
              serial.printf("Write failed\n");
            }
            else
            {
              commandIsOk = true;
            }
          }
        }
      }
      else if (cp.isCommand("crc32File"))
      {
        commandIsOk = false;
        if (openFile == NULL)
        {
          serial.printf("No open file\n");
        }
        else
        {
          // Save the current position then seek to beginning to crc the whole file
          auto prevSeekPos = ftell(openFile);
          fseek(openFile, 0, SEEK_SET);

          uint32_t crc32 = 0;
          uint8_t fileByte;
          while (fread(&fileByte, 1, 1, openFile) == 1)
          {
            crc32 = Utils::crc32(fileByte, crc32);
          }

          // Restore previous seek position
          fseek(openFile, prevSeekPos, SEEK_SET);

          serial.printf("crc32: %08X\n", crc32);
          commandIsOk = true;
        }
      }

      if (commandIsOk)
      {
        serial.printf("Ok\n");
      }
      else
      {
        serial.printf("Error\n");
      }
    }
  }
}

bool MasterBoard::command_Load()
{
  if (!isStateBusy())
  {
    FILE *file = fopen("/sd/storyboard.json", "r");
    if (file == NULL)
    {
      serial.printf("File not available\n");
      return false;
    }
    else
    {
      fseek(file, 0, SEEK_END);
      long fileSize = ftell(file);
      fseek(file, 0, SEEK_SET);
      serial.printf("Reading %i bytes\n", fileSize);

      char *buff = new char[fileSize];
      fread(buff, fileSize, 1, file);
      fclose(file);

      serial.printf("Parsing storyboard\n");
      StoryboardLoader loader(&storyboard, buff);
      loader.load();

      serial.printf("Loaded %i timelines, duration: %i ms\n",
                    storyboard.getTimelinesCount(),
                    storyboard.getDuration());
      return true;
    }
  }
  else
  {
    return false;
  }
}
bool MasterBoard::command_Upload()
{
  return tryGoToStateIfIdleAndHasDevices(EProtocolState::SendStoryboard_Start);
}
bool MasterBoard::command_Play()
{
  if (tryGoToStateIfIdleAndHasDevices(EProtocolState::Play_Start))
  {
    isPlaying = true;
    return true;
  }
  return false;
}
bool MasterBoard::command_Stop()
{
  if (tryGoToStateIfIdleAndHasDevices(EProtocolState::Stop_Start))
  {
    isPlaying = false;
    return true;
  }
  return false;
}

void MasterBoard::mainLoop_keyboard()
{
  // Check for debounce
  if (inputDebounceTimeout > 0)
    return;

  EInputKey releasedKey = EInputKey::Key_None;

  if (pressedKey == EInputKey::Key_None)
  {
    if (inPlay.read())
    {
      pressedKeyTime = 0;
      pressedKey = EInputKey::Key_A;
      inputDebounceTimeout = InputDebounceTimeoutValue;
    }
    else if (inStop.read())
    {
      pressedKeyTime = 0;
      pressedKey = EInputKey::Key_B;
      inputDebounceTimeout = InputDebounceTimeoutValue;
    }
  }
  else
  {
    if (pressedKey == EInputKey::Key_A)
    {
      if (!inPlay.read())
      {
        releasedKey = pressedKey;
        pressedKey = EInputKey::Key_None;
      }
    }
    else if (pressedKey == EInputKey::Key_B)
    {
      if (!inStop.read())
      {
        releasedKey = pressedKey;
        pressedKey = EInputKey::Key_None;
      }
    }
  }

  switch (releasedKey)
  {
  case EInputKey::Key_None:
    break;

  case EInputKey::Key_A:
    displayState = (EDisplayState)((displayState + 1) % EDisplayState::DisplayStateCount);
    isDisplayDirty = true;
    break;

  case EInputKey::Key_B:
    switch (displayState)
    {
    case EDisplayState::Home:
      break;

    case EDisplayState::DeviceList:
      break;

    case EDisplayState::Commands:
      if (pressedKeyTime > 1000)
      {
        // Apply current command
        switch (selectedCommand)
        {
        case ECommand::Load:
          command_Load();
          break;
        case ECommand::Upload:
          command_Upload();
          break;
        case ECommand::Play:
          command_Play();
          break;
        case ECommand::Stop:
          command_Stop();
          break;
        }
      }
      else
      {
        // Go to next command
        selectedCommand = (ECommand)((selectedCommand + 1) % ECommand::CommandsCount);
        isDisplayDirty = true;
      }
      break;

    case EDisplayState::Stats:
      break;
    }
    break;
  }
}

void MasterBoard::printDisplay()
{
  if (!isDisplayDirty)
    return;

  isDisplayDirty = false;

  oled.clearDisplay();
  oled.setTextCursor(0, 0);
  switch (displayState)
  {
  case EDisplayState::Home:
    //oled.printf("== Il presepe + fico ==\n");
    oled.printf("== Pimp my presepe ==\n");
    oled.printf("Net: %s\n", ringNetwork->getIsConnected() ? "connected" : "disconnected");
    oled.printf("Devices: %i\n", enumeratedAddressesCount);
    oled.printf("Play status: %s\n", isPlaying ? "playing" : "stopped");
    break;

  case EDisplayState::DeviceList:
    oled.printf("== Devices ==\n");
    break;

  case EDisplayState::Commands:
    oled.printf("== Comandi ==\n> ");
    switch (selectedCommand)
    {
    case ECommand::Load:
      oled.printf("Load\n");
      break;
    case ECommand::Upload:
      oled.printf("Upload\n");
      break;
    case ECommand::Play:
      oled.printf("Play\n");
      break;
    case ECommand::Stop:
      oled.printf("Stop\n");
      break;
    }
    break;

  case EDisplayState::Stats:
    oled.printf("== Statistiche ==\n");
    oled.printf("Uptime: %i s\n", upTime / 1000);
    oled.printf("Conn. lost: %i\n", connectionLostCount);
    oled.printf("Packets: %i\n", freePacketsCount);
    break;
  }
  oled.display();
}

int32_t MasterBoard::findDeviceByHardwareId(uint32_t hardwareId)
{
  for (uint32_t i = 0; i < enumeratedAddressesCount; i++)
  {
    if (enumeratedAddresses[i].hardwareId == hardwareId)
      return i;
  }
  return -1;
}

void MasterBoard::goToState(EState newState, EProtocolState newProtocolState)
{
  state = newState;
  goToProtocolState(newProtocolState);
}
void MasterBoard::goToProtocolState(EProtocolState newProtocolState)
{
  protocolState = newProtocolState;
  waitStateTimeout = 1000;
  waitStateTimeoutEnabled = true;
}
void MasterBoard::goToStateIdle()
{
  protocolState = EProtocolState::PS_Idle;
  waitStateTimeoutEnabled = false;
  waitStateTimeout = 0;
  state = EState::Idle;
}
void MasterBoard::goToStateIdle2()
{
  protocolState = EProtocolState::PS_Idle;
  waitStateTimeoutEnabled = false;
  waitStateTimeout = 0;
}
bool MasterBoard::tryGoToStateIfIdleAndHasDevices(EProtocolState newState, uint32_t currDeviceIdx)
{
  if (isIdleAndHasDevices())
  {
    state_currDeviceIdx = currDeviceIdx;
    goToState(EState::BusyWithProtocol, newState);
    return true;
  }
  else
  {
    return false;
  }
}

void MasterBoard::tick(millisec timeDelta)
{
  waitStateTimeout -= timeDelta;
  if (waitStateTimeout < 0)
    waitStateTimeout = 0;

  inputDebounceTimeout -= timeDelta;
  if (inputDebounceTimeout < 0)
    inputDebounceTimeout = 0;

  if (pressedKey != EInputKey::Key_None)
  {
    pressedKeyTime += timeDelta;
  }

  upTime += timeDelta;

  eachSecondTimeout -= timeDelta;
  if (eachSecondTimeout <= 0)
  {
    secondElapsed = true;
    eachSecondTimeout = 1000;
  }

  if (isPlaying)
  {
    storyboardTime += timeDelta;
    if (storyboardTime >= storyboard.getDuration())
    {
      storyboardTime -= storyboard.getDuration();
    }
  }
}

// TODO State machine timeout for waiting states

/*
--- Enumerate procedure ---
Purpose: discover other devices that belong to the ring network
1. Enumerate_Start sends a WhoAreYou packet with ttl=n, with n starting from 1, 
   and goes into Enumerate_WaitHello state
2. Enumerate_WaitHello waits for an Hello packet for this device:
   If the source address if this device, the enumeration is completed
   Otherwise it records the address contained, sets n=n+1 and returns to Enumerate_Start until n=10

--- Storyboard upload procedure ---
Purpose: upload the storyboard to the enumerated devices, sending to each device the timelines with 
         matching hardwareId
1. SendStoryboard_Start sends to the first enumerated device a CreateStoryboard packet with:
   - The number of timelines for that device hardwareId
   - The total storyboard duration (shared across all timelines of all devices)
   - For each timeline, outputId and entry count
   Then goes into SendStoryboard_SendTimelines state
2. SendStoryboard_SendTimelines cycles through the timelines for the device hardwareId and
   sends the timeline entries using a SetTimelineEntries packet.
   If all entries do not fit in a SetTimelineEntries packet, multiple packets for the same 
   timeline are sent.
   When all timelines are sent:
   If this device was the last device, the upload is ended.
   Otherwise it goes into SendStoryboard_Start state for the next device.

--- ReadState procedure ---
Purpose: Retrieve the state of enumerated device, to check the uploaded storyboard crc and time sync.
1. ReadState_Start sends a GetState packet to the first device and
   goes into ReadState_WaitCrc state
2. ReadState_WaitCrc waits for a TellState packet from the device
   It checks the crc received then goes into ReadState_Start state for the next device
*/

enum EMsgType
{
  SetLed = 1,
  CreateStoryboard = 2,
  SetTimelineEntries = 3,
  GetState = 4,
  TellState = 5,
  SyncStoryboardTime = 6,
  Play = 7,
  Pause = 8,
  Stop = 9,
  SetOutput = 10,
  DebugPrint = 255
};

void MasterBoard::onPacketReceived(RingPacket *p, PTxAction *pTxAction)
{
  *pTxAction = PTxAction::SendFreePacket;

  auto isFree = p->isFreePacket();
  if (isFree)
  {
    freePacketsCount += 1;
  }
  else
  {
    /*
    serial.printf("pkt> #%i %c src:%i dst:%i ttl:%i\n",
                  p->header.data_size,
                  p->isProtocolPacket() ? 'p' : 'd',
                  p->header.src_address,
                  p->header.dst_address,
                  p->header.ttl);
    */
  }

  if (p->isDataPacket(ringNetwork->getAddress(), 0, EMsgType::DebugPrint))
  {
    char *buff = (char *)&p->data[1];
    buff[p->header.data_size - 2] = '\0';
    serial.printf(buff);
    return;
  }

  // 1.Send a WhoAreYou packet with ttl from 1 to 11 and wait for the response Hello packet
  // 1. Send
  switch (protocolState)
  {
  case EProtocolState::PS_Idle:
    break;

  case EProtocolState::Enumerate_Start:
    if (isFree)
    {
      led = !led;
      p->header.data_size = 1;
      p->header.control = 0;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = 0;
      p->header.ttl = enumeratedAddressesCount + 1;
      p->data[0] = RingNetworkProtocol::protocol_msgid_whoareyou;
      *pTxAction = PTxAction::Send;
      goToProtocolState(EProtocolState::Enumerate_WaitHello);
      return;
    }
    break;
  case EProtocolState::Enumerate_WaitHello:
    if (p->isProtocolPacket() &&
        p->isForDstAddress(ringNetwork->getAddress()) &&
        p->header.data_size >= (1 + 4) &&
        p->data[0] == RingNetworkProtocol::protocol_msgid_hello)
    {
      // If we asked ourself who we are, the loop is completed
      led = !led;
      uint8_t src_address = p->header.src_address;
      bool isMyself = (src_address == ringNetwork->getAddress());
      if (isMyself)
      {
        goToStateIdle2();
      }
      else
      {
        enumeratedAddresses[enumeratedAddressesCount].address = src_address;
        enumeratedAddresses[enumeratedAddressesCount].hardwareId = p->getDataUInt32(1);
        enumeratedAddresses[enumeratedAddressesCount].crcReceived = 0;
        enumeratedAddressesCount += 1;
        if (enumeratedAddressesCount == 10)
        {
          goToStateIdle2();
        }
        else
        {
          goToProtocolState(EProtocolState::Enumerate_Start);
        }
      }
      return;
    }
    break;
  case EProtocolState::ToggleLed_Start:
    if (isFree)
    {
      bool ledState = !led;
      led = ledState;
      p->header.data_size = 2;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[0].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::SetLed;
      p->data[1] = ledState;
      *pTxAction = PTxAction::Send;
      goToStateIdle();
      return;
    }
    break;

  case EProtocolState::SendStoryboard_Start:
    if (isFree)
    {
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[state_currDeviceIdx].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::CreateStoryboard;
      p->data[1] = 0; // timelines count, will be set later, we don't know yet
      p->setDataInt32(2, storyboard.getDuration());

      uint32_t timelinesCount = 0;
      for (uint8_t i = 0; i < storyboard.getTimelinesCount(); i++)
      {
        auto t = storyboard.getTimelineByIdx(i);
        if (t->getOutputHardwareId() == enumeratedAddresses[state_currDeviceIdx].hardwareId)
        {
          auto offset = 6 + timelinesCount * 2;
          p->data[offset + 0] = t->getOutputId();
          p->data[offset + 1] = t->getEntriesCount();
          timelinesCount += 1;

          // Never send more than 32 timelines, they won't fit.
          // TODO Check storyboard validity: can't have more than 32 timelines for device
          if (timelinesCount == 32)
          {
            break;
          }
        }
      }

      // serial.printf("Sending %i timelines\n", timelinesCount);

      // Now we know
      p->data[1] = timelinesCount;
      p->header.data_size = 6 + timelinesCount * 2;

      *pTxAction = PTxAction::Send;
      state_nextTimelineIdxMaybeToSend = 0;
      goToProtocolState(EProtocolState::SendStoryboard_SendTimelines);
    }
    break;

  case EProtocolState::SendStoryboard_SendTimelines:
    if (isFree)
    {
      // search for the next timeline to send
      Timeline *t;
      uint8_t idxTimelineToSend;
      bool found = false;
      for (uint8_t i = state_nextTimelineIdxMaybeToSend; i < storyboard.getTimelinesCount(); i++)
      {
        t = storyboard.getTimelineByIdx(i);
        if (t->getOutputHardwareId() == enumeratedAddresses[state_currDeviceIdx].hardwareId)
        {
          found = true;
          idxTimelineToSend = i;
          break;
        }
      }

      if (!found)
      {
        if (state_currDeviceIdx == enumeratedAddressesCount - 1)
        {
          // No more devices, done
          goToStateIdle();
        }
        else
        {
          state_currDeviceIdx += 1;
          goToProtocolState(EProtocolState::SendStoryboard_Start);
        }
      }
      else
      {
        p->header.control = 1;
        p->header.src_address = ringNetwork->getAddress();
        p->header.dst_address = enumeratedAddresses[state_currDeviceIdx].address;
        p->header.ttl = RingNetworkProtocol::ttl_max;
        p->data[0] = EMsgType::SetTimelineEntries;
        p->data[1] = t->getOutputId();
        p->data[2] = 0;
        // For now send only first 20 entries, don't bother with cycles
        auto entryCountToSend = Utils::min(20, t->getEntriesCount());
        p->data[3] = entryCountToSend;

        // serial.printf("Sending %i entries\n", entryCountToSend);

        for (int i = 0; i < entryCountToSend; i++)
        {
          auto offset = 4 + i * 12;
          auto entry = t->getEntry(i);
          p->setDataInt32(offset + 0, entry->time);
          p->setDataInt32(offset + 4, entry->value);
          p->setDataInt32(offset + 8, entry->duration);
        }
        p->header.data_size = 4 + entryCountToSend * 12;

        *pTxAction = PTxAction::Send;

        // Stay in EProtocolState::SendStoryboard_SendTimelines state
        state_nextTimelineIdxMaybeToSend = idxTimelineToSend + 1;
      }
    }
    break;

  case EProtocolState::ReadState_Start:
    if (isFree)
    {
      p->header.data_size = 1;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[state_currDeviceIdx].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::GetState;
      *pTxAction = PTxAction::Send;
      goToProtocolState(EProtocolState::ReadState_WaitCrc);
    }
    break;

  case EProtocolState::ReadState_WaitCrc:
    if (p->isDataPacket(ringNetwork->getAddress(), 1 + 4 + 4, EMsgType::TellState))
    {
      enumeratedAddresses[state_currDeviceIdx].crcReceived = p->getDataUInt32(1);
      enumeratedAddresses[state_currDeviceIdx].storyboardTime = p->getDataInt32(1 + 4);

      if (state_currDeviceIdx == enumeratedAddressesCount - 1)
      {
        goToStateIdle();
      }
      else
      {
        state_currDeviceIdx += 1;
        goToProtocolState(EProtocolState::ReadState_Start);
      }
    }
    break;

  case EProtocolState::Play_Start:
    if (isFree)
    {
      p->header.data_size = 1;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = RingNetworkProtocol::broadcast_address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::Play;
      *pTxAction = PTxAction::Send;
      goToStateIdle();
    }
    break;

  case EProtocolState::Stop_Start:
    if (isFree)
    {
      p->header.data_size = 1;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = RingNetworkProtocol::broadcast_address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::Stop;
      *pTxAction = PTxAction::Send;
      goToStateIdle();
    }
    break;
  case EProtocolState::SetOutput_Start:
    if (isFree)
    {
      p->header.data_size = 1 + 1 + 4;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[state_currDeviceIdx].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::SetOutput;
      p->data[1] = stateArg_OutputId;
      p->setDataUInt32(2, stateArg_Value);
      *pTxAction = PTxAction::Send;
      goToStateIdle();
    }
    break;
  }
}

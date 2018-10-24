#include "MasterBoard.h"

#include "..\bitLabCore\src\utils.h"

Serial serial(USBTX, USBRX);

MasterBoard::MasterBoard() : led(LED2),
                             state(EState::WaitAddressAssigned),
                             freePacketsCount(0),
                             enumeratedAddressesCount(0),
                             storyboard()
{
}

void MasterBoard::init(const bitLabCore *core)
{
  serial.baud(115200);
  serial.puts("Hello!\n");
  //serial.baud(1200);
  ringNetwork = (RingNetwork *)core->findModule("RingNetwork");
  ringNetwork->attachOnPacketReceived(callback(this, &MasterBoard::onPacketReceived));
}

void MasterBoard::mainLoop()
{
  if (state == EState::WaitAddressAssigned)
  {
    //serial.puts("W\n");
    if (ringNetwork->isAddressAssigned())
    {
      serial.printf("Address assigned: %i\n", ringNetwork->getAddress());
      serial.printf("Starting enumeration...\n");
      enumeratedAddressesCount = 0;
      state = EState::Enumerate_Start;
    }
  }

  if (serial.readable())
  {
    char line[128];
    serial.gets(line, 127);
    if (strcmp(line, "state\n") == 0)
    {
      serial.printf("Address: %i\n", ringNetwork->getAddress());
      serial.printf("Enumerated devices: [");
      for (uint32_t i = 0; i < enumeratedAddressesCount; i++)
      {
        if (i > 0)
          serial.puts(", ");
        serial.printf("addr:%i; hwId:%i; crc:%i", 
                      enumeratedAddresses[i].address, 
                      enumeratedAddresses[i].hardwareId,
                      enumeratedAddresses[i].crcReceived);
      }
      serial.printf("]\n");
      serial.printf("Storyboard crc: %u\n", storyboard.calcCrc32(0));
      serial.printf("Free packets: %i\n", freePacketsCount);
    }
    else if (strcmp(line, "toggleLed\n") == 0)
    {
      if (state == EState::Idle && enumeratedAddressesCount > 0)
      {
        state = EState::ToggleLed_Start;
        serial.printf("Ok\n");
      }
      else
      {
        serial.printf("Error\n");
      }
    }
    else if (strcmp(line, "upload\n") == 0)
    {
      if (state == EState::Idle && enumeratedAddressesCount > 0)
      {
        currDeviceIdx = 0;
        state = EState::SendStoryboard_Start;
        serial.printf("Ok\n");
      }
      else
      {
        serial.printf("Error\n");
      }
    }
    else if (strcmp(line, "check\n") == 0)
    {
      if (state == EState::Idle && enumeratedAddressesCount > 0)
      {
        currDeviceIdx = 0;
        state = EState::CheckStoryboard_Start;
        serial.printf("Ok\n");
      }
      else
      {
        serial.printf("Error\n");
      }
    }
    else
    {
      serial.puts("Command unrecognized\n");
    }
    serial.puts("\n");
  }
}

void MasterBoard::tick(millisec timeDelta)
{
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

--- Storyboard check procedure ---
Purpose: check the storyboard uploaded to the enumerated devices, asking each device its crc
1. CheckStoryboard_Start sends a GetStoryboardChecksum packet to the first device and
   goes into CheckStoryboard_WaitCrc state
2. CheckStoryboard_WaitCrc waits for a TellStoryboardChecksum packet from the device
   It checks the crc received then goes into CheckStoryboard_Start state for the next device
*/

enum EMsgType
{
  SetLed = 1,
  CreateStoryboard = 2,
  SetTimelineEntries = 3,
  GetStoryboardChecksum = 4,
  TellStoryboardChecksum = 5,
  SetStoryboardTime = 6,
  Play = 7,
  Pause = 8,
  Stop = 9
};

void MasterBoard::onPacketReceived(RingPacket *p, PTxAction *pTxAction)
{
  *pTxAction = PTxAction::SendFreePacket;

  auto isFree = p->isFreePacket();
  if (isFree)
  {
    freePacketsCount += 1;
  }
  // 1.Send a WhoAreYou packet with ttl from 1 to 11 and wait for the response Hello packet
  // 1. Send
  switch (state)
  {
  case EState::Idle:
    break;
  case EState::WaitAddressAssigned:
    break;

  case EState::Enumerate_Start:
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
      state = EState::Enumerate_WaitHello;
      return;
    }
    break;
  case EState::Enumerate_WaitHello:
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
        state = EState::Idle;
      }
      else
      {
        enumeratedAddresses[enumeratedAddressesCount].address = src_address;
        enumeratedAddresses[enumeratedAddressesCount].hardwareId = *((uint32_t *)&p->data[1]);
        enumeratedAddresses[enumeratedAddressesCount].crcReceived = 0;
        enumeratedAddressesCount += 1;
        if (enumeratedAddressesCount == 10)
        {
          state = EState::Idle;
        }
        else
        {
          state = EState::Enumerate_Start;
        }
      }
      return;
    }
    break;
  case EState::ToggleLed_Start:
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
      state = EState::Idle;
      return;
    }
    break;

  case EState::SendStoryboard_Start:
    if (isFree)
    {
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[currDeviceIdx].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::CreateStoryboard;
      p->data[1] = 0; // timelines count, will be set later, we don't know yet
      *((millisec *)&p->data[2]) = storyboard.getDuration();

      uint32_t timelinesCount = 0;
      for (uint8_t i = 0; i < storyboard.getTimelinesCount(); i++)
      {
        auto t = storyboard.getTimelineByIdx(i);
        if (t->getOutputHardwareId() == enumeratedAddresses[currDeviceIdx].hardwareId)
        {
          p->data[6 + timelinesCount * 2 + 0] = t->getOutputId();
          p->data[6 + timelinesCount * 2 + 1] = t->getEntriesCount();
          timelinesCount += 1;

          // Never send more than 32 timelines, they won't fit.
          // TODO Check storyboard validity: can't have more than 32 timelines for device
          if (timelinesCount == 32)
          {
            break;
          }
        }
      }

      // Now we know
      p->data[1] = timelinesCount;
      p->header.data_size = 6 + timelinesCount * 2;

      *pTxAction = PTxAction::Send;
      nextTimelineIdxMaybeToSend = 0;
      state = EState::SendStoryboard_SendTimelines;
    }
    break;

  case EState::SendStoryboard_SendTimelines:
    if (isFree)
    {
      // search for the next timeline to send
      Timeline *t;
      uint8_t idxTimelineToSend;
      bool found = false;
      for (uint8_t i = nextTimelineIdxMaybeToSend; i < storyboard.getTimelinesCount(); i++)
      {
        t = storyboard.getTimelineByIdx(i);
        if (t->getOutputHardwareId() == enumeratedAddresses[currDeviceIdx].hardwareId)
        {
          found = true;
          idxTimelineToSend = i;
        }
      }

      if (!found)
      {
        if (currDeviceIdx == enumeratedAddressesCount - 1)
        {
          // No more devices, done
          state = EState::Idle;
        }
        else
        {
          currDeviceIdx += 1;
          state = EState::SendStoryboard_Start;
        }
      }
      else
      {
        p->header.control = 1;
        p->header.src_address = ringNetwork->getAddress();
        p->header.dst_address = enumeratedAddresses[currDeviceIdx].address;
        p->header.ttl = RingNetworkProtocol::ttl_max;
        p->data[0] = EMsgType::SetTimelineEntries;
        p->data[1] = t->getOutputId();
        p->data[2] = 0;
        // For now send only first 20 entries, don't bother with cycles
        auto entryCountToSend = Utils::min(20, t->getEntriesCount());
        p->data[3] = entryCountToSend;

        for (int i = 0; i < entryCountToSend; i++)
        {
          auto offset = 4 + i * 12;
          auto entry = t->getEntry(i);
          *((millisec *)&p->data[offset + 0]) = entry->time;
          *((int32_t *)&p->data[offset + 4]) = entry->value;
          *((millisec *)&p->data[offset + 8]) = entry->duration;
        }
        p->header.data_size = 4 + entryCountToSend * 12;

        *pTxAction = PTxAction::Send;

        // Stay in EState::SendStoryboard_SendTimelines state
        nextTimelineIdxMaybeToSend = idxTimelineToSend + 1;
      }
    }
    break;

  case EState::CheckStoryboard_Start:
    if (isFree)
    {
      p->header.data_size = 1;
      p->header.control = 1;
      p->header.src_address = ringNetwork->getAddress();
      p->header.dst_address = enumeratedAddresses[currDeviceIdx].address;
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = EMsgType::GetStoryboardChecksum;
      *pTxAction = PTxAction::Send;
      state = EState::CheckStoryboard_WaitCrc;
    }
    break;

  case EState::CheckStoryboard_WaitCrc:
    if (p->isDataPacket(ringNetwork->getAddress(), 1 + 4, EMsgType::TellStoryboardChecksum))
    {
      enumeratedAddresses[currDeviceIdx].crcReceived = p->getDataUInt32(1);

      if (currDeviceIdx == enumeratedAddressesCount - 1)
      {
        state = EState::Idle;
      }
      else
      {
        state = EState::CheckStoryboard_Start;
      }
    }
    break;
  }
}

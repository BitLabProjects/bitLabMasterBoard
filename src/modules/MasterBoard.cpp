#include "MasterBoard.h"

Serial serial(USBTX, USBRX);

MasterBoard::MasterBoard() : led(LED2),
                             state(EState::WaitAddressAssigned),
                             enumeratedAddressesCount(0),
                             freePacketsCount(0)
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
        serial.printf("%i", enumeratedAddresses[i]);
      }
      serial.printf("]\n");
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
        p->header.data_size > 0 &&
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
        enumeratedAddresses[enumeratedAddressesCount] = src_address;
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
      p->header.dst_address = enumeratedAddresses[0];
      p->header.ttl = RingNetworkProtocol::ttl_max;
      p->data[0] = 1;
      p->data[1] = ledState;
      *pTxAction = PTxAction::Send;
      state = EState::Idle;
      return;
    }
    break;
  }
}

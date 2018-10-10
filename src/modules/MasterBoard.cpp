#include "MasterBoard.h"

MasterBoard::MasterBoard(): led(LED2)
{
}

void MasterBoard::init(const bitLabCore *core)
{
  ringNetwork = (RingNetwork *)core->findModule("RingNetwork");
  if (ringNetwork != NULL)
  {
    ringNetwork->attachDataPacketReceived(callback(this, &MasterBoard::dataPacketReceived));
  }
}

void MasterBoard::mainLoop()
{
}

void MasterBoard::tick(millisec64 timeDelta)
{
}

void MasterBoard::dataPacketReceived(RingPacket* p, PTxAction* pTxAction)
{
  *pTxAction = PTxAction::SendFreePacket;

  //Data content specifies the message type in the first byte, then the following are based on the message type
  auto data_size = p->header.data_size;
  if (data_size < 1) return; //Too short

  auto msgType = p->data[0];
  if (msgType == 1) { //Set output
    if (data_size < 2) return; //Too short

    led = p->data[1];
    return;

  } else {
    // Unknown message type, discard
    return;
  }
}

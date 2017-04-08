#pragma once

struct BidMessage
{
  enum Phase {UnknowPhase = 0, NonBidPhase, FirstPhase, ModifyPhase};
  Phase phase;
  int serverTime;
  int endTime;
  int tradeablePrice;
  std::wstring notification;
  
  BidMessage() : phase(UnknowPhase), serverTime(0), endTime(0), tradeablePrice(0) {}
};

typedef void(*ReceivedBidMsg) (const BidMessage& bidMsg, void *arg);

class BidInfoMonitor
{
public:
  BidInfoMonitor(ReceivedBidMsg printBidMsgFunc, void *receivedBidMsgArg)
    : m_workThread(NULL)
    , m_divert(NULL)
    , m_receivedBidMsgFunc(printBidMsgFunc)
    , m_receivedBidMsgArg(receivedBidMsgArg)
  {}

  ~BidInfoMonitor() { Stop(); }

  void ThreadFunc();
  bool Start(const wchar_t *ip1, int port1, const wchar_t *ip2, int port2);
  bool Stop();
  bool isWorking() { return m_workThread && m_divert.load(); }

private:
  void *m_workThread;
  std::atomic<void*> m_divert;
  ReceivedBidMsg m_receivedBidMsgFunc;
  void *m_receivedBidMsgArg;
  BidMessage m_bidMessage;
};
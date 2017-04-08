#include "stdafx.h"
#include "BidInfoMonitor.h"

#define MAXBUF 0xFFFF
static unsigned char key[16] = "!@p!a";

static wchar_t *UTF8ToUTF16(const char* msg, int len)
{
  DWORD unicodeSize = MultiByteToWideChar(CP_UTF8, 0, msg, len, NULL, 0);
  wchar_t *unicodeMsg = (wchar_t*)malloc((unicodeSize + 1) * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, msg, -1, unicodeMsg, unicodeSize);
  unicodeMsg[unicodeSize] = L'\0';
  return unicodeMsg;
}

static wchar_t *DecodeBidInfo(const char* msg) {
  wchar_t *unicodeMsg = UTF8ToUTF16(msg, -1);

  // decode
  unsigned int origlen = (unsigned int)wcslen(unicodeMsg);
  unsigned int remainder = 0;
  unsigned int len = origlen;
  unsigned int seg = 7;
  if ((remainder = len % seg) != 0)
    len += seg - remainder;

  wchar_t *res = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
  for (unsigned int i = 0; i < seg; i++) {
    for (unsigned int j = 0, n = len / seg; j < n; j++) {
      if (i * n + j < origlen)
        res[i + j * seg] = unicodeMsg[i * n + j];
      else
        res[i + seg * j] = L' ';
    }
  }

  free(unicodeMsg);

  // trim right
  while (--len > 0 && res[len] == L' ')
    res[len] = L'\0';

  return res;
}

static int ParseTimeToSeconds(const wchar_t *strTime, bool hasSecond)
{
  if (!strTime || !*strTime || (hasSecond && wcslen(strTime) < 8) || (!hasSecond && wcslen(strTime) < 5))
    return 0;

  TCHAR buf[3] = { 0 };
  buf[0] = strTime[0];
  buf[1] = strTime[1];
  int hour = _wtoi(buf);
  buf[0] = strTime[3];
  buf[1] = strTime[4];
  int min = _wtoi(buf);
  int second = 0;
  if (hasSecond)
  {
    buf[0] = strTime[6];
    buf[1] = strTime[7];
    second = _wtoi(buf);
  }

  return (hour * 3600) + (min * 60) + second;
}

static void ParseNotification(wchar_t *msg, BidMessage& bidMsg)
{
  wchar_t phase = L'\0';
  wchar_t *msgArray[32] = { NULL };

  bidMsg.phase = BidMessage::UnknowPhase;
  bidMsg.serverTime = 0;
  bidMsg.endTime = 0;
  bidMsg.tradeablePrice = 0;
  bidMsg.notification.clear();

  // parse comma seperated message to array
  {
    int i = 0;
    wchar_t *ctx;
    for (wchar_t *token = wcstok(msg, L",", &ctx); token != NULL && i < _countof(msgArray); token = wcstok(NULL, L",", &ctx))
      msgArray[i++] = token;
  }

  if (!msgArray[1] || !(phase = *(msgArray[1])))
    return;

  if (phase == L'A') // first bid phase
  {
    bidMsg.phase = BidMessage::FirstPhase;
    bidMsg.serverTime = ParseTimeToSeconds(msgArray[13], true);
    bidMsg.endTime = ParseTimeToSeconds(msgArray[10], false);
    bidMsg.tradeablePrice = _wtoi(msgArray[5]);
  }
  else if (phase == L'B') // second bid phase
  {
    bidMsg.phase = BidMessage::ModifyPhase;
    bidMsg.serverTime = ParseTimeToSeconds(msgArray[13], true);
    bidMsg.endTime = ParseTimeToSeconds(msgArray[12], false);
    bidMsg.tradeablePrice = _wtoi(msgArray[14]);
  }
  else if (phase == L'C') // non-bid phase
  {
    bidMsg.phase = BidMessage::NonBidPhase;
    bidMsg.notification = msgArray[2];

    wchar_t *timePos = NULL;
    if (msgArray[2] && (timePos = wcsstr(msgArray[2], L"ǰʱ�䣺")))
      bidMsg.serverTime = ParseTimeToSeconds(timePos+4, true);
  }
}

static void MonitorThreadFunc(void* pArguments)
{
  if (BidInfoMonitor *pBidMonitor = (BidInfoMonitor*)pArguments)
    pBidMonitor->ThreadFunc();
}

void BidInfoMonitor::ThreadFunc()
{
  UINT8 packet[MAXBUF];
  UINT packet_len;
  WINDIVERT_ADDRESS addr;
  PWINDIVERT_IPHDR ip_header;
  PWINDIVERT_TCPHDR tcp_header;
  PVOID payload;
  UINT payload_len;
  while (m_divert.load())
  {
    memset(packet, 0, sizeof(packet));

    // Read a matching packet.
    // Parse TCP packet
    if (!WinDivertRecv(m_divert.load(), packet, sizeof(packet), &addr, &packet_len) && m_divert.load()
      || !WinDivertHelperParsePacket(packet, packet_len, &ip_header, NULL, NULL, NULL, &tcp_header, NULL, &payload, &payload_len)
      || payload_len <= 10 || ntohl(*(int*)payload) - ntohl(*(int*)((char*)payload + 6)) != 10)
      continue;

    ((char*)payload)[payload_len] = '\0';
    if (((char*)payload)[4] == 3 && ((char*)payload)[5] == 1)
    {
      wchar_t *decodedMsg = DecodeBidInfo((char*)payload + 10);
      ParseNotification(decodedMsg, m_bidMessage);
      free(decodedMsg);

      if (m_receivedBidMsgFunc)
        m_receivedBidMsgFunc(m_bidMessage, m_receivedBidMsgArg);
    }
  }
}

bool BidInfoMonitor::Start(const wchar_t *ip1, int port1, const wchar_t *ip2, int port2)
{
  char filterBuf[1024] = { 0 };
  bool server1Valid = ip1 && *ip1 && port1 > 1 && port1 < 65535;
  bool server2Valid = ip2 && *ip2 && port2 > 1 && port2 < 65535;
  if ((!server1Valid && !server2Valid) || m_workThread != NULL)
    return false;

  if (server1Valid && server2Valid)
  {
    snprintf(filterBuf, sizeof(filterBuf) - 1, "inbound and ip and ((ip.SrcAddr == %S and tcp.SrcPort == %d) or (ip.SrcAddr == %S and tcp.SrcPort == %d)) and tcp.PayloadLength > 0",
      ip1, port1, ip2, port2);
  }
  else
  {
    snprintf(filterBuf, sizeof(filterBuf) - 1, "inbound and ip and ip.SrcAddr == %S and tcp.SrcPort == %d and tcp.PayloadLength > 0", ip1, port1);
  }

  HANDLE divert = WinDivertOpen(filterBuf, WINDIVERT_LAYER_NETWORK, 1000, WINDIVERT_FLAG_SNIFF);
  if (divert == INVALID_HANDLE_VALUE)
  {
    DWORD dwLastError = GetLastError();
    if (dwLastError == ERROR_INVALID_PARAMETER)
      LOGE << "Invalid WinDivert filter synatax: " << filterBuf;
    else
      LOGE << "Failed to open WinDivert device, error = " << dwLastError;

    return false;
  }

  LOGI << "Start monitoring: " << filterBuf;

  m_divert = divert;
  m_workThread = (void*)_beginthread(MonitorThreadFunc, 0, this);
  return true;
}

bool BidInfoMonitor::Stop()
{
  HANDLE divert = m_divert.load();
  if (!m_workThread || !divert)
    return false;

  m_divert = NULL;
  WinDivertClose(divert);
  m_workThread = NULL;
  return true;
}

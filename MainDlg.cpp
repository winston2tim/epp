#include "stdafx.h"
#include "MainDlg.h"

#define TIMER_FORCESUBMIT 0x100

#define FOCUS_ON_PRICE    ClickBrowser(720, 250);
#define SUBMIT_PRICE      ClickBrowser(800, 250);
#define FOCUS_ON_CAPTCHA  ClickBrowser(740, 255);
#define SUBMIT_CAPTCHA    ClickBrowser(550, 335);

#define DEFAULT_SWF_OFFSET {1, 170}

static const wchar_t *script_swf_left = L"$(\"#testsocket\").position().left - $(document).scrollLeft()";
static const wchar_t *script_swf_top = L"$(\"#testsocket\").position().top - $(document).scrollTop()";

static POINT GetSwfOffset(CComPtr<IWebBrowser2>& webBrowser)
{
  POINT offset = DEFAULT_SWF_OFFSET;

  CComQIPtr<IDispatch> spDisp;
  CComQIPtr<IHTMLDocument2> pHTMLDoc;
  CComPtr<IHTMLWindow2> spHTMLWindow;
  CComDispatchDriver disp;
  CComVariant left, top;
  if (SUCCEEDED(webBrowser->get_Document(&spDisp)) && (pHTMLDoc = spDisp)
     && SUCCEEDED(pHTMLDoc->get_parentWindow(&spHTMLWindow)) && (disp = spHTMLWindow)
     && SUCCEEDED(disp.Invoke1(L"eval", &CComVariant(script_swf_left), &left))
     && SUCCEEDED(disp.Invoke1(L"eval", &CComVariant(script_swf_top), &top)))
  {
    offset.x = left.lVal;
    offset.y = top.lVal;
  }
  else
  {
    LOGW << "Failed to get offset of swf in DOM";
  }

  return offset;
}

bool CMainDlg::GetTradeServerFromCookie()
{
  CComQIPtr<IDispatch> spDisp;
  CComQIPtr<IHTMLDocument2> pHTMLDoc;
  CComBSTR cookie;
  LPWSTR pwCookie = NULL;
  wchar_t *clientIdPos, *endClientIdPos, *serverStartPos, *endPos;

  if (FAILED(m_pWebBrowser->get_Document(&spDisp)) || !(pHTMLDoc = spDisp) || FAILED(pHTMLDoc->get_cookie(&cookie))
      || !(pwCookie = (LPWSTR)cookie)
      || !(clientIdPos = wcsstr(pwCookie, L"clientId")) || !(endClientIdPos = wcschr(clientIdPos, L';'))
      || !(serverStartPos = wcsstr(pwCookie, L"tradeserver=")) || !(endPos = wcschr(serverStartPos + 12, L';')))
    return false;

  LOGI << "Got cookie: " << pwCookie;

  // parse clientId
  clientIdPos = wcschr(clientIdPos, L'=');
  if (clientIdPos)
  {
    ++clientIdPos;
    m_clientId.Format(_T("%.*s"), endClientIdPos - clientIdPos, clientIdPos);
    LOGI << "ClientId = " << m_clientId;
  }

  // Parse server
  serverStartPos += 12;
  *endPos = L'\0';
  UrlUnescapeW(serverStartPos, NULL, NULL, URL_UNESCAPE_INPLACE);

  bool ret = false;
  wchar_t *ctx;
  for (wchar_t *token = wcstok(serverStartPos, L",", &ctx); token != NULL; token = wcstok(NULL, L",", &ctx))
  {
    if (endPos = wcschr(token, L':'))
    {
      *(endPos++) = L'\0';
      if (token == serverStartPos)
      {
        m_strServer1IP = token;
        m_nServer1Port = _wtoi(endPos);
        ret = m_nServer1Port > 0;
      }
      else
      {
        m_strServer2IP = token;
        m_nServer2Port = _wtoi(endPos);
      }
    }
  }

  return ret;
}

BOOL CMainDlg::OnIdle()
{
  DoDataExchange(TRUE);

  bool bTradeServerValid = (!m_strServer1IP.IsEmpty() && m_strServer1IP.Compare(_T("0.0.0.0")) && m_nServer1Port > 1 && m_nServer1Port < 65535)
    || (!m_strServer2IP.IsEmpty() && m_strServer2IP.Compare(_T("0.0.0.0")) && m_nServer2Port > 1 && m_nServer2Port > 65535);

  if (!bTradeServerValid && GetTradeServerFromCookie())
  {
    bTradeServerValid = true;
    m_bidInfoMonitor.Start(m_strServer1IP, m_nServer1Port, m_strServer2IP, m_nServer2Port);
  }

  EnableDlgItem(IDC_CHECK_AUTOBID, bTradeServerValid && m_latestBidMsg.phase == BidMessage::ModifyPhase);
  EnableDlgItem(IDC_CHECK_SUBMITCAPTCHA, bTradeServerValid && m_latestBidMsg.phase == BidMessage::ModifyPhase);
  EnableDlgItem(IDC_CHECK_FORCESUBMITTIME, bTradeServerValid && m_latestBidMsg.phase == BidMessage::ModifyPhase);

  return TRUE;
}

BOOL CALLBACK EnumChildIEWndProc(HWND hWnd, LPARAM lParam)
{
  TCHAR className[128] = { 0 };
  GetClassName(hWnd, className, _countof(className) - 1);
  if (*className && !_tcscmp(className, _T("Internet Explorer_Server")))
  {
    *(HWND*)lParam = hWnd;
    return FALSE;
  }

  return TRUE;
}

void __stdcall CMainDlg::OnNavigateComplete2(IDispatch* pDisp, VARIANT* URL)
{
  HWND browserWnd = NULL;
  EnumChildWindows(GetDlgItem(IDC_BROWSER), EnumChildIEWndProc, (LPARAM)&browserWnd);
  m_browserWnd = browserWnd;

  GetDlgItem(IDC_COMBO_URL).EnableWindow(FALSE);
  GetDlgItem(IDC_BUTTON_BROWSE).EnableWindow(FALSE);

  LOGI << "OnNavigateComplete2";
}

LRESULT CMainDlg::OnInitDialog(HWND /*hWnd*/, LPARAM /*lParam*/)
{
  // center the dialog on the screen
  CenterWindow();

  DlgResize_Init(true, false);

  // set icons
  HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_EPP), IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
  SetIcon(hIcon, TRUE);
  HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_EPP), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
  SetIcon(hIconSmall, FALSE);

  // Set font for remaining time
  CFont font;
  font.CreateFont(-20, -10, 0, 0, 400,
    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
    DEFAULT_QUALITY, FF_DONTCARE, _T("Arial"));
  HFONT f = font.Detach();
  CStatic(GetDlgItem(IDC_STATIC_REMAIN_TIME)).SetFont(f);
  CStatic(GetDlgItem(IDC_STATIC_TRADEABLE_PRICE)).SetFont(f);
  CStatic(GetDlgItem(IDC_STATIC_SERVERTIME)).SetFont(f);

  // Get IWebBrowser2 interface
  CAxWindow wndBrowser = GetDlgItem(IDC_BROWSER);
  wndBrowser.QueryControl(&m_pWebBrowser);

  // Set spin buttons
  UDACCEL udaccel = {0, 100};
  CUpDownCtrl(GetDlgItem(IDC_SPIN_AUTOBIDTIME)).SetRange(1, 1800);
  CUpDownCtrl bidSpinCtrl(GetDlgItem(IDC_SPIN_AUTOPRICE));
  bidSpinCtrl.SetRange(0, 10000);
  bidSpinCtrl.SetAccel(1, &udaccel);

  CUpDownCtrl captchaPriceSpinCtrl(GetDlgItem(IDC_SPIN_CAPTCAHPRICE));
  captchaPriceSpinCtrl.SetRange(0, 10000);
  captchaPriceSpinCtrl.SetAccel(1, &udaccel);

  CUpDownCtrl(GetDlgItem(IDC_SPIN_FORCESUBMITTIME)).SetRange(1, 1800);

  // Set log level
  CComboBox(GetDlgItem(IDC_COMBO_LOG)).SetCurSel(plog::info);

  // register object for message filtering
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  pLoop->AddMessageFilter(this);
  pLoop->AddIdleHandler(this);

  DoDataExchange();
  return TRUE;
}

void CMainDlg::OnDestroy()
{
  if (m_pWebBrowser)
    m_pWebBrowser.Release();
}

HBRUSH CMainDlg::OnCtlColorStatic(CDCHandle dc, CStatic wndStatic)
{
  if (GetDlgItem(IDC_STATIC_REMAIN_TIME) == (HWND)wndStatic
    || GetDlgItem(IDC_STATIC_TRADEABLE_PRICE) == (HWND)wndStatic
    || GetDlgItem(IDC_STATIC_SERVERTIME) == (HWND)wndStatic)
  {
    RECT rect;
    wndStatic.GetClientRect(&rect);
    COLORREF bkColor = ::GetSysColor(COLOR_3DFACE);
    dc.FillSolidRect(&rect, bkColor);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(0xFF, 0x00, 0x00));
    return (HBRUSH)::GetStockObject(NULL_BRUSH);
  }

  return 0;
}

LRESULT CMainDlg::OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  DoDataExchange(TRUE);
  CComVariant v;
  if (!m_strBrowserURL.IsEmpty() && m_pWebBrowser)
    m_pWebBrowser->Navigate(CComBSTR(m_strBrowserURL), &v, &v, &v, &v);

  return 0;
}

static float GetLocalTimeAsSeconds()
{
  SYSTEMTIME localTime = { 0 };
  ::GetLocalTime(&localTime);
  return (localTime.wHour * 3600.0f) + (localTime.wMinute * 60.0f) + localTime.wSecond + (localTime.wMilliseconds / 1000.0f);
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  CloseDialog(wID);
  return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
  DestroyWindow();
  ::PostQuitMessage(nVal);
}

void CMainDlg::OnSysCommand(UINT nID, CPoint point)
{
  SetMsgHandled(FALSE);
}

void CMainDlg::ClickBrowser(int x, int y)
{
  if (m_browserWnd)
  {
    POINT offset = GetSwfOffset(m_pWebBrowser);
    if (offset.x < LONG_MAX && offset.y < LONG_MAX)
    {
      x += offset.x;
      y += offset.y;
      m_browserWnd.PostMessage(WM_LBUTTONDOWN, MK_LBUTTON, x | (y << 16));
      m_browserWnd.PostMessage(WM_LBUTTONUP, 0, x | (y << 16));
    }
  }
}

void CMainDlg::SendBrowserChars(LPCTSTR text)
{
  if (m_browserWnd)
  {
    for (LPCTSTR c = text; *c; c++)
      m_browserWnd.PostMessage(WM_CHAR, *c, 0);
  }
}

void CMainDlg::ClearBrowserChars(int nRepeat)
{
  if (m_browserWnd)
  {
    for (int i = 0; i < nRepeat; i++)
      m_browserWnd.PostMessage(WM_KEYDOWN, VK_BACK, 0);
  }
}

void CMainDlg::AutoBid(int price)
{
  TCHAR priceText[32] = { 0 };

  m_nPendingPrice = price;

  m_browserWnd.SetActiveWindow();
  m_browserWnd.SetFocus();

  // click price input box
  FOCUS_ON_PRICE

  // clear price input box
  ClearBrowserChars(6);

  // send price
  _sntprintf(priceText, _countof(priceText), _T("%d"), price);
  SendBrowserChars(priceText);

  // click bid button
  SUBMIT_PRICE

  LOGI << "Auto bidded, price = " << price;
}

LRESULT CMainDlg::OnIEEnterKey(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
  if (!m_bAutoSubmitCaptcha && m_latestBidMsg.phase == BidMessage::ModifyPhase && m_nPendingPrice >= m_latestBidMsg.tradeablePrice)
  {
    if (m_nPendingPrice - m_latestBidMsg.tradeablePrice <= m_nSubmitCaptcahPrice)
    {
      LOGI << "Submit captcha, my price = " << m_nPendingPrice << ", tradeable price = " << m_latestBidMsg.tradeablePrice;
      m_browserWnd.SetActiveWindow();
      m_browserWnd.SetFocus();
      SUBMIT_CAPTCHA
      m_nPendingPrice = 0;
    }
    else
    {
      m_bAutoSubmitCaptcha = true;
      DoDataExchange(FALSE, IDC_CHECK_SUBMITCAPTCHA);
    }
  }
  else
  {
    // click the OK button to submit CAPTCHA and price
    m_browserWnd.SetActiveWindow();
    m_browserWnd.SetFocus();
    SUBMIT_CAPTCHA
  }

  return 0;
}

void CMainDlg::OnTimer(UINT_PTR nIDEvent)
{
  if (nIDEvent == TIMER_FORCESUBMIT)
  {
    // click the OK button to submit CAPTCHA and price
    m_browserWnd.SetActiveWindow();
    m_browserWnd.SetFocus();
    SUBMIT_CAPTCHA

    LOGI << "Force price submitted!";
    KillTimer(TIMER_FORCESUBMIT);
    m_bAutoSubmitByTime = false;
    DoDataExchange(FALSE, IDC_CHECK_FORCESUBMITTIME);
    GetDlgItem(IDC_EDIT_FORCESUBMITTIME).EnableWindow(TRUE);
  }
}

LRESULT CMainDlg::OnBnClickedForceSubmitByTime(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  DoDataExchange(TRUE);
  if (m_bAutoSubmitByTime)
  {
    float elapse = (float)m_latestBidMsg.endTime - GetLocalTimeAsSeconds() - m_fFinalSubmitTime;
    if (elapse > 0)
    {
      SetTimer(TIMER_FORCESUBMIT, (UINT)(elapse*1000.0f), NULL);
      GetDlgItem(IDC_EDIT_FORCESUBMITTIME).EnableWindow(FALSE);
      LOGI << "Start forece submit timer, elapse = " << (UINT)(elapse*1000.0f) << " ms";
    }
    else
    {
      m_bAutoSubmitByTime = false;
      DoDataExchange(FALSE, IDC_CHECK_FORCESUBMITTIME);
    }
  }
  else
  {
    KillTimer(TIMER_FORCESUBMIT);
    GetDlgItem(IDC_EDIT_FORCESUBMITTIME).EnableWindow(TRUE);
  }

  return 0;
}

LRESULT CMainDlg::OnTest(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  DoDataExchange(TRUE);
  return 0;
}

LRESULT CMainDlg::OnReceivedBidMsg(WPARAM wParam, LPARAM lParam)
{
  if (BidMessage *pBidMsg = (BidMessage*)lParam)
  {
    m_latestBidMsg = *pBidMsg;
    delete pBidMsg;
  }

  if (!m_hWnd)
    return 0;

  DoDataExchange(TRUE);

  int remainingSeconds = m_latestBidMsg.endTime - m_latestBidMsg.serverTime;
  float serverDelay = (float)m_latestBidMsg.serverTime - GetLocalTimeAsSeconds();
  HandleNewPriceMessage();

  if (m_latestBidMsg.phase == BidMessage::ModifyPhase && remainingSeconds >= 0 && remainingSeconds <= 30)
    LOGI << "Remain time: " << remainingSeconds << " s, price: " << m_latestBidMsg.tradeablePrice;

  CString strMsg;

  if (m_latestBidMsg.serverTime > 0)
  {
    int hour = m_latestBidMsg.serverTime / 3600;
    int min = (m_latestBidMsg.serverTime - hour * 3600) / 60;
    int sec = m_latestBidMsg.serverTime % 60;
    strMsg.Format(_T("%02d:%02d:%02d (%.02f)"), hour, min, sec, serverDelay);
    GetDlgItem(IDC_STATIC_SERVERTIME).SetWindowText(strMsg);
  }

  if (remainingSeconds >= 0)
  {
    strMsg.Format(_T("%d s"), remainingSeconds);
    GetDlgItem(IDC_STATIC_REMAIN_TIME).SetWindowText(strMsg); // remaining time
  }

  if (m_latestBidMsg.tradeablePrice > 0)
  {
    strMsg.Format(_T("%d"), m_latestBidMsg.tradeablePrice);
    GetDlgItem(IDC_STATIC_TRADEABLE_PRICE).SetWindowText(strMsg);
  }

  return 0;
}

bool CMainDlg::HandleNewPriceMessage()
{
  if (m_latestBidMsg.phase != BidMessage::ModifyPhase || m_latestBidMsg.endTime <= 0)
    return true;

  if (m_bAutoSubmitCaptcha && m_nPendingPrice >= m_latestBidMsg.tradeablePrice && m_nPendingPrice - m_latestBidMsg.tradeablePrice <= m_nSubmitCaptcahPrice)
  {
    LOGI << "Submit captcha, my price = " << m_nPendingPrice << ", tradeable price = " << m_latestBidMsg.tradeablePrice;

    m_browserWnd.SetActiveWindow();
    m_browserWnd.SetFocus();
    SUBMIT_CAPTCHA

    m_nPendingPrice = 0;
    m_bAutoSubmitCaptcha = false;
    DoDataExchange(FALSE, IDC_CHECK_SUBMITCAPTCHA);
  }

  if (m_bAutoBid && (m_latestBidMsg.serverTime - m_latestBidMsg.endTime) * 1000 + (m_fAutoBidTime * 1000) > -10)
  {
    LOGI << "Time to bid, servertime = " << m_latestBidMsg.serverTime;
    AutoBid(m_latestBidMsg.tradeablePrice + m_nAutoBidExtraPrice);
    m_bAutoBid = false;
    DoDataExchange(FALSE, IDC_CHECK_AUTOBID);
  }


  return true;
}

LRESULT CMainDlg::OnReceivedPriceCode(WPARAM wParam, LPARAM lParam)
{
  if (std::wstring *pPriceCode = (std::wstring*)lParam)
  {
    m_latestPriceCode = pPriceCode->c_str();
    delete pPriceCode;
  }

  return 0;
}

void CallBackReceivedBidMsg(const BidMessage& bidMsg, void *arg)
{
  CMainDlg* pMainDlg = NULL;
  if ((pMainDlg = (CMainDlg*)arg) && pMainDlg->m_hWnd)
    pMainDlg->PostAction(&CMainDlg::OnReceivedBidMsg, NULL, (LPARAM)new BidMessage(bidMsg));
}

LRESULT CMainDlg::OnCbnSelchangeLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  int nCurSel = CComboBox(GetDlgItem(IDC_COMBO_LOG)).GetCurSel();
  plog::Logger<PLOG_DEFAULT_INSTANCE>* logger = plog::get();
  if (logger && nCurSel >= plog::none && nCurSel <= plog::verbose)
    logger->setMaxSeverity(plog::Severity(nCurSel));
  return 0;
}

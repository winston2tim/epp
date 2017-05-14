// maindlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////
#pragma once
#include "resource.h"
#include "BidInfoMonitor.h"

#define WM_POSTCALLBACK WM_USER+0x123

void CallBackReceivedBidMsg(const BidMessage& bidMsg, void *arg);

class CMainDlg
  : public CAxDialogImpl<CMainDlg>
  , public CMessageFilter
  , public CIdleHandler
  , public CWinDataExchange<CMainDlg>
  , public CDialogResize<CMainDlg>
  , public IDispEventImpl<IDC_BROWSER, CMainDlg>
{
public:
  enum { IDD = IDD_MAIN_DIALOG };

	CMainDlg()
    : m_nServer1Port(8300)
    , m_nServer2Port(8300)
    , m_bAutoBid(false)
    , m_fAutoBidTime(15.0f)
    , m_nAutoBidExtraPrice(1000)
    , m_nPendingPrice(0)
    , m_bAutoSubmitCaptcha(false)
    , m_nSubmitCaptcahPrice(300)
    , m_bAutoSubmitByTime(false)
    , m_fFinalSubmitTime(3.5f)
    , m_bidInfoMonitor(CallBackReceivedBidMsg, this)
	{
	}

  // Post call back function in message queue, simulate asynchrous call.
  typedef LRESULT(CMainDlg::*ActionFunc)(WPARAM, LPARAM);
  struct PostActionData
  {
    ActionFunc func;
    WPARAM wParam;
    LPARAM lParam;
  };

  void PostAction(ActionFunc actionFunc, WPARAM wParam, LPARAM lParam)
  {
    PostActionData *postActionData = new PostActionData;
    postActionData->func = actionFunc;
    postActionData->wParam = wParam;
    postActionData->lParam = lParam;
    PostMessage(WM_POSTCALLBACK, NULL, (LPARAM)postActionData);
  }

  LRESULT OnReceivedBidMsg(WPARAM wParam, LPARAM lParam);
  LRESULT OnReceivedPriceCode(WPARAM wParam, LPARAM lParam);

  virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
    // skip 'escape' key to avoid exit the main dialog window accidently.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
      return TRUE;

    // handle 'enter' key on IE browser window.
    if (pMsg && pMsg->hwnd == (HWND)m_browserWnd && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
      PostAction(&CMainDlg::OnIEEnterKey, pMsg->wParam, pMsg->lParam);

    // translate accelerator key in IE browser.
    CComQIPtr<IOleInPlaceActiveObject> pIOIPAO;
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST
        && (pIOIPAO = m_pWebBrowser) && !pIOIPAO->TranslateAccelerator(pMsg))
        return TRUE;

    return ::IsDialogMessage(m_hWnd, pMsg);
	}

  bool HandleNewPriceMessage();

  virtual BOOL OnIdle();
 
  BEGIN_MSG_MAP_EX(CMainDlg)
    CHAIN_MSG_MAP(CAxDialogImpl<CMainDlg>)
    MSG_WM_INITDIALOG(OnInitDialog)
    MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER(IDC_BUTTON_BROWSE, OnBrowse)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
    MSG_WM_TIMER(OnTimer)
    COMMAND_HANDLER(IDC_CHECK_FORCESUBMITTIME, BN_CLICKED, OnBnClickedForceSubmitByTime)
    MESSAGE_HANDLER(WM_POSTCALLBACK, OnPostAction)
    COMMAND_HANDLER(IDC_COMBO_LOG, CBN_SELCHANGE, OnCbnSelchangeLog)
    CHAIN_MSG_MAP(CDialogResize<CMainDlg>)
  END_MSG_MAP()

  BEGIN_DDX_MAP(CMainDlg)
    DDX_TEXT(IDC_COMBO_URL, m_strBrowserURL)
    DDX_CHECK(IDC_CHECK_AUTOBID, m_bAutoBid)
    DDX_FLOAT_RANGE(IDC_EDIT_AUTOBIDTIME, m_fAutoBidTime, .0f, 1800.0f)
    DDX_UINT_RANGE(IDC_EDIT_AUTOPRICE, m_nAutoBidExtraPrice, 0, 100000)
    DDX_CHECK(IDC_CHECK_SUBMITCAPTCHA, m_bAutoSubmitCaptcha)
    DDX_UINT_RANGE(IDC_EDIT_CAPTCHAPRICE, m_nSubmitCaptcahPrice, 0, 100000)
    DDX_CHECK(IDC_CHECK_FORCESUBMITTIME, m_bAutoSubmitByTime)
    DDX_FLOAT_RANGE(IDC_EDIT_FORCESUBMITTIME, m_fFinalSubmitTime, .0f, 1800.0f)
  END_DDX_MAP()

  BEGIN_DLGRESIZE_MAP(CMainDlg)
    DLGRESIZE_CONTROL(IDC_COMBO_URL, DLSZ_SIZE_X)
    DLGRESIZE_CONTROL(IDC_BUTTON_BROWSE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_BROWSER, DLSZ_SIZE_X|DLSZ_SIZE_Y)
    DLGRESIZE_CONTROL(IDC_STATIC_SERVERTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TRADEABLE_PRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_GROUP_PARAMETER, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_REMAIN_TIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_CHECK_AUTOBID, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_EDIT_AUTOBIDTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_SPIN_AUTOBIDTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_EDIT_AUTOPRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_SPIN_AUTOPRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_CHECK_SUBMITCAPTCHA, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_EDIT_CAPTCHAPRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_SPIN_CAPTCAHPRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_MINPRICE, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_REMAINTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_SECOND, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_REACH, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_WHEN, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_CHECK_FORCESUBMITTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_EDIT_FORCESUBMITTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_SPIN_FORCESUBMITTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_FORCESUBMITTIME, DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_STATIC_TEXT_LOG, DLSZ_MOVE_X|DLSZ_MOVE_Y)
    DLGRESIZE_CONTROL(IDC_COMBO_LOG, DLSZ_MOVE_X|DLSZ_MOVE_Y)
  END_DLGRESIZE_MAP()

  BEGIN_SINK_MAP(CMainDlg)
    SINK_ENTRY(IDC_BROWSER, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
  END_SINK_MAP()

private:
  CComPtr<IWebBrowser2> m_pWebBrowser;
  CWindow m_browserWnd;
  CString m_strBrowserURL;

  CString m_clientId;
  CString m_strServer1IP;
  int m_nServer1Port;
  CString m_strServer2IP;
  int m_nServer2Port;

  bool m_bAutoBid;
  float m_fAutoBidTime;
  int m_nAutoBidExtraPrice;
  int m_nPendingPrice;
  bool m_bAutoSubmitCaptcha;
  int m_nSubmitCaptcahPrice;
  bool m_bAutoSubmitByTime;
  float m_fFinalSubmitTime;

  BidInfoMonitor m_bidInfoMonitor;
  BidMessage m_latestBidMsg;
  CString m_latestPriceCode;

  void __stdcall OnNavigateComplete2(IDispatch* pDisp, VARIANT* URL);
  //void __stdcall OnEventDocumentComplete(IDispatch* /*pDisp*/, VARIANT* URL);

  LRESULT OnInitDialog(HWND /*hWnd*/, LPARAM /*lParam*/);
  void OnDestroy();
  HBRUSH OnCtlColorStatic(CDCHandle dc, CStatic wndStatic);
  LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  void OnSysCommand(UINT nID, CPoint point);
  void OnTimer(UINT_PTR nIDEvent);
  LRESULT OnBnClickedForceSubmitByTime(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnTest(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnIEEnterKey(WPARAM wParam, LPARAM lParam);
  LRESULT OnCbnSelchangeLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

  LRESULT OnPostAction(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
  {
    LRESULT res = 0;
    PostActionData *postActionData = (PostActionData*)lParam;
    if (postActionData)
    {
      res = (this->*(postActionData->func))(postActionData->wParam, postActionData->lParam);
      delete postActionData;
    }
    return res;
  }

  bool GetTradeServerFromCookie();
  void AutoBid(int price);
  void CloseDialog(int nVal);

  void EnableDlgItem(int nID, BOOL bEnable = TRUE) { CWindow(GetDlgItem(nID)).EnableWindow(bEnable); }
  void ClickBrowser(int x, int y);
  void ClearBrowserChars(int nRepeat);
  void SendBrowserChars(LPCTSTR text);
};

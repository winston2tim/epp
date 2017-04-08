// EPP.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "MainDlg.h"

CAppModule _Module;

int Run(LPTSTR /*lpCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
  CMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);

  plog::init(plog::info, "epp.log", 10000000, 5);
  CMainDlg dlgMain;

  if (dlgMain.Create(NULL) == NULL)
  {
    ATLTRACE(_T("Main dialog creation failed!\n"));
    return 0;
  }

  dlgMain.ShowWindow(nCmdShow);

  int nRet = theLoop.Run();

  _Module.RemoveMessageLoop();
  return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int nCmdShow)
{
  INITCOMMONCONTROLSEX iccx;
  iccx.dwSize = sizeof(iccx);
  iccx.dwICC = ICC_BAR_CLASSES;	// change to support other controls
  ::InitCommonControlsEx(&iccx);

  _Module.Init(NULL, hInstance);
  int nRet = Run(lpCmdLine, nCmdShow);
  _Module.Term();
  return nRet;
}

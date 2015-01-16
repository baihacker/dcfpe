#include "dpe_service/main/dpe_service_resource.h"

#ifndef min
#define ADD_MIN_MAX 1
#define min(x, y) ((x) < (y)) ? (x) : (y)
#define max(x, y) ((x) > (y)) ? (x) : (y)
#else
#define ADD_MIN_MAX 0
#endif

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctl.h>
#include <atlctrls.h>
#include <atlframe.h>

#if ADD_MIN_MAX
#undef min
#undef max
#endif

extern CAppModule _Module;

class CDPEShellDlg : public CDialogImpl<CDPEShellDlg>, public CUpdateUI<CDPEShellDlg>,
    public CMessageFilter, public CIdleHandler
{
public:
  enum { IDD = IDD_DPE_SHELL_DLG };
  CDPEShellDlg()
  {

  }
  ~CDPEShellDlg()
  {

  }
  virtual BOOL PreTranslateMessage(MSG* pMsg)
  {
    return CWindow::IsDialogMessage(pMsg);
  }

  virtual BOOL OnIdle()
  {
    return FALSE;
  }

  BEGIN_UPDATE_UI_MAP(CDPEShellDlg)
  END_UPDATE_UI_MAP()

  BEGIN_MSG_MAP(CDPEShellDlg)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    COMMAND_ID_HANDLER(IDOK, OnOK)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

  LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

  LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

  LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  void CloseDialog(int nVal);

private:

};

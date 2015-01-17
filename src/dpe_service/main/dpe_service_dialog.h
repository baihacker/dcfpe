#ifndef DPE_SERVICE_MAIN_DPE_SERVICE_DIALOG_H_
#define DPE_SERVICE_MAIN_DPE_SERVICE_DIALOG_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/dpe_service.h"
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
extern CAppModule _Module;

#if ADD_MIN_MAX
#undef min
#undef max
#endif

namespace ds
{
class CDPEServiceDlg :
    public CDialogImpl<CDPEServiceDlg>,
    public CUpdateUI<CDPEServiceDlg>,
    public CMessageFilter,
    public CIdleHandler,
    public base::RefCounted<CDPEServiceDlg>
{
public:
  enum { IDD = IDD_DPE_SERVICE_DLG };
  CDPEServiceDlg(DPEService* dpe) : dpe_(dpe)
  {

  }
  ~CDPEServiceDlg()
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

  BEGIN_UPDATE_UI_MAP(CDPEServiceDlg)
  END_UPDATE_UI_MAP()

  BEGIN_MSG_MAP(CDPEServiceDlg)
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

  void  UpdateServerList();
private:
  DPEService*  dpe_;
  CListViewCtrl   m_ServerListCtrl;
};
}

#endif
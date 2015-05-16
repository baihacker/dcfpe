#ifndef DPE_SERVICE_MAIN_DPE_SERVICE_DIALOG_H_
#define DPE_SERVICE_MAIN_DPE_SERVICE_DIALOG_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/dpe_service.h"
#include "dpe_service/main/dpe_service_resource.h"

#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_project.h"
#include "dpe_service/main/dpe_model/dpe_compiler.h"
#include "dpe_service/main/dpe_model/dpe_scheduler.h"

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
    public base::RefCounted<CDPEServiceDlg>,
    public DPECompilerHost,
    public DPESchedulerHost,
    public DPEGraphObserver
{
public:
  enum { IDD = IDD_DPE_SERVICE_DLG };
  CDPEServiceDlg(DPEService* dpe) : dpe_(dpe), job_state_(DPE_JOB_STATE_PREPARE)
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
    COMMAND_HANDLER(IDC_SELECT_PROJECT, BN_CLICKED, OnBnClickedSelectProject)
    COMMAND_HANDLER(IDC_COMPILE_PROJECT_BTN, BN_CLICKED, OnBnClickedCompileProject)
    COMMAND_HANDLER(IDC_RUN_OR_STOP_PROJECT_BTN, BN_CLICKED, OnBnClickedRunOrStopProject)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

  LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

  LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

  LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  void CloseDialog(int nVal);

  void  OnServerListUpdated();
  
  LRESULT OnBnClickedSelectProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnBnClickedCompileProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnBnClickedRunOrStopProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

  void  AppendCompilerOutput();

  void  LoadProject(const base::FilePath& path);
  void  OnCompileError();
  void  OnCompileSuccess();
  void  OnRunningError();
  void  OnRunningSuccess();
private:
  DPEService*  dpe_;
  scoped_refptr<DPEProject>                     dpe_project_;
  scoped_refptr<DPECompiler>                    dpe_compiler_;
  scoped_refptr<DPEScheduler>                   dpe_scheduler_;
  int32_t                                       job_state_;
private:
  CListViewCtrl   m_ServerListCtrl;
  CEdit           m_ProjectInfo;
};
}

#endif
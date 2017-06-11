#include "dpe_service/main/dpe_service_dialog.h"

#include <atldlgs.h>
namespace ds
{
LRESULT CDPEServiceDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  // center the dialog on the screen
  CenterWindow();

  // set icons
  HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
  SetIcon(hIcon, TRUE);
  HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
  SetIcon(hIconSmall, FALSE);

  // register object for message filtering and idle updates
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  ATLASSERT(pLoop != NULL);
  pLoop->AddMessageFilter(this);
  pLoop->AddIdleHandler(this);

  UIAddChildWindowContainer(m_hWnd);
  
  {
    m_ServerListCtrl.Attach(GetDlgItem(IDC_DPE_SERVER_LIST));
    DWORD dwStyle = m_ServerListCtrl.GetExtendedListViewStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
    m_ServerListCtrl.SetExtendedListViewStyle(dwStyle);
    
    m_ServerListCtrl.InsertColumn(0, L"Address");
    m_ServerListCtrl.SetColumnWidth(0, 150);
    
    m_ServerListCtrl.InsertColumn(1, L"Response time");
    m_ServerListCtrl.SetColumnWidth(1, 100);
    
    //UpdateServerList();
  }

  m_ProjectInfo.Attach(GetDlgItem(IDC_EDIT_PROJECT_INFO));
  m_ProjectInfo.SetReadOnly(TRUE);
  
  if (!dpe_->last_open_.empty())
  {
    //LoadProject(base::FilePath(base::UTF8ToNative(dpe_->last_open_)));
  }
  return TRUE;
}

LRESULT CDPEServiceDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  // unregister message filtering and idle updates
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  ATLASSERT(pLoop != NULL);
  pLoop->RemoveMessageFilter(this);
  pLoop->RemoveIdleHandler(this);
  return 0;
}

LRESULT CDPEServiceDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  CloseDialog(wID);
  return 0;
}

LRESULT CDPEServiceDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  CloseDialog(wID);
  return 0;
}

void CDPEServiceDlg::CloseDialog(int nVal)
{
  //DestroyWindow();
  //::PostQuitMessage(nVal);
  dpe_->WillStop();
}

void  CDPEServiceDlg::OnServerListUpdated()
{
  auto& mgr = dpe_->GetNodeManager();
  auto& nl = mgr.NodeList();
  
  const int n = static_cast<int>(nl.size());
  const int old_cnt = m_ServerListCtrl.GetItemCount();
  
  int selected_server_id = -1;
  int selected_index = -1;
  LVITEM item;
  if (m_ServerListCtrl.GetSelectedItem(&item))
  {
    int idx = item.iItem;
    selected_server_id = (int)m_ServerListCtrl.GetItemData(idx);
  }
  for (int i = 0; i < n; ++i)
  {
    auto& item = nl[i];
    if (i >= old_cnt)
    {
      m_ServerListCtrl.InsertItem(i, L"");
    }
    
    m_ServerListCtrl.SetItemText(i, 0, base::UTF8ToNative(item->server_address_).c_str());
    
    if (item->response_count_ == 0)
    {
      m_ServerListCtrl.SetItemText(i, 1, L"N/A");
    }
    else
    {
      double tmp = item->response_time_.InSecondsF() / item->response_count_;
      m_ServerListCtrl.SetItemText(i, 1, base::StringPrintf(L"%.3f", tmp).c_str());
    }
    
    m_ServerListCtrl.SetItemData(i, (DWORD_PTR)item->node_id_);
    
    if (selected_server_id == item->node_id_ || selected_server_id == -1 && i == 0)
    {
      CEdit pa(GetDlgItem(IDC_EDIT_SERVER_PHISICAL_ADDRESS));
      pa.SetWindowText(base::UTF8ToNative(item->pa_).c_str());
      selected_index = i;
    }
  }
  
  int extra = old_cnt - n;
  while (extra > 0)
  {
    m_ServerListCtrl.DeleteItem(m_ServerListCtrl.GetItemCount() - 1);
    --extra;
  }
  
  if (selected_index >= 0)
  {
    m_ServerListCtrl.SelectItem(selected_index);
  }
}

LRESULT CDPEServiceDlg::OnBnClickedSelectProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (job_state_ == DPE_JOB_STATE_COMPILING)
  {
    m_ProjectInfo.AppendText(
        L"Error : Another project is compiling.\r\n"
      );
    return 0;
  }
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    m_ProjectInfo.AppendText(
        L"Error : Another project is running.\r\n"
      );
    return 0;
  }
  CFileDialog dlg(TRUE, NULL, NULL, OFN_SHAREAWARE, L"Dpe Files (*.dpe)\0*.dpe\0");
  wchar_t strBuffer[65535] = {0};
  dlg.m_ofn.lpstrFile = strBuffer;
  dlg.m_ofn.nMaxFile = 65535;
  if (dlg.DoModal() == IDOK)
  {
    LoadProject(base::FilePath(dlg.m_ofn.lpstrFile));
  }
  dpe_->test_action();
  return 0;
}

void  CDPEServiceDlg::LoadProject(const base::FilePath& path)
{
  if (dpe_project_ = DPEProject::FromFile(path))
  {
    CEdit e(GetDlgItem(IDC_EDIT_PROJECT_PATH));
    e.SetWindowText(path.value().c_str());
    m_ProjectInfo.AppendText(
      base::StringPrintf(L"Load %ls successful.\r\n", dpe_project_->job_name_.c_str()).c_str()
    );
    m_ProjectInfo.AppendText(
      base::StringPrintf(L"Source path :\r\n%ls\r\n", dpe_project_->source_path_.value().c_str()).c_str()
    );
    m_ProjectInfo.AppendText(
      base::StringPrintf(L"Worker path :\r\n%ls\r\n", dpe_project_->worker_path_.value().c_str()).c_str()
    );
    m_ProjectInfo.AppendText(
      base::StringPrintf(L"Sink path :\r\n%ls\r\n", dpe_project_->sink_path_.value().c_str()).c_str()
    );
    //m_ProjectInfo.AppendText((L"Load " + dpe_project_->job_name_ + L" successful!\r\n").c_str());
    dpe_->SetLastOpen(path);
    job_state_ = DPE_JOB_STATE_PREPARE;
  }
  else
  {
    m_ProjectInfo.AppendText(
      base::StringPrintf(L"Load %ls failed.\r\n", path.value()).c_str()
    );
  }
}

LRESULT CDPEServiceDlg::OnBnClickedCompileProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (!dpe_project_)
  {
    m_ProjectInfo.AppendText(L"Error : Project does not exist.\r\n");
    return 0;
  }

  if (job_state_ == DPE_JOB_STATE_COMPILING)
  {
    m_ProjectInfo.AppendText(L"Error : Another project is compiling.\r\n");
    return 0;
  }
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    m_ProjectInfo.AppendText(L"Error : Another project is running.\r\n");
    return 0;
  }

  dpe_compiler_ = new DPECompiler(this, dpe_);
  if (dpe_compiler_->StartCompile(dpe_project_))
  {
    job_state_ = DPE_JOB_STATE_COMPILING;
    m_ProjectInfo.AppendText(L"Info : Compiling.\r\n");
  }
  else
  {
    job_state_ = DPE_JOB_STATE_FAILED;
    m_ProjectInfo.AppendText(L"Error : Can not compile.\r\n");
  }
  return 0;
}

LRESULT CDPEServiceDlg::OnBnClickedRunOrStopProject(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (!dpe_project_)
  {
    m_ProjectInfo.AppendText(L"Error : Project does not exist.\r\n");
    return 0;
  }

  if (job_state_ == DPE_JOB_STATE_PREPARE ||
      job_state_ == DPE_JOB_STATE_COMPILING ||
      job_state_ == DPE_JOB_STATE_COMPILE_ERROR)
  {
    m_ProjectInfo.AppendText(L"Error : Please compile project.\r\n");
    return 0;
  }
  else if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
  }
  else if (job_state_ == DPE_JOB_STATE_COMPILED ||
            job_state_ == DPE_JOB_STATE_FINISH ||
            job_state_ == DPE_JOB_STATE_FAILED)
  {
    std::vector<std::pair<bool, std::string> >    servers;
    auto& mgr = dpe_->GetNodeManager();
    auto& nl = mgr.NodeList();
    for (auto iter: nl)// if (!iter->is_local_)
    {
      servers.emplace_back(iter->is_local_, iter->server_address_);
    }
    dpe_scheduler_ = new DPEScheduler(this);
    if (dpe_scheduler_->Run(servers, dpe_project_, dpe_compiler_))
    {
      job_state_ = DPE_JOB_STATE_RUNNING;
      m_ProjectInfo.AppendText(L"Info : Project is running.\r\n");
    }
    else
    {
      job_state_ = DPE_JOB_STATE_FAILED;
      dpe_scheduler_ = NULL;
      m_ProjectInfo.AppendText(L"Info : Project runs failed.\r\n");
    }
  }
  return 0;
}

void  CDPEServiceDlg::AppendCompilerOutput()
{
  if (auto cj = dpe_compiler_->SourceCompileJob())
  {
    m_ProjectInfo.AppendText(L"Compile source output:\r\n");
    if (cj->compile_process_ && !cj->compile_process_->GetProcessContext()->cmd_line_.empty())
    {
      auto text = cj->compile_process_->GetProcessContext()->cmd_line_ + L"\r\n";
      m_ProjectInfo.AppendText(text.c_str());
    }
    if (!cj->compiler_output_.empty())
    {
      auto text = cj->compiler_output_ + "\r\n";
      m_ProjectInfo.AppendText(base::UTF8ToNative(text).c_str());
    }
  }
  if (auto cj = dpe_compiler_->WorkerCompileJob())
  {
    m_ProjectInfo.AppendText(L"Compile worker output:\r\n");
    if (cj->compile_process_ && !cj->compile_process_->GetProcessContext()->cmd_line_.empty())
    {
      auto text = cj->compile_process_->GetProcessContext()->cmd_line_ + L"\r\n";
      m_ProjectInfo.AppendText(text.c_str());
    }
    if (!cj->compiler_output_.empty())
    {
      auto text = cj->compiler_output_ + "\r\n";
      m_ProjectInfo.AppendText(base::UTF8ToNative(text).c_str());
    }
  }
  if (auto cj = dpe_compiler_->SinkCompileJob())
  {
    m_ProjectInfo.AppendText(L"Compile sink output:\r\n");
    if (cj->compile_process_ && !cj->compile_process_->GetProcessContext()->cmd_line_.empty())
    {
      auto text = cj->compile_process_->GetProcessContext()->cmd_line_ + L"\r\n";
      m_ProjectInfo.AppendText(text.c_str());
    }
    if (!cj->compiler_output_.empty())
    {
      auto text = cj->compiler_output_ + "\r\n";
      m_ProjectInfo.AppendText(base::UTF8ToNative(text).c_str());
    }
  }
}

void  CDPEServiceDlg::OnCompileError()
{
  if (job_state_ != DPE_JOB_STATE_COMPILING)
  {
    m_ProjectInfo.AppendText(L"Error : OnCompileError while the state is not compiling.\r\n");
    return;
  }
  job_state_ = DPE_JOB_STATE_COMPILE_ERROR;
  AppendCompilerOutput();
  m_ProjectInfo.AppendText(L"Error : Compile error.\r\n");
}

void  CDPEServiceDlg::OnCompileSuccess()
{
  if (job_state_ != DPE_JOB_STATE_COMPILING)
  {
    m_ProjectInfo.AppendText(L"Error : OnCompileSuccess while the state is not compiling.\r\n");
    return;
  }
  job_state_ = DPE_JOB_STATE_COMPILED;
  AppendCompilerOutput();
  m_ProjectInfo.AppendText(L"Info : Compile successful.\r\n");
}

void  CDPEServiceDlg::OnRunningError()
{
  if (job_state_ != DPE_JOB_STATE_RUNNING)
  {
    m_ProjectInfo.AppendText(L"Error : OnRunningError while the state is not running.\r\n");
    return;
  }
  job_state_ = DPE_JOB_STATE_FAILED;
  m_ProjectInfo.AppendText(L"Error : Run error.\r\n");
  dpe_scheduler_ = NULL;
}

void  CDPEServiceDlg::OnRunningSuccess()
{
  if (job_state_ != DPE_JOB_STATE_RUNNING)
  {
    m_ProjectInfo.AppendText(L"Error : OnRunningSuccess while the state is not running.\r\n");
    return;
  }
  job_state_ = DPE_JOB_STATE_FINISH;
  m_ProjectInfo.AppendText(L"Info : Run successful.\r\n");
  dpe_scheduler_ = NULL;
}
}
#include "dpe_service/main/dpe_service_dialog.h"

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
    
    m_ServerListCtrl.InsertColumn(0, L"id");
    m_ServerListCtrl.SetColumnWidth(0, 20);
    
    m_ServerListCtrl.InsertColumn(1, L"Address");
    m_ServerListCtrl.SetColumnWidth(1, 140);
    
    m_ServerListCtrl.InsertColumn(2, L"Response time");
    m_ServerListCtrl.SetColumnWidth(2, 140);
    
    UpdateServerList();
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

void  CDPEServiceDlg::UpdateServerList()
{
  auto& mgr = dpe_->GetNodeManager();
  auto& nl = mgr.NodeList();
  
  const int n = nl.size();
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
    
    m_ServerListCtrl.SetItemText(i, 0, base::StringPrintf(L"%d", item->node_id_).c_str());
    
    m_ServerListCtrl.SetItemText(i, 1, base::UTF8ToNative(item->server_address_).c_str());
    
    if (item->response_count_ == 0)
    {
      m_ServerListCtrl.SetItemText(i, 2, L"N/A");
    }
    else
    {
      double tmp = item->response_time_.InSecondsF() / item->response_count_;
      m_ServerListCtrl.SetItemText(i, 2, base::StringPrintf(L"%.3f", tmp).c_str());
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
}
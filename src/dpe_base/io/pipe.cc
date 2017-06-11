#include "dpe_base/pipe.h"

namespace base
{
const wchar_t kPipePrefix[] = L"\\\\.\\pipe\\{DPE_8F03487D-8A10-4d0b-B5DA-69CE50FD4497}";

PipeServer::PipeServer(int32_t open_mode, int32_t pipe_mode, int32_t buffer_size)
  : open_mode_(open_mode),
    pipe_mode_(pipe_mode),
    buffer_size_(buffer_size)
{
  ResetState();
  
  DWORD   om = static_cast<DWORD>(open_mode_) | FILE_FLAG_OVERLAPPED;
  int32_t pm = pipe_mode == PIPE_DATA_BYTE ?
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE :
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE;
  
  if (buffer_size_ <= 0)
  {
    buffer_size_ = kDefaultIOBufferSize;
  }

  pipe_name_ = base::StringPrintf(L"%lsPipeServer_%p_%p",
      kPipePrefix, ::GetCurrentProcessId(), this);
  
  overlap_.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  for (int id = 0; id < 100; ++id)
  {
    pipe_handle_ = ::CreateNamedPipeW(
        pipe_name_.c_str(),
        om, pm, 1, buffer_size_, buffer_size_, PIPE_TIMEOUT, NULL);
    
    if (pipe_handle_ != INVALID_HANDLE_VALUE) break;
    
    PLOG(ERROR) << "CreateNamedPipeW failed:\n"
                << base::SysWideToUTF8(pipe_name_);
  
    pipe_name_ = base::StringPrintf(L"%lsPipeServer_%p_%p_%d_%d",
                  kPipePrefix, ::GetCurrentProcessId(), this, id, rand());
  }
  state_ = PIPE_IDLE_STATE;
  
  if (open_mode_ != PIPE_OPEN_MODE_OUTBOUND)
  {
    read_buffer_.resize(buffer_size_+64);
    read_size_ = 0;
  }
  if (open_mode_ != PIPE_OPEN_MODE_INBOUND)
  {
    //write_buffer_.resize(buffer_size_+64);
    write_size_ = 0;
  }
}

PipeServer::~PipeServer()
{
  Close();
}

void PipeServer::ResetState()
{
  memset(&overlap_, 0, sizeof (overlap_));
  pipe_handle_ = NULL;
  read_size_ = 0;
  write_size_ = 0;
  state_ = PIPE_UNKNOWN_STATE;
  io_pending_ = false;
}

bool PipeServer::Close()
{
  if (pipe_handle_)
  {
    ::DisconnectNamedPipe(pipe_handle_);
    ::CloseHandle(pipe_handle_);
  }
  if (overlap_.hEvent)
  {
    ::CloseHandle(overlap_.hEvent);
  }
  ResetState();
  return true;
}

bool PipeServer::ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo, bool& fPendingIO)
{
  fPendingIO = false;

  if (ConnectNamedPipe(hPipe, lpo) != 0)
  {
    PLOG(ERROR) << "ConnectNamedPipe: can not connect named pipe:" << hPipe;
    return false;
  }
  switch (GetLastError())
  {
    case ERROR_IO_PENDING:
        fPendingIO = TRUE;
        break;

    case ERROR_PIPE_CONNECTED:
        if (::SetEvent(lpo->hEvent))
            break;
    default:
    {
      PLOG(ERROR) << "ConnectNamedPipe: can not connect named pipe:" << hPipe;
      return false;
    }
  }
  return true;
}

bool PipeServer::Accept()
{
  if (state_ != PIPE_IDLE_STATE)
  {
    LOG(ERROR) << "Accept failed: invalid state";
    return false;
  }
  if (ConnectToNewClient(pipe_handle_, &overlap_, io_pending_))
  {
    if (io_pending_)
    {
      state_ = PIPE_CONNECTING_STATE;
    }
    else if (open_mode_ == PIPE_OPEN_MODE_OUTBOUND)
    {
      state_ = PIPE_WRITING_STATE;
    }
    else
    {
      state_ = PIPE_READING_STATE;
    }
    write_size_ = read_size_ = 0;
    return true;
  }
  else
  {
    LOG(ERROR) << "Accept failed: ConnectToNewClient failed";
    state_ = PIPE_ERROR_STATE;
    return false;
  }
}

bool PipeServer::Read()
{
  if (state_ == PIPE_CONNECTING_STATE) return false;
  
  if (io_pending_) return false;
  
  if (open_mode_ == PIPE_OPEN_MODE_OUTBOUND) return false;
  
  overlap_.Offset = overlap_.OffsetHigh = 0;
  
  BOOL fSuccess = ReadFile(
               pipe_handle_,
               (char*)read_buffer_.c_str(),
               static_cast<int>(read_buffer_.size()),
               (DWORD*)&read_size_,
               &overlap_);

  state_ = PIPE_READING_STATE;

  if (fSuccess && read_size_ != 0)
  {
    io_pending_ = false;
    return true;
  }
  
  DWORD dwErr = GetLastError();
  if (!fSuccess && dwErr == ERROR_IO_PENDING)
  {
    io_pending_ = true;
    return true;
  }

  Close();

  return false;
}

bool PipeServer::Write(const char* buffer, int32_t size)
{
  if (state_ == PIPE_CONNECTING_STATE) return false;
  
  if (io_pending_) return false;
  
  if (open_mode_ == PIPE_OPEN_MODE_INBOUND) return false;
  
  if (!buffer || size <= 0) return false;

  std::string(buffer, buffer+size).swap(write_buffer_);
  
  overlap_.Offset = overlap_.OffsetHigh = 0;
  
  BOOL fSuccess = WriteFile(
               pipe_handle_,
               (char*)write_buffer_.c_str(),
               size,
               (DWORD*)&write_size_,
               &overlap_);

  state_ = PIPE_WRITING_STATE;

  if (fSuccess && write_size_ != 0)
  {
    io_pending_ = false;
    return true;
  }
  
  DWORD dwErr = GetLastError();
  if (!fSuccess && dwErr == ERROR_IO_PENDING)
  {
    io_pending_ = true;
    return true;
  }
  
  Close();
  return false;
}

bool PipeServer::WaitForPendingIO(int32_t time_out)
{
  switch (state_)
  {
    case PIPE_CONNECTING_STATE:
    case PIPE_WRITING_STATE:
    case PIPE_READING_STATE:
    {
      if (!io_pending_) return true;
      
      DWORD result = ::WaitForMultipleObjects(1, &overlap_.hEvent, FALSE, time_out);
      if (result == WAIT_TIMEOUT)
      {
        return false;
      }
      if (result != WAIT_OBJECT_0)
      {
        Close();
        return false;
      }
      DWORD cbRet = 0;
      BOOL fSuccess = GetOverlappedResult(
                pipe_handle_,
                &overlap_,
                &cbRet,
                FALSE);
      if (fSuccess)
      {
        io_pending_ = false;
        if (state_ == PIPE_READING_STATE)
        {
          read_size_ = cbRet;
        }
        else if (state_ == PIPE_WRITING_STATE)
        {
          write_size_ = cbRet;
        }
        else
        {
          state_ = open_mode_ == PIPE_OPEN_MODE_OUTBOUND ?
                PIPE_WRITING_STATE : PIPE_READING_STATE;
          write_size_ = read_size_ = 0;
        }
        return true;
      }
      Close();
      return true;
    }
    default:
      return false;
  }
}

scoped_refptr<IOHandler> PipeServer::CreateClientAndConnect(bool inherit, bool overlap)
{
  if (!Accept())
  {
    LOG(ERROR) << "Can not accept";
    return NULL;
  }

  int32_t io_flag = 0;
  
  DWORD mode = 0;
  if (open_mode_ == PIPE_OPEN_MODE_INBOUND)
  {
    mode = GENERIC_WRITE;
    io_flag |= IO_FLAG_WRITE;
  }
  else if (open_mode_ == PIPE_OPEN_MODE_OUTBOUND)
  {
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365787(v=vs.85).aspx
    mode = GENERIC_READ | FILE_WRITE_ATTRIBUTES;
    io_flag |= IO_FLAG_READ;
  }
  else
  {
    mode = GENERIC_WRITE | GENERIC_READ;
    io_flag |= IO_FLAG_WRITE | IO_FLAG_READ;
  }
  
  SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 
  SECURITY_ATTRIBUTES* psa = inherit ? &saAttr : NULL;
  
  DWORD fa = 0;
  if (overlap)
  {
    fa |= FILE_FLAG_OVERLAPPED;
    io_flag |= IO_FLAG_OVERLAP;
  }
  
  HANDLE handle = CreateFileW(
      pipe_name_.c_str(),
      mode,
      0,
      psa,
      OPEN_EXISTING,
      fa,
      NULL);

  if (INVALID_HANDLE_VALUE == handle) return NULL;

  {
    DWORD dwMode = pipe_mode_ == PIPE_DATA_BYTE ?
                        PIPE_READMODE_BYTE :
                        PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState( 
      handle,
      &dwMode,
      NULL,
      NULL);
    if (!fSuccess)
    {
      PLOG(ERROR) << "Can not SetNamedPipeHandleState";
      ::CloseHandle(handle);
      return NULL;
    }
  }
  
  if (!WaitForPendingIO(3000))
  {
    PLOG(ERROR) << "Can not WaitForPendingIO";
    ::CloseHandle(handle);
    return NULL;
  }
  return new IOHandler(handle, io_flag, buffer_size_);
}

}
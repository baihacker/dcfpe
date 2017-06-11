#include "dpe_base/io_handler.h"

namespace base
{

IOHandler::IOHandler(HANDLE io_handle, int32_t io_flag, int32_t buffer_size) :
  io_handle_(io_handle),
  io_flag_(io_flag),
  io_pending_(false),
  buffer_size_(buffer_size),
  io_operation_(IO_OPERATION_READ)
{
  memset(&overlap_, 0, sizeof (overlap_));
  if (io_flag & IO_FLAG_OVERLAP)
  {
    overlap_.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  }
  
  if (buffer_size_ <= 0) buffer_size = kDefaultIOBufferSize;
  
  if (io_flag & IO_FLAG_READ)
  {
    read_buffer_.resize(buffer_size_);
    read_size_ = 0;
  }
  
  if (io_flag & IO_FLAG_WRITE)
  {
    //write_buffer_.resize(buffer_size_);
    write_size_ = 0;
  }
  
  io_pending_ = false;
}

IOHandler::~IOHandler()
{
  if (overlap_.hEvent)
  {
    ::CloseHandle(overlap_.hEvent);
  }
  ::CloseHandle(io_handle_);
}

bool IOHandler::Read()
{
  if (io_pending_) return false;
  
  if (!(io_flag_ & IO_FLAG_READ)) return false;
  
  overlap_.Offset = overlap_.OffsetHigh = 0;
  
  OVERLAPPED* ol = io_flag_ & IO_FLAG_OVERLAP ?
                    &overlap_ : NULL;
  BOOL fSuccess = ReadFile(
               io_handle_,
               (char*)read_buffer_.c_str(),
               static_cast<int>(read_buffer_.size()),
               (DWORD*)&read_size_,
               ol);

  io_operation_ = IO_OPERATION_READ;

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
  
  return false;
}

bool IOHandler::Write(const char* buffer, int32_t size)
{
  if (io_pending_) return false;
  
  if (!(io_flag_ & IO_FLAG_WRITE)) return false;
  
  if (!buffer || size <= 0) return false;

  std::string(buffer, buffer+size).swap(write_buffer_);
  
  overlap_.Offset = overlap_.OffsetHigh = 0;
  
  OVERLAPPED* ol = io_flag_ & IO_FLAG_OVERLAP ?
                    &overlap_ : NULL;
  BOOL fSuccess = WriteFile(
               io_handle_,
               (char*)write_buffer_.c_str(),
               size,
               (DWORD*)&write_size_,
               ol);

  io_operation_ = IO_OPERATION_WRITE;

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
  
  return false;
}

bool IOHandler::WaitForPendingIO(int32_t time_out)
{
  if (!io_pending_) return true;
  
  DWORD result = ::WaitForMultipleObjects(1, &overlap_.hEvent, FALSE, time_out);
  if (result == WAIT_TIMEOUT)
  {
    return false;
  }
  if (result != WAIT_OBJECT_0)
  {
    return false;
  }
  DWORD cbRet = 0;
  BOOL fSuccess = GetOverlappedResult(
            io_handle_,
            &overlap_,
            &cbRet,
            FALSE);
  if (fSuccess)
  {
    io_pending_ = false;
    if (io_operation_ == IO_OPERATION_READ)
    {
      read_size_ = cbRet;
    }
    else if (io_operation_ == IO_OPERATION_WRITE)
    {
      write_size_ = cbRet;
    }
    else
    {
      write_size_ = read_size_ = 0;
    }
    return true;
  }
  return false;
}

}

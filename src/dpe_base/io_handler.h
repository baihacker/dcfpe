#ifndef DPE_BASE_IO_HANDLER_H_
#define DPE_BASE_IO_HANDLER_H_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/chromium_base.h"

namespace base
{

enum
{
  IO_FLAG_READ = 0x01,
  IO_FLAG_WRITE = 0x02,
  IO_FLAG_OVERLAP = 0x04,
};

enum
{
  IO_OPERATION_READ = 0,
  IO_OPERATION_WRITE = 1,
};

static const int32_t kDefaultIOBufferSize = 4096;

class DPE_BASE_EXPORT IOHandler : public base::RefCounted<IOHandler>
{
public:
  IOHandler(HANDLE io_handle, int32_t io_flag, int32_t buffer_size = kDefaultIOBufferSize);
  ~IOHandler();

  HANDLE        Handle() const {return io_handle_;}
  
  bool          Read();
  bool          Write(const char* buffer, int32_t size);
  
  bool          WaitForPendingIO(int32_t time_out);
  bool          HasPendingIO() const {return io_pending_ ? true : false;}
  
  std::string&  ReadBuffer(){return read_buffer_;}
  int32_t       ReadSize() const{return read_size_;}
  
private:
  HANDLE        io_handle_;
  int32_t       io_flag_;
  int32_t       buffer_size_;
  int32_t       io_operation_;
  
  OVERLAPPED    overlap_;
  bool          io_pending_;
  
  std::string   read_buffer_;
  int32_t       read_size_;
  std::string   write_buffer_;
  int32_t       write_size_;
};

}
#endif
#ifndef DPE_BASE_PIPE_H_
#define DPE_BASE_PIPE_H_

#include <string>

#include "dpe_base/chromium_base.h"
#include "dpe_base/dpe_base_export.h"
#include "dpe_base/io_handler.h"

namespace base
{
#define     PIPE_TIMEOUT                    5000

enum
{
  PIPE_UNKNOWN_STATE = -1,
  PIPE_IDLE_STATE = 0,
  PIPE_CONNECTING_STATE = 1,
  PIPE_READING_STATE = 2,
  PIPE_WRITING_STATE = 3,
  PIPE_ERROR_STATE = 4,
};

enum
{
  PIPE_OPEN_MODE_INBOUND = 1,  // PIPE_ACCESS_INBOUND
  PIPE_OPEN_MODE_OUTBOUND = 2, // PIPE_ACCESS_OUTBOUND
  PIPE_OPEN_MODE_DUPLEX = 3,   // PIPE_ACCESS_DUPLEX
};

enum
{
  PIPE_DATA_BYTE = 0,
  PIPE_DATE_MESSAGE = 1,
};

class IOHandler;
class DPE_BASE_EXPORT PipeServer  : public base::RefCounted<PipeServer>
{
public:
  PipeServer(int32_t open_mode, int32_t pipe_mode, int32_t buffer_size = kDefaultIOBufferSize);
  ~PipeServer();

  bool          Close();
  bool          Accept();
  bool          IsConnected() const;
  bool          WaitForPendingIO(int32_t time_out);
  bool          HasPendingIO() const;
  HANDLE        Handle() const {return pipe_handle_;}
  
  bool          Read();
  bool          Write(const char* buffer, int32_t size);
  
  std::string&  ReadBuffer(){return read_buffer_;}
  int32_t       ReadSize() const{return read_;}

  std::wstring  PipeName() const {return pipe_name_;}
  
  scoped_refptr<IOHandler> CreateClientAndConnect(bool inherit = true, bool overlap = true);
  
private:
  bool          ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo, BOOL& fPendingIO);
  void          ResetState();
  
private:
  std::wstring  pipe_name_;
  
  int32_t       open_mode_;
  int32_t       pipe_mode_;
  int32_t       buffer_size_;
  
  OVERLAPPED    overlap_;
  HANDLE        pipe_handle_;
  std::string   write_buffer_;
  DWORD         read_;
  std::string   read_buffer_;
  DWORD         write_;
  DWORD         state_;
  BOOL          io_pending_;
};

}

#endif
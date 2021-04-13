#include "dpe_base/dpe_base.h"

#include <windows.h>
#include <winioctl.h>


namespace base
{
static void quit_main_lop()
{
  LOG(INFO) << "quit_main_loop";
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  base::MessageLoop::current()->Quit();
}

void will_quit_main_loop()
{
  LOG(INFO) << "will_quit_main_loop";
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(quit_main_lop));
}

static MessageCenter* msg_center_impl = NULL;
static ZMQServer* zmq_server_impl = NULL;
static ZMQClient* zmq_client_impl = NULL;

int32_t dpe_base_main(void (*logic_main)(),
  base::MessagePumpDispatcher* dispatcher,
  int loggingLevel)
{
  {
    logging::SetMinLogLevel(loggingLevel);
    auto x = logging::LoggingSettings();
    x.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::BaseInitLoggingImpl(x);
    LOG(INFO) << "Dpe base main";
  }
  LOG(INFO) << "InitializeThreadPool";
  base::ThreadPool::InitializeThreadPool();

  LOG(INFO) << "MessageCenter";
  msg_center_impl = new MessageCenter();
  msg_center_impl->Start();

  LOG(INFO) << "ZMQServer";
  zmq_server_impl = new ZMQServer();
  zmq_server_impl->Start();

  LOG(INFO) << "ZMQClient";
  zmq_client_impl = new ZMQClient();
  zmq_client_impl->Start();

  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(logic_main));

  LOG(INFO) << "RunMainLoop";
  base::ThreadPool::RunMainLoop(dispatcher);

  LOG(INFO) << "Stop services";

  //LOG(INFO) << "Stop zmq client";
  delete zmq_client_impl;
  zmq_client_impl = NULL;

  //LOG(INFO) << "Stop zmq server";
  delete zmq_server_impl;
  zmq_server_impl = NULL;

  //LOG(INFO) << "Stop zmq center";
  delete msg_center_impl;
  msg_center_impl = NULL;

  LOG(INFO) << "DeinitializeThreadPool";
  base::ThreadPool::DeinitializeThreadPool();

  return 0;
}

MessageCenter* zmq_message_center()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return msg_center_impl;
}

ZMQServer* zmq_server()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return zmq_server_impl;
}

ZMQClient* zmq_client()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return zmq_client_impl;
}

static int blocking_pool_thread_number = 3;

void set_blocking_pool_thread_number(int thread_number) {
  blocking_pool_thread_number = thread_number;
}

int get_blocking_pool_thread_number() { return blocking_pool_thread_number; }

}

namespace base
{
bool StringEqualCaseInsensitive(const std::wstring& x, const std::wstring& y)
{
  const int n = static_cast<int>(x.size());
  const int m = static_cast<int>(y.size());
  if (n != m) return false;
  for (int i = 0; i < n; ++i)
  if (tolower(x[i]) != tolower(y[i]))
    return false;
  return true;
}
bool StringEqualCaseInsensitive(const std::string& x, const std::string& y)
{
  const int n = static_cast<int>(x.size());
  const int m = static_cast<int>(y.size());
  if (n != m) return false;
  for (int i = 0; i < n; ++i)
  if (tolower(x[i]) != tolower(y[i]))
    return false;
  return true;
}
#if defined(OS_WIN)
  std::wstring NativeToWide(const NativeString& x) {return x;}
  std::string NativeToUTF8(const NativeString& x) {return base::SysWideToUTF8(x);}
  NativeString WideToNative(const std::wstring& x) {return x;}
  NativeString UTF8ToNative(const std::string& x) {return base::SysUTF8ToWide(x);}
#elif defined(OS_POSIX)
  std::wstring NativeToWide(const NativeString& x) {return base::SysUTF8ToWide(x);}
  std::string NativeToUTF8(const NativeString& x) {return x;}
  NativeString WideToNative(const std::wstring& x) {return base::SysWideToUTF8(x);}
  NativeString UTF8ToNative(const std::string& x) {return x;}
#endif

static BOOL DoIdentify(HANDLE hPhysicalDriveIOCTL,
                            PSENDCMDINPARAMS pSCIP,
                            PSENDCMDOUTPARAMS pSCOP,
                            BYTE btIDCmd,
                            BYTE btDriveNum,
                            PDWORD pdwBytesReturned)
{
    pSCIP->cBufferSize = IDENTIFY_BUFFER_SIZE;
    pSCIP->irDriveRegs.bFeaturesReg = 0;
    pSCIP->irDriveRegs.bSectorCountReg  = 1;
    pSCIP->irDriveRegs.bSectorNumberReg = 1;
    pSCIP->irDriveRegs.bCylLowReg  = 0;
    pSCIP->irDriveRegs.bCylHighReg = 0;

    pSCIP->irDriveRegs.bDriveHeadReg = (btDriveNum & 1) ? 0xB0 : 0xA0;
    pSCIP->irDriveRegs.bCommandReg = btIDCmd;
    pSCIP->bDriveNumber = btDriveNum;
    pSCIP->cBufferSize = IDENTIFY_BUFFER_SIZE;

    return DeviceIoControl(hPhysicalDriveIOCTL,
                            SMART_RCV_DRIVE_DATA,
                            (LPVOID)pSCIP,
                            sizeof(SENDCMDINPARAMS) - 1,
                            (LPVOID)pSCOP,
                            sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
                            pdwBytesReturned, NULL);
}

static std::string ConvertToString(DWORD dwDiskData[256], int nFirstIndex, int nLastIndex)
{
  std::string result;

  for(int32_t nIndex = nFirstIndex; nIndex <= nLastIndex; nIndex++)
  {
    char val = dwDiskData[nIndex] / 256;
    if (val != 0 && val != 32) result.append(1, val);

    val = dwDiskData[nIndex] % 256;
    if (val != 0 && val != 32) result.append(1, val);
  }
  return result;
}

static std::string HardDriverIDImpl(int32_t driver_id)
{
  std::wstring path = base::StringPrintf(L"\\\\.\\PHYSICALDRIVE%d", driver_id);

  HANDLE hFile = ::CreateFileW(path.c_str(),
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING,
                       0, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    return "";
  }
  DWORD dwBytesReturned;
  GETVERSIONINPARAMS gvopVersionParams={0};
  DeviceIoControl(hFile,
                  SMART_GET_VERSION,
                  NULL,
                  0,
                  &gvopVersionParams,
                  sizeof(gvopVersionParams),
                   &dwBytesReturned, NULL);
  if(gvopVersionParams.bIDEDeviceMap <= 0)
  {
    ::CloseHandle(hFile);
    return "";
  }

  int btIDCmd = 0;
  SENDCMDINPARAMS InParams = {0};

  #define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
  #define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.

  btIDCmd = (gvopVersionParams.bIDEDeviceMap >> driver_id & 0x10) ? IDE_ATAPI_IDENTIFY : IDE_ATA_IDENTIFY;

  BYTE btIDOutCmd[sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1] = {0};

  if(DoIdentify(hFile,
                  &InParams,
                  (PSENDCMDOUTPARAMS)btIDOutCmd,
                  (BYTE)btIDCmd,
                  (BYTE)driver_id, &dwBytesReturned) == FALSE)
  {
    ::CloseHandle(hFile);
    return "";
  }
  ::CloseHandle(hFile);

  DWORD dwDiskData[256];
  USHORT *pIDSector;

  pIDSector = (USHORT*)((SENDCMDOUTPARAMS*)btIDOutCmd)->bBuffer;
  for(int i=0; i < 256; i++) dwDiskData[i] = pIDSector[i];

  return ConvertToString(dwDiskData, 10, 19);
  // 10 19 ���к�
  // 27 46 ģ�ͺ�
}

static std::string GetCPUID()
{
#if 0
  std::string CPUID;
  unsigned long s1,s2;
  unsigned char vendor_id[]="------------";
  char sel;
  sel='1';
  std::string VernderID;
  std::string CPUID1,CPUID2;
  switch(sel)
  {
  case '1':
      __asm{
          xor eax,eax      //eax=0:ȡVendor��Ϣ
          cpuid    //ȡcpu idָ�����Ring3��ʹ��
          mov dword ptr vendor_id,ebx
          mov dword ptr vendor_id[+4],edx
          mov dword ptr vendor_id[+8],ecx
      }
      VernderID = base::StringPrintf("%s-",vendor_id);
      __asm{
          mov eax,01h   //eax=1:ȡCPU���к�
          xor edx,edx
          cpuid
          mov s1,edx
          mov s2,eax
      }
      CPUID1 = base::StringPrintf("%08X%08X",s1,s2);
      __asm{
          mov eax,03h
          xor ecx,ecx
          xor edx,edx
          cpuid
          mov s1,edx
          mov s2,ecx
      }
      CPUID2 = base::StringPrintf("%08X%08X",s1,s2);
      break;
  case '2':
      {
          __asm{
              mov ecx,119h
              rdmsr
              or eax,00200000h
              wrmsr
          }
      }
      break;
  }
  return CPUID1+CPUID2;
#endif
  return "";
}

std::string PhysicalAddress()
{
  static std::string address;
  if (!address.empty()) return address;

  std::string cpuid = GetCPUID();
  std::string driver_id;
  for (int i = 1; i <= 8; ++i)
  {
    driver_id = HardDriverIDImpl(1);
    if (!driver_id.empty()) break;
  }
  LOG(INFO) << "cpuid : " << cpuid;
  LOG(INFO) << "driver id : " << driver_id;
  std::string id = base::StringPrintf("%s:%s", cpuid.c_str(), driver_id.c_str());
  StringToLowerASCII(&id);
  return address = base::MD5String(id);
}

void AddPaAndTs(base::DictionaryValue* value)
{
  if (value)
  {
    value->SetString("pa", base::PhysicalAddress());
    value->SetString("ts", base::StringPrintf("%lld", base::Time::Now().ToInternalValue()));
  }
}
}

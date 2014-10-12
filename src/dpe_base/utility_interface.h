#ifndef DPE_BASE_UTILITY_INTERFACE_H_
#define DPE_BASE_UTILITY_INTERFACE_H_

#include "interface_base.h"

enum
{
  INTERFACE_BUFFER = 100,
  INTERFACE_INTERFACE_LIST,
  INTERFACE_STRING_LIST,
  INTERFACE_STRICT_DICTIONARY,
  INTERFACE_DICTIONARY,
  
  INTERFACE_THREAD_CHECKER,
  INTERFACE_MESSAGE_HANDLER,
  INTERFACE_MESSAGE_CENTER,
};

struct IBuffer: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_BUFFER};
  virtual const int32_t size() const = 0;
  virtual char*         buffer() = 0;
  virtual int32_t       append(char* buffer, int32_t size) = 0;
  virtual int32_t       resize(int32_t size) = 0;
  virtual int32_t       reserve(int32_t size) = 0;
  virtual int32_t       memset(int32_t start, char c, int32_t size = -1) = 0;
};

struct IInterfaceList: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_INTERFACE_LIST};
  virtual const int32_t size() const = 0;
  virtual UnknownPtr    at(int32_t idx) = 0;
  virtual int32_t       remove(int32_t idx) = 0;
  virtual int32_t       clear() = 0;
  virtual int32_t       push_back(UnknownPtr element) = 0;
  virtual int32_t       pop_back() = 0;
};

struct IStringList : public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_STRING_LIST};
  virtual const int32_t   size() const = 0;
  virtual const wchar_t*  at(int32_t idx) = 0;
  virtual int32_t         remove(int32_t idx) = 0;
  virtual int32_t         clear() = 0;
  virtual int32_t         push_back(const wchar_t* val) = 0;
  virtual int32_t         pop_back() = 0;
};

struct IStrictDictionary : public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_STRICT_DICTIONARY};
  virtual const int32_t   size() const = 0;
  virtual int32_t         set(const wchar_t* key, const wchar_t* val) = 0;
  virtual const wchar_t*  get(const wchar_t* key) = 0;
  virtual int32_t         has_key(const wchar_t* key) = 0;
};

struct IDictionary : public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_DICTIONARY};
  virtual const int32_t   size() const = 0;
  virtual int32_t         set(const wchar_t* key, const wchar_t* val) = 0;
  virtual const wchar_t*  get(const wchar_t* key) = 0;
  virtual int32_t         has_key(const wchar_t* key) = 0;
};

struct IThreadChecker: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_THREAD_CHECKER};
  
  virtual int32_t         set_owned_thread(int32_t thread_id = -1) = 0;
  virtual int32_t         on_valid_thread() const = 0;
};

struct IMessageHandler: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_MESSAGE_HANDLER};
  
  // return 1 if handled
  virtual int32_t           handle_message(
        int32_t channel_id, int32_t is_ctrl, const char* msg, int32_t length) = 0;
};

enum
{
  INVALID_CHANNEL_ID = -1
};

struct IMessageCenter: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_MESSAGE_CENTER};

  // strong or weak reference?
  virtual int32_t       add_message_handler(IMessageHandler* handler) = 0;
  virtual int32_t       remove_message_handler(IMessageHandler* handler) = 0;
  
  virtual int32_t       add_sender_address(const char* address) = 0;
  virtual int32_t       add_receive_address(const char* address) = 0;
  virtual int32_t       remove_channel(int32_t channel_id) = 0;
  virtual const char*   get_address_by_channel(int32_t channel_id) = 0;

  virtual int32_t       send_ctrl_message(const char* msg, int32_t length) = 0;
  virtual int32_t       send_message(int32_t channel_id, const char* msg, int32_t length) = 0;
  virtual int32_t       worker_handle() = 0;
  virtual int32_t       start() = 0;
  virtual int32_t       stop() = 0;
};

int32_t      CreateUtility(int32_t interface_id, void** pp);

#endif
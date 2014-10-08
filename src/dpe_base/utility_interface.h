#ifndef SRC_DPE_BASE_UTILITY_INTERFACE_H
#define SRC_DPE_BASE_UTILITY_INTERFACE_H

#include "interface_base.h"

enum
{
  INTERFACE_BUFFER = 100,
  INTERFACE_INTERFACE_LIST,
  INTERFACE_STRING_LIST,
  INTERFACE_DICTIONARY,
  
  INTERFACE_THREAD_CHECKER,
  INTERFACE_RUNNABLE,
};

struct IBuffer: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_BUFFER};
  virtual const int32_t     size() = 0;
  virtual void*             buffer() = 0;
  virtual int32_t           append(void* buffer, int size) = 0;
  virtual int32_t           resize(int32_t size) = 0;
  virtual int32_t           reserve(int32_t size) = 0;
};

struct IInterfaceList: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_INTERFACE_LIST};
  virtual const int32_t     size();
  virtual UnknownPtr        at(int32_t idx) = 0;
  virtual int32_t           remove(int32_t idx) = 0;
  virtual int32_t           clear() = 0;
  virtual int32_t           push_back(UnknownPtr element) = 0;
  virtual int32_t           pop_back(UnknownPtr element) = 0;
};

struct IStringList : public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_STRING_LIST};
  virtual const int32_t     size() = 0;
  virtual const wchar_t*    at(int32_t idx) = 0;
  virtual int32_t           remove(int32_t idx) = 0;
  virtual int32_t           clear() = 0;
  virtual int32_t           push_back(const wchar_t* val) = 0;
  virtual int32_t           pop_back(const wchar_t* val) = 0;
};

struct IDictionary : public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_DICTIONARY};
  virtual const int32_t     size() = 0;
  virtual int32_t           is_strict() = 0;      // return 0 if case sensitive
  virtual int32_t           set(const wchar_t* key, const wchar_t* val) = 0;
  virtual const wchar_t*    get(const wchar_t* key) = 0;
  virtual int32_t           has_key(const wchar_t* key) = 0;
};

struct IThreadChecker: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_THREAD_CHECKER};
  
  virtual int32_t           on_valid_thread() const = 0;
};

struct IRunnable: public IDPEUnknown
{
  enum{INTERFACE_ID=INTERFACE_RUNNABLE};
  virtual int32_t           run() = 0;
  virtual int32_t           operator () () = 0;
};


UnknownPtr        CreateUtility(int32_t interface_id);
IDictionary*      CreateDictionary(int32_t is_strict);

#endif
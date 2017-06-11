#include "dpe_base/utility_interface.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>

namespace utility_impl
{
struct BufferImpl : public DPESingleInterfaceObjectRoot<IBuffer>
{

  explicit BufferImpl(int length)
  {
    if (length <= 0) length = 8;
    length = ((length + 7) >> 3) << 3;
    capacity_ = length;
    size_ = 0;
    ptr_ = new char[capacity_];
  }
  
  ~BufferImpl()
  {
    delete[] ptr_;
  }
  
  const int32_t     size() const override
  {
    return size_;
  }
  
  char*             buffer() override
  {
    return ptr_;
  }
  
  int32_t           append(char* buffer, int32_t size) override
  {
    const int32_t available = capacity_ - size_;
    if (available < size)
    {
      grow((size + size_)<<1);
    }
    memcpy(ptr_+size_, buffer, size);
    size_ += size;
    return DPE_OK;
  }
  
  int32_t           resize(int32_t size) override
  {
    int32_t old = size_;
    grow(size);
    size_ = size;
    return old;
  }
  
  int32_t           reserve(int32_t size) override
  {
    int32_t old = capacity_;
    grow(size);
    return old;
  }
  
  int32_t           memset(int32_t start, char c, int32_t size) override
  {

    int32_t end = size <= 0 ? size_ : std::min(start+size, size_);
    std::fill(ptr_+start, ptr_+end, c);
    
    return end - start;
  }
  
  void              grow(int32_t dest)
  {
    const int32_t new_capacity = ((dest + 7) >> 3) << 3;
    if (new_capacity > capacity_)
    {
      capacity_ = new_capacity;
      char* buffer = new char[capacity_];
      memcpy(buffer, ptr_, size_);
      delete[] ptr_;
      ptr_ = buffer;
    }
  }
  
private:
  char*             ptr_;
  int32_t           size_;
  int32_t           capacity_;
};

struct InterfaceListImpl : public DPESingleInterfaceObjectRoot<IInterfaceList>
{
  InterfaceListImpl()
  {
  }
  
  ~InterfaceListImpl()
  {
  }
  
  const int32_t     size() const override
  {
    return static_cast<int32_t>(ilist_.size());
  }
  
  UnknownPtr        at(int32_t idx) override
  {
    const int32_t n = size();
    return idx >= 0 && idx < n ? ilist_[idx] : nullptr;
  }
  
  int32_t           remove(int32_t idx) override
  {
    const int32_t n = size();
    if (idx >= 0 && idx < n)
    {
      if (IDPEUnknown* it = ilist_[idx])
      {
        it->Release();
      }
      ilist_.erase(ilist_.begin() + idx);
      return DPE_OK;
    }
    return DPE_FAILED;
  }
  
  int32_t           clear() override
  {
    const int32_t n = size();
    for (auto it: ilist_) if (it)
    {
      it->Release();
    }
    ilist_.clear();
    return DPE_OK;
  }
  
  int32_t           push_back(UnknownPtr element) override
  {
    if (element)
    {
      element->AddRef();
    }
    ilist_.push_back(element);
    return DPE_OK;
  }
  
  int32_t           pop_back()
  {
    if (size() > 0)
    {
      if (IDPEUnknown* it = ilist_.back())
      {
        it->Release();
      }
      ilist_.pop_back();
      return DPE_OK;
    }
    return DPE_FAILED;
  }
private:
  std::vector<IDPEUnknown*> ilist_;
};

struct StringListImpl : public DPESingleInterfaceObjectRoot<IStringList>
{
  StringListImpl()
  {
  }
  
  ~StringListImpl()
  {
  }
  
  const int32_t     size() const override
  {
    return static_cast<int32_t>(slist_.size());
  }
  
  const wchar_t*        at(int32_t idx) override
  {
    const int32_t n = size();
    return idx >= 0 && idx < n ? slist_[idx].c_str() : L"";
  }
  
  int32_t           remove(int32_t idx) override
  {
    const int32_t n = size();
    if (idx >= 0 && idx < n)
    {
      slist_.erase(slist_.begin() + idx);
      return DPE_OK;
    }
    return DPE_FAILED;
  }
  
  int32_t           clear() override
  {
    slist_.clear();
    return DPE_OK;
  }
  
  int32_t           push_back(const wchar_t* val) override
  {
    slist_.push_back(val ? val : L"");
    return DPE_OK;
  }
  
  int32_t           pop_back()
  {
    if (size() > 0)
    {
      slist_.pop_back();
      return DPE_OK;
    }
    return DPE_FAILED;
  }
private:
  std::vector<std::wstring> slist_;
};

struct StrictDictionaryImpl : public DPESingleInterfaceObjectRoot<IStrictDictionary>
{
  StrictDictionaryImpl()
  {
  }
  
  ~StrictDictionaryImpl()
  {
  }
  
  const int32_t     size() const override
  {
    return static_cast<int32_t>(map_impl_.size());
  }
  
  int32_t           set(const wchar_t* key, const wchar_t* val) override
  {
    if (!key || !key[0] || !val || !val[0]) return DPE_FAILED;
    map_impl_[key] = val;
    return DPE_OK;
  }
  
  const wchar_t*    get(const wchar_t* key) override
  {
    if (!key || !key[0])
    {
      return L"";
    }
    auto iter = map_impl_.find(key);
    return iter != map_impl_.end() ? iter->second.c_str() : L"";
  }
  
  virtual int32_t           has_key(const wchar_t* key) override
  {
    if (!key || !key[0])
    {
      return 0;
    }
    return static_cast<int>(map_impl_.count(key));
  }
private:
  std::unordered_map<std::wstring, std::wstring>  map_impl_;
};

struct NonStrictDictionaryImpl : public DPESingleInterfaceObjectRoot<IDictionary>
{
  NonStrictDictionaryImpl()
  {
  }
  
  ~NonStrictDictionaryImpl()
  {
  }
  
  const int32_t     size() const override
  {
    return static_cast<int32_t>(map_impl_.size());
  }
  
  int32_t           set(const wchar_t* key, const wchar_t* val) override
  {
    if (!key || !key[0] || !val || !val[0]) return DPE_FAILED;
    map_impl_[transform(key)] = val;
    return DPE_OK;
  }
  
  const wchar_t*    get(const wchar_t* key) override
  {
    if (!key || !key[0])
    {
      return L"";
    }
    auto iter = map_impl_.find(transform(key));
    return iter != map_impl_.end() ? iter->second.c_str() : L"";
  }
  
  virtual int32_t           has_key(const wchar_t* key) override
  {
    if (!key || !key[0])
    {
      return 0;
    }
    return static_cast<int>(map_impl_.count(transform(key)));
  }
private:
  std::wstring transform(const wchar_t* key)
  {
    std::wstring ret = key;
    for (auto& it: ret) it = tolower(it);
    return ret;
  }
private:
  std::unordered_map<std::wstring, std::wstring>  map_impl_;
};

struct ThreadCheckerImpl : public DPESingleInterfaceObjectRoot<IThreadChecker>
{
  ThreadCheckerImpl() : owned_thread_id_(-1)
  {
  }
  ~ThreadCheckerImpl()
  {
  }
  int32_t           set_owned_thread(int32_t thread_id) override
  {
    owned_thread_id_ = thread_id <= 0 ?
        static_cast<int32_t>(GetCurrentThreadId()) :
        thread_id;
    return DPE_OK;
  }
  int32_t           on_valid_thread() const override
  {
    return owned_thread_id_ <= 0 ||
      owned_thread_id_ == static_cast<int32_t>(GetCurrentThreadId());
  }
private:
  int32_t owned_thread_id_;
};

}

using namespace utility_impl;
int32_t      CreateUtility(int32_t interface_id, void** pp)
{
  *pp = nullptr;
  switch (interface_id)
  {
    case INTERFACE_BUFFER:
        {
          auto& ptr = *reinterpret_cast<IBuffer**>(pp);
          ptr = new BufferImpl(8);
          ptr->AddRef();
        }
        break;
    case INTERFACE_INTERFACE_LIST:
        {
          auto& ptr = *reinterpret_cast<IInterfaceList**>(pp);
          ptr = new InterfaceListImpl();
          ptr->AddRef();
        }
        break;
    case INTERFACE_STRING_LIST:
        {
          auto& ptr = *reinterpret_cast<IStringList**>(pp);
          ptr = new StringListImpl();
          ptr->AddRef();
        }
        break;
    case INTERFACE_STRICT_DICTIONARY:
        {
          auto& ptr = *reinterpret_cast<IStrictDictionary**>(pp);
          ptr = new StrictDictionaryImpl();
          ptr->AddRef();
        }
        break;
    case INTERFACE_DICTIONARY:
        {
          auto& ptr = *reinterpret_cast<IDictionary**>(pp);
          ptr = new NonStrictDictionaryImpl();
          ptr->AddRef();
        }
        break;
    case INTERFACE_THREAD_CHECKER:
        {
          auto& ptr = *reinterpret_cast<IThreadChecker**>(pp);
          ptr = new ThreadCheckerImpl();
          ptr->AddRef();
        }
        break;
  }
  if (*pp)
  {
    return DPE_OK;
  }
  return DPE_FAILED;
}

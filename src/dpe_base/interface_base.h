#ifndef SRC_DPE_BASE_INTERFACE_BASE_H
#define SRC_DPE_BASE_INTERFACE_BASE_H

#include <cstdint>
#include <atomic>
#include <utility>
/*
  Interface base for each module.
  
  Thread safe for interface:
    InterfacePtr:
      It supports multi-thread if the implementation of corresponding interface support.
    
    WeakInterfacePtr:
      It is required that the corresponding object is used in a single thread.
      It does not support to promote weak pointer to strong pointer.
    
    Design Rule:
      A specified implementation is always required to access in a single thread if possible.
*/
enum
{
  INTERFACE_UNKNOWN = 0,
};

enum
{
  DPE_OK = 0,
  DPE_FAILED = -1,
};

struct WeakPtrData;
struct IDPEUnknown
{
  enum{INTERFACE_ID=0};
  virtual ~IDPEUnknown(){}
  virtual int32_t GetInterfaceID() = 0;
  virtual int32_t QueryInterface(int32_t ID, void** ppInterface) = 0;
  virtual int32_t AddRef() = 0;
  virtual int32_t Release() = 0;
  virtual WeakPtrData* GetWeakPtrData() = 0;
};

#define SAFE_WEAK_PTR_DATA 1

struct WeakPtrData
{
#if SAFE_WEAK_PTR_DATA
  typedef IDPEUnknown*    ObjectPtr;
#else
  typedef void*           ObjectPtr;
#endif
  explicit WeakPtrData(ObjectPtr object) : object_(object)//, ref_count_(0) // msvc does not support
  {
    ref_count_.store(0);
  }
  
  ~WeakPtrData()
  {
    object_ = nullptr;
  }
  
  void Notify()
  {
    object_ = nullptr;
  }
  
  void Release()
  {
    if (--ref_count_ == 0)
    {
      delete this;
    }
  }
  
  ObjectPtr Access()
  {
    return object_;
  }
  
  void AddRef()
  {
    ++ref_count_;
  }
  ObjectPtr object_;
  std::atomic_int ref_count_;
};

template <class T, int32_t ObjectId = T::INTERFACE_ID>
class DPEObjectRoot : public T
{
public:
  enum { OBJECT_ID = ObjectId };

  DPEObjectRoot() : weakptr_data_(nullptr)
  {
    ref_count_.store(0);
  }
  
  ~DPEObjectRoot()
  {
    if (weakptr_data_) {
      weakptr_data_->Notify();
      weakptr_data_ = nullptr;
    }
  }

  virtual int32_t GetInterfaceID()
  {
    return T::INTERFACE_ID;
  }

  virtual int32_t AddRef()
  {
    return ++ref_count_;
  }

  virtual int32_t Release()
  {
    int32_t val = --ref_count_;
    if (val == 0)
    {
      delete this;
    }
    return val;
  }
  
  // on demand construct
  WeakPtrData* GetWeakPtrData()
  {
    if (!weakptr_data_)
    {
      weakptr_data_ = new WeakPtrData(this);
      weakptr_data_->AddRef();
    }
    return weakptr_data_;
  }

  virtual int32_t QueryInterface(int32_t id, void** ppInterface)
  {
    *ppInterface = nullptr;

    if (id == INTERFACE_UNKNOWN)
    {
      *ppInterface = (IDPEUnknown*)(T*)this;
    }
    else if (id == T::INTERFACE_ID)
    {
      *ppInterface = (T*)this;
    }

    if (*ppInterface)
    {
      AddRef();
      return DPE_OK;
    }

    return DPE_FAILED;
  }

protected:
  std::atomic_int ref_count_;
  WeakPtrData* weakptr_data_;
};

// see ref_counted.h
template <class T>
class InterfacePtr {
 public:
  typedef T element_type;

  InterfacePtr() : ptr_(nullptr) {
  }

  InterfacePtr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->AddRef();
  }

  InterfacePtr(const InterfacePtr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      ptr_->AddRef();
  }
  
  InterfacePtr(InterfacePtr<T>&& r) : ptr_(r.ptr_) {
    r.ptr_ = nullptr;
  }

  template <typename U>
  InterfacePtr(const InterfacePtr<U>& r) : ptr_(r.get()) {
    if (ptr_)
      ptr_->AddRef();
  }

  ~InterfacePtr() {
    if (ptr_)
      ptr_->Release();
  }
  void** storage() {return reinterpret_cast<void**>(&ptr_);}
  T* get() const { return ptr_; }
  
  operator T*() const { return ptr_; }
  
  T* operator->() const {
    return ptr_;
  }

  InterfacePtr<T>& operator=(T* p) {
    if (p)
      p->AddRef();
    T* old_ptr = ptr_;
    ptr_ = p;
    if (old_ptr)
      old_ptr->Release();
    return *this;
  }

  InterfacePtr<T>& operator=(const InterfacePtr<T>& r) {
    return *this = r.ptr_;
  }
  
  InterfacePtr<T>& operator=(InterfacePtr<T>&& r) {
    if (r.ptr_ != ptr_) {
      if (ptr_) ptr_->Release();
      ptr_ = r.ptr_;
      r.ptr_ = nullptr;
    }
    return *this;
  }

  template <typename U>
  InterfacePtr<T>& operator=(const InterfacePtr<U>& r) {
    return *this = r.get();
  }

  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(InterfacePtr<T>& r) {
    swap(&r.ptr_);
  }

 protected:
  T* ptr_;
};

template <class T>
class WeakInterfacePtr {
 public:
  typedef T element_type;
  
  WeakInterfacePtr(T* object) :
    weakptr_data_(object ? object->GetWeakPtrData() : nullptr)
  {
  }
  
  WeakInterfacePtr(const WeakInterfacePtr& r) : weakptr_data_(r.weakptr_data_)
  {
  }
  
  WeakInterfacePtr(WeakInterfacePtr&& r) : weakptr_data_(std::move(r.weakptr_data_))
  {
  }
  
  ~WeakInterfacePtr()
  {
  }
  
  operator T*() const { return get(); }
  
  T* operator->() const { return get(); }
  
  WeakInterfacePtr& operator = (T* object)
  {
    return *this = std::move(WeakInterfacePtr(object));
  }
  
  WeakInterfacePtr& operator = (const WeakInterfacePtr& r)
  {
    weakptr_data_ = r.weakptr_data_;
    return *this;
  }
  
  WeakInterfacePtr& operator = (WeakInterfacePtr&& r)
  {
    weakptr_data_ = std::move(r.weakptr_data_);
    return *this;
  }

#if SAFE_WEAK_PTR_DATA
  T* get() const
  {
    IDPEUnknown* ptr = weakptr_data_ ? weakptr_data_->Access() : nullptr;
    if (!ptr) return nullptr;
    
    T* ret = nullptr;
    int32_t rc = ptr->QueryInterface(T::INTERFACE_ID, reinterpret_cast<void**>(&ret));
    if (rc == 0 && ret) ret->Release(); // remove additional AddRef in QueryInterface
    
    return ret;
  }
#else
  T* get() const
  {
    return weakptr_data_ ? static_cast<T*>(weakptr_data_->Access()) : nullptr;
  }
#endif

private:
  InterfacePtr<WeakPtrData>  weakptr_data_;
};

typedef InterfacePtr<IDPEUnknown> UnknownPtr;

#endif
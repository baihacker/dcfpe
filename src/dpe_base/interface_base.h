#ifndef DPE_BASE_INTERFACE_BASE_H_
#define DPE_BASE_INTERFACE_BASE_H_

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
      A specified implementation is always required to access in a single thread 
      unless the implementation has an object model of RefCountedObjectModelEx
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
  virtual int32_t QueryInterface(int32_t ID, void** ppInterface) = 0;
  virtual int32_t AddRef() = 0;
  virtual int32_t Release() = 0;
  virtual WeakPtrData* GetWeakPtrData() = 0;
};

/*
  WeakPtrData
  
  // Object model
  RefCountedObjectModel       // weak pointer promoting forbidden
  RefCountedObjectModelEx     // weak pointer promoting support
  NormalObjectModel           // do not support reference counted. static object or object on stack.
  
  // An object implement multiple interfaces.
  OBJECT_MODEL_IMPL
  BEGIN_DECLARE_INTERFACE
  INTERFACE_ENTRY
  END_DECLARE_INTERFACE
  
  class Impl: public ObjectModel, public Interface0, public Interface1
  {
    OBJECT_MODEL_IMPL(ObjectModel);
    
    BEGIN_DECLARE_INTERFACE(Impl)
      INTERFACE_ENTRY(Interface0)
      INTERFACE_ENTRY(Interface1)
    END_DECLARE_INTERFACE
  };
  
  // An object implement single interface.
  DPESingleInterfaceObjectRoot
  class Impl: public DPESingleInterfaceObjectRoot<Interface, ObjectModel>
  {
  };
*/

struct WeakPtrData
{
  explicit WeakPtrData() //:ref_count_(0) msvc does not support
  {
    destroyed_flag_.store(0);
    ref_count_.store(0);
    object_strong_ref_count_.store(0);
  }

  ~WeakPtrData()
  {
    destroyed_flag_ .store(1);
  }

  void AddRef()
  {
    ++ref_count_;
  }
  
  void Release()
  {
    if (--ref_count_ == 0)
    {
      delete this;
    }
  }

  void Notify()
  {
    destroyed_flag_.store(1);
  }

  int32_t Valid() const
  {
    return !destroyed_flag_;
  }
  
  // used by RefCountedObjectModelEx only
  int32_t AddObjectRef()
  {
    return ++object_strong_ref_count_;
  }
  
  int32_t ReleaseObjectRef()
  {
    return --object_strong_ref_count_;
  }
  
  bool Promote()
  {
    for (;;)
    {
      int32_t old_val = object_strong_ref_count_.load();
      if (old_val == 0)
      {
        return false;
      }
      
      int32_t new_val = old_val + 1;
      if (object_strong_ref_count_.compare_exchange_strong(old_val, new_val))
      {
        return true;
      }
    }
  }

private:
  std::atomic_int   destroyed_flag_;
  std::atomic_int   ref_count_;
  std::atomic_int   object_strong_ref_count_;
};

class RefCountedObjectModel
{
public:
  RefCountedObjectModel() : weakptr_data_(nullptr)
  {
    ref_count_.store(0);
  }

  virtual ~RefCountedObjectModel()
  {
    if (weakptr_data_)
    {
      weakptr_data_->Notify();
      weakptr_data_->Release();
      weakptr_data_ = nullptr;
    }
  }

  virtual int32_t InternalAddRef()
  {
    return ++ref_count_;
  }

  virtual int32_t InternalRelease()
  {
    int32_t val = --ref_count_;
    if (val == 0)
    {
      delete this;
    }
    return val;
  }

  // on demand construct
  WeakPtrData* InternalGetWeakPtrData()
  {
    if (!weakptr_data_)
    {
      weakptr_data_ = new WeakPtrData();
      weakptr_data_->AddRef();
    }
    return weakptr_data_;
  }

protected:
  std::atomic_int ref_count_;
  WeakPtrData* weakptr_data_;
};

class RefCountedObjectModelEx
{
public:
  RefCountedObjectModelEx() : weakptr_data_(nullptr)
  {
    weakptr_data_ = new WeakPtrData();
    weakptr_data_->AddRef();
  }

  virtual ~RefCountedObjectModelEx()
  {
    if (weakptr_data_)
    {
      weakptr_data_->Notify();
      weakptr_data_->Release();
      weakptr_data_ = nullptr;
    }
  }

  virtual int32_t InternalAddRef()
  {
    return weakptr_data_->AddObjectRef();
  }

  virtual int32_t InternalRelease()
  {
    int32_t val = weakptr_data_->ReleaseObjectRef();
    if (val == 0)
    {
      delete this;
    }
    return val;
  }

  WeakPtrData* InternalGetWeakPtrData()
  {
    return weakptr_data_;
  }

protected:
  WeakPtrData* weakptr_data_;
};

class NormalObjectModel
{
public:
  NormalObjectModel() : weakptr_data_(nullptr)
  {
  }

  virtual ~NormalObjectModel()
  {
    if (weakptr_data_)
    {
      weakptr_data_->Notify();
      weakptr_data_->Release();
      weakptr_data_ = nullptr;
    }
  }

  virtual int32_t InternalAddRef()
  {
    return 1;
  }

  virtual int32_t InternalRelease()
  {
    return 1;
  }

  WeakPtrData* InternalGetWeakPtrData()
  {
    if (!weakptr_data_)
    {
      weakptr_data_ = new WeakPtrData();
      weakptr_data_->AddRef();
    }
    return weakptr_data_;
  }
protected:
  WeakPtrData* weakptr_data_;
};

#define OBJECT_MODEL_IMPL(MODEL)                                        \
  int32_t AddRef() override                                             \
  {                                                                     \
    return MODEL::InternalAddRef();                                     \
  }                                                                     \
                                                                        \
  int32_t Release() override                                            \
  {                                                                     \
    return MODEL::InternalRelease();                                    \
  }                                                                     \
                                                                        \
  WeakPtrData* GetWeakPtrData() override                                \
  {                                                                     \
    return MODEL::InternalGetWeakPtrData();                             \
  }

#define BEGIN_DECLARE_INTERFACE(T)                                      \
virtual int32_t QueryInterface(int32_t id, void** ppInterface) override \
{                                                                       \
  *ppInterface = nullptr;                                               \
  if (id == INTERFACE_UNKNOWN)                                          \
  {                                                                     \
    *ppInterface = (IDPEUnknown*)(T*)this;                              \
  }

#define INTERFACE_ENTRY(I)                                              \
  else if (id == I::INTERFACE_ID)                                       \
  {                                                                     \
    *ppInterface = (I*)this;                                            \
  }

#define END_DECLARE_INTERFACE                                           \
    if (*ppInterface)                                                   \
    {                                                                   \
      AddRef();                                                         \
      return DPE_OK;                                                    \
    }                                                                   \
                                                                        \
    return DPE_FAILED;                                                  \
}

template<typename I, typename OM = RefCountedObjectModel>
class DPESingleInterfaceObjectRoot : public OM, public I
{
public:
  DPESingleInterfaceObjectRoot()
  {
  }

  ~DPESingleInterfaceObjectRoot()
  {
  }

  int32_t AddRef() override
  {
    return OM::InternalAddRef();
  }

  int32_t Release() override
  {
    return OM::InternalRelease();
  }

  WeakPtrData* GetWeakPtrData() override
  {
    return OM::InternalGetWeakPtrData();
  }

  virtual int32_t QueryInterface(int32_t id, void** ppInterface) override
  {
    *ppInterface = nullptr;
    if (id == INTERFACE_UNKNOWN)
    {
      *ppInterface = (IDPEUnknown*)this;
    }
    else if (id == I::INTERFACE_ID)
    {
      *ppInterface = (I*)this;
    }

    if (*ppInterface)
    {
      AddRef();
      return DPE_OK;
    }

    return DPE_FAILED;
  }
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
    object_ptr_(object),
    weakptr_data_(object ? object->GetWeakPtrData() : nullptr)
  {
  }

  WeakInterfacePtr(const WeakInterfacePtr& r) :
    object_ptr_(r.object_ptr_),
    weakptr_data_(r.weakptr_data_)
  {
  }

  WeakInterfacePtr(WeakInterfacePtr&& r) :
    object_ptr_(r.object_ptr_),
    weakptr_data_(std::move(r.weakptr_data_))
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
    object_ptr_ = r.object_ptr_;
    weakptr_data_ = r.weakptr_data_;
    return *this;
  }

  WeakInterfacePtr& operator = (WeakInterfacePtr&& r)
  {
    object_ptr_ = r.object_ptr_;
    weakptr_data_ = std::move(r.weakptr_data_);
    return *this;
  }

  T* get() const
  {
    return weakptr_data_ && weakptr_data_->Valid() ? object_ptr_ : nullptr;
  }
  
  InterfacePtr<T> promote()
  {
    if (weakptr_data_ && weakptr_data_->Promote())
    {
      InterfacePtr<T> ret = object_ptr_;
      // we add an extra reference to this object when promoting
      // remove the extra reference now
      object_ptr_->Release();
      return ret;
    }
    return NULL;
  }
private:
  T*                          object_ptr_;
  InterfacePtr<WeakPtrData>   weakptr_data_;
};

typedef InterfacePtr<IDPEUnknown> UnknownPtr;

#endif
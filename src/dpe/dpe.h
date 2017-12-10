#ifndef DPE_DPE_H_
#define DPE_DPE_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __declspec(dllexport)
#define DPE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define DPE_EXPORT __declspec(dllimport)
#define DPE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(DPE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __attribute__((visibility("default")))
#define DPE_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif  // defined(DPE_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif

#include <cstdint>
typedef std::int64_t int64;

struct VariantsReaderStub {
  int (*getVariantsSize)(void* opaque);
  int64 (*getInt64Value)(void* opaque, int idx);
  const char* (*getStringValue)(void* opaque, int idx);
};

struct VariantsBuilderStub {
  void (*appendInt64Value)(void* opaque, int64 value);
  void (*appendStringValue)(void* opaque, const char* str);
};

struct CacheReaderStub
{
  void (*addRef)(void* opaque);
  void (*release)(void* opaque);
  // The VariantsReader* is valid before releasing the CacheReader.
  void* (*get)(void* opaque, int64 taskId);
  int64 (*getInt64)(void* opaque, int64 taskId);
  const char* (*getString)(void* opaque, int64 taskId);
};

class VariantsBuilder;

struct CacheWriterStub
{
  void (*addRef)(void* opaque);
  void (*release)(void* opaque);
  void (*append)(void* opaque, int64 taskId, VariantsBuilder* result);
  void (*appendInt64)(void* opaque, int64 taskId, int64 value);
  void (*appendString)(void* opaque, int64 taskId, const char* value);
};

class Solver;
struct DpeStub
{
  void* (*newCacheReader)(const char* path);
  void* (*newCacheWriter)(const char* path, bool reset);
  void* (*newDefaultCacheReader)();
  void* (*newDefaultCacheWriter)();
  
  VariantsReaderStub* VariantsReaderStub;
  VariantsBuilderStub* VariantsBuilderStub;
  CacheReaderStub* cacheReaderStub;
  CacheWriterStub* cacheWriterStub;
  
  void (*runDpe)(Solver* solver, int argc, char* argv[]);
};


DPE_EXPORT DpeStub* get_stub();

class VariantsReader
{
public:
  VariantsReader(void* opaque): opaque(opaque) {
    stub = get_stub()->VariantsReaderStub;
  }

  int size() const {
    return stub->getVariantsSize(opaque);
  }
  int64 int64Value(int idx) const {
    return stub->getInt64Value(opaque, idx);
  }
  const char* stringValue(int idx) const {
    return stub->getStringValue(opaque, idx);
  }
private:
  void* opaque;
  VariantsReaderStub* stub;
};

class VariantsBuilder
{
public:
  VariantsBuilder(void* opaque): opaque(opaque) {
    stub = get_stub()->VariantsBuilderStub;
  }
  VariantsBuilder* appendInt64Value(int64 value) {
    stub->appendInt64Value(opaque, value);
    return this;
  }
  VariantsBuilder* appendStringValue(const char* str) {
    stub->appendStringValue(opaque, str);
    return this;
  }
  void* opaque;
  VariantsBuilderStub* stub;
};

class CacheReader
{
public:
  CacheReader(void* opaque): opaque(opaque) {
    stub = get_stub()->cacheReaderStub;
  }
  ~CacheReader() {
  }
  
  void addRef() {
    stub->addRef(opaque);
  }
  void release() {
    stub->release(opaque);
  }
  VariantsReader get(int64 taskId) {
    void* vr = stub->get(opaque, taskId);
    return VariantsReader(vr);
  }
  int64 getInt64(int64 taskId) {
    return stub->getInt64(opaque, taskId);
  }
  const char* getString(int64 taskId) {
    return stub->getString(opaque, taskId);
  }
  static CacheReader newCacheReader(const char* path) {
    return CacheReader(get_stub()->newCacheReader(path));
  }
  static CacheReader newDefaultCacheReader() {
    return CacheReader(get_stub()->newDefaultCacheReader());
  }
  
  void* opaque;
  CacheReaderStub* stub;
};

class CacheWriter
{
public:
  CacheWriter(void* opaque): opaque(opaque) {
    stub = get_stub()->cacheWriterStub;
    addRef();
  }
  ~CacheWriter() {
    release();
  }

  void addRef() {
    stub->addRef(opaque);
  }
  void release() {
    stub->release(opaque);
  }
  void append(int64 taskId, VariantsBuilder* result) {
    stub->append(opaque, taskId, result);
  }
  void append(int64 taskId, int64 value) {
    stub->appendInt64(opaque, taskId, value);
  }
  void append(int64 taskId, const char* value) {
    stub->appendString(opaque, taskId, value);
  }
  static CacheWriter newCacheWriter(const char* path, bool reset) {
    return CacheWriter(get_stub()->newCacheWriter(path, reset));
  }
  static CacheWriter newDefaultCacheWriter() {
    return CacheWriter(get_stub()->newDefaultCacheWriter());
  }
private:
  void* opaque;
  CacheWriterStub* stub;
};

class TaskAppender
{
public:
  virtual void addTask(int64 taskId) = 0;
};

#define RUN_ON_MASTER_NODE
#define RUN_ON_WORKER_NODE
class Solver
{
public:
  #pragma RUN_ON_MASTER_NODE
  virtual void initAsMaster(TaskAppender* taskAppender) = 0;

  #pragma RUN_ON_WORKER_NODE
  virtual void initAsWorker() = 0;

  #pragma RUN_ON_MASTER_NODE
  virtual void setResult(int64 taskId, VariantsReader* result, int64 timeUsage) = 0;

  #pragma RUN_ON_WORKER_NODE
  virtual void compute(int64 taskId, VariantsBuilder* result) = 0;

  #pragma RUN_ON_MASTER_NODE
  virtual void finish() = 0;
};

#endif

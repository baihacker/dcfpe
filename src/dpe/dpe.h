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

class VariantsReader
{
public:
  virtual ~VariantsReader(){}
  virtual int size() const = 0;
  virtual int64 int64Value(int idx) const = 0;
  virtual const char* stringValue(int idx) const = 0;
};

class VariantsBuilder
{
public:
  virtual ~VariantsBuilder(){}
  virtual VariantsBuilder* appendInt64Value(int64 value) = 0;
  virtual VariantsBuilder* appendStringValue(const char* str) = 0;
};

class CacheReader
{
public:
  virtual ~CacheReader(){}
  virtual void addRef() = 0;
  virtual void release() = 0;
  // The VariantsReader* is valid before releasing the CacheReader.
  virtual VariantsReader* get(int64 taskId) = 0;
  virtual int64 getInt64(int64 taskId) = 0;
  virtual const char* getString(int64 taskId) = 0;
};

class CacheWriter
{
public:
  virtual ~CacheWriter(){}
  virtual void addRef() = 0;
  virtual void release() = 0;
  virtual void append(int64 taskId, VariantsBuilder* result) = 0;
  virtual void append(int64 taskId, int64 value) = 0;
  virtual void append(int64 taskId, const char* value) = 0;
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

struct DpeStub
{
  CacheReader* (*newCacheReader)(const char* path);
  CacheWriter* (*newCacheWriter)(const char* path, bool reset);
  CacheReader* (*newDefaultCacheReader)();
  CacheWriter* (*newDefaultCacheWriter)();
  void (*runDpe)(Solver* solver, int argc, char* argv[]);
};

DPE_EXPORT DpeStub* get_stub();

#endif

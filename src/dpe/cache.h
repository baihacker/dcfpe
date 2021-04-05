#ifndef DPE_CACHE_H_
#define DPE_CACHE_H_

#include <unordered_map>

#include "dpe/dpe.h"
#include "dpe_base/dpe_base.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/variants.h"

namespace dpe {
class CacheReaderImpl {
 public:
  CacheReaderImpl(const std::unordered_map<int64, VariantsReaderImpl*>& data);
  CacheReaderImpl(std::unordered_map<int64, VariantsReaderImpl*>&& data);
  ~CacheReaderImpl();
  void addRef();
  void release();
  VariantsReaderImpl* get(int64 taskId);
  int64 getInt64(int64 taskId);
  const char* getString(int64 taskId);

  static CacheReaderImpl* fromLines(std::vector<std::string>& lines);

 private:
  void clearMap();

 private:
  int refCount;
  std::unordered_map<int64, VariantsReaderImpl*> data;
};

class CacheWriterImpl {
 public:
  CacheWriterImpl(const base::FilePath& path, bool reset);
  ~CacheWriterImpl();
  bool ready() const { return file.IsValid(); }
  void addRef();
  void release();
  void append(int64 taskId, VariantsBuilderImpl* result);
  void append(int64 taskId, int64 value);
  void append(int64 taskId, const char* value);

 private:
  void append(int64 taskId, const Variants& vars);

 private:
  int refCount;
  base::File file;
};
}  // namespace dpe
#endif

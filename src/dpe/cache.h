#ifndef DPE_CACHE_H_
#define DPE_CACHE_H_

#include "dpe/dpe.h"
#include "dpe_base/dpe_base.h"
#include <unordered_map>

namespace dpe
{
class CacheReaderImpl : public CacheReader
{
public:
  CacheReaderImpl(const std::unordered_map<int64, VariantsReader*>& data);
  CacheReaderImpl(std::unordered_map<int64, VariantsReader*>&& data);
  ~CacheReaderImpl();
  void addRef();
  void release();
  VariantsReader* get(int64 taskId);
  
  static CacheReaderImpl* fromLines(std::vector<std::string>& lines);
private:
  void clearMap();
private:
  int refCount;
  std::unordered_map<int64, VariantsReader*> data;
};

class CacheWriterImpl : public CacheWriter
{
public:
  CacheWriterImpl(const base::FilePath& path, bool reset);
  ~CacheWriterImpl();
  bool ready() const
  {
    return file.IsValid();
  }
  void addRef();
  void release();
  void append(int64 taskId, VariantsBuilder* result);
private:
  int refCount;
  base::File file;
};
}
#endif

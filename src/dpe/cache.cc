#include "dpe/cache.h"
#include "dpe/variants.h"
#include "dpe/proto/dpe.pb.h"
#include "google/protobuf/text_format.h"

namespace dpe
{
CacheReaderImpl::CacheReaderImpl(const std::unordered_map<int64, VariantsReader*>& data) : refCount(0), data(data)
{
}

CacheReaderImpl::CacheReaderImpl(std::unordered_map<int64, VariantsReader*>&& odata) : refCount(0)
{
  data.swap(odata);
}

CacheReaderImpl::~CacheReaderImpl()
{
  clearMap();
}

void CacheReaderImpl::addRef()
{
  ++refCount;
}

void CacheReaderImpl::release()
{
  if (--refCount == 0)
  {
    delete this;
  }
}

VariantsReader* CacheReaderImpl::get(int64 taskId)
{
  auto where = data.find(taskId);
  if (where != data.end())
  {
    return where->second;
  }
  return NULL;
}

void CacheReaderImpl::clearMap()
{
  for (auto& iter: data)
  {
    delete iter.second;
  }
  std::unordered_map<int64, VariantsReader*>().swap(data);
}

CacheReaderImpl* CacheReaderImpl::fromLines(std::vector<std::string>& lines)
{
  std::unordered_map<int64, VariantsReader*> result;
  for (const auto& iter: lines)
  {
    const int n = static_cast<int>(iter.size());
    const char* start = iter.c_str();
    const char* last = iter.c_str() + n;
    const char* now = start;
    while (*now != ':') ++now;
    
    int64 taskId = std::atoll(start);
    Variants vars;
    if (google::protobuf::TextFormat::ParseFromString(std::string(now+1, last), &vars))
    {
      auto where = result.find(taskId);
      if (where != result.end())
      {
        delete where->second;
        where->second = new VariantsReaderImpl(vars);
      }
      else
      {
        result[taskId] = new VariantsReaderImpl(vars);
      }
    }
  }
  return new CacheReaderImpl(std::move(result));
}

CacheWriterImpl::CacheWriterImpl(const base::FilePath& path, bool reset) : refCount(0), file(path, (reset ? base::File::FLAG_CREATE_ALWAYS : base::File::FLAG_OPEN_ALWAYS) | base::File::FLAG_APPEND)
{
}

CacheWriterImpl::~CacheWriterImpl()
{
}

void CacheWriterImpl::addRef()
{
  ++refCount;
}

void CacheWriterImpl::release()
{
  if (--refCount == 0)
  {
    delete this;
  }
}

void CacheWriterImpl::append(int64 taskId, VariantsBuilder* result)
{
  auto impl = static_cast<VariantsBuilderImpl*>(result);
  std::string data = base::StringPrintf("%s:%s\n",
    std::to_string(taskId).c_str(), impl->getVariants().ShortDebugString().c_str());
  file.Write(0, data.c_str(), data.size());
}

CacheReader* newCacheReader(const char* path)
{
  std::string data;
  base::FilePath filePath(base::UTF8ToNative(path));
  if (!base::ReadFileToString(filePath, &data))
  {
    return NULL;
  }
  
  std::vector<std::string> lines;
  Tokenize(data, "\n", &lines);
  
  return dpe::CacheReaderImpl::fromLines(lines);
}

CacheWriter* newCacheWriter(const char* path, bool reset)
{
  auto result = new dpe::CacheWriterImpl(base::FilePath(base::UTF8ToNative(path)), reset);
  if (!result->ready())
  {
    delete result;
    return NULL;
  }
  return result;
}
}

#ifndef DPE_VARIANTS_H_
#define DPE_VARIANTS_H_

#include "dpe/dpe.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe
{
class VariantsReaderImpl {
public:
  VariantsReaderImpl(const Variants& variants);
  ~VariantsReaderImpl();

  int size() const;
  int64 int64Value(int idx) const;
  const char* stringValue(int idx) const;
private:
  Variants variants;
};

class VariantsBuilderImpl {
public:
  VariantsBuilderImpl();
  ~VariantsBuilderImpl();
  const Variants& getVariants() const;
  VariantsBuilderImpl* appendInt64Value(int64 value);
  VariantsBuilderImpl* appendStringValue(const char* str);
private:
  Variants variants;
};
}

#endif
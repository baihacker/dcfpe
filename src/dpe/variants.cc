#include "dpe/variants.h"

namespace dpe
{
VariantsReaderImpl::VariantsReaderImpl(const Variants& variants) : variants(variants)
{
}

int VariantsReaderImpl::size() const
{
  return variants.element_size();
}

int64 VariantsReaderImpl::int64Value(int idx) const
{
  return variants.element(idx).value_int64();
}

const char* VariantsReaderImpl::stringValue(int idx) const
{
  return variants.element(idx).value_string().c_str();
}

const Variants& VariantsBuilderImpl::getVariants() const
{
  return variants;
}

VariantsBuilder* VariantsBuilderImpl::appendInt64Value(int64 value)
{
  variants.add_element()->set_value_int64(value);
  return this;
}

VariantsBuilder* VariantsBuilderImpl::appendStringValue(const char* str)
{
  variants.add_element()->set_value_string(str);
  return this;
}
}
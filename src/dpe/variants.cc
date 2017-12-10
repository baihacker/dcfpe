#include "dpe/variants.h"

namespace dpe
{
VariantsReaderImpl::VariantsReaderImpl(const Variants& variants) : variants(variants)
{
}

VariantsReaderImpl::~VariantsReaderImpl()
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

VariantsBuilderImpl::VariantsBuilderImpl()
{
}

VariantsBuilderImpl::~VariantsBuilderImpl()
{
}


const Variants& VariantsBuilderImpl::getVariants() const
{
  return variants;
}

VariantsBuilderImpl* VariantsBuilderImpl::appendInt64Value(int64 value)
{
  variants.add_element()->set_value_int64(value);
  return this;
}

VariantsBuilderImpl* VariantsBuilderImpl::appendStringValue(const char* str)
{
  variants.add_element()->set_value_string(str);
  return this;
}
}
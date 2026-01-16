#ifndef __DISTRIBUTOR_H__
#define __DISTRIBUTOR_H__

#pragma once

#include <memory>
#include "row_generated.h"
#include "data.pb.h"
#include "rocksdb/transformer.h"
#include "protobuf_distributor_schema.h"

namespace ROCKSDB_NAMESPACE {

class Distributor : public Transformer {
  public:
    Distributor() {};
    ~Distributor() {};

    std::string Name() const override { return "Distributor"; }

    virtual std::vector<ByteBuffer> Transform(
      const ByteBuffer& input_bytes,
      const std::shared_ptr<SchemaDescriptor>& schema) const override;
    
    TransformerType Supports() const override { return TransformerType::DISTRIBUTOR; }
  private:
    
};

} // namespace ROCKSDB_NAMESPACE
#endif // __DISTRIBUTOR_H__
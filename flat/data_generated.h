// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_DATA_H_
#define FLATBUFFERS_GENERATED_DATA_H_

#include "flatbuffers/flatbuffers.h"

struct NumericColumn;

struct FbRow;

struct NumericColumn FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_NAME = 4,
    VT_VALUE = 6
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  uint64_t value() const {
    return GetField<uint64_t>(VT_VALUE, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.VerifyString(name()) &&
           VerifyField<uint64_t>(verifier, VT_VALUE) &&
           verifier.EndTable();
  }
};

struct NumericColumnBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(NumericColumn::VT_NAME, name);
  }
  void add_value(uint64_t value) {
    fbb_.AddElement<uint64_t>(NumericColumn::VT_VALUE, value, 0);
  }
  explicit NumericColumnBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  NumericColumnBuilder &operator=(const NumericColumnBuilder &);
  flatbuffers::Offset<NumericColumn> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<NumericColumn>(end);
    return o;
  }
};

inline flatbuffers::Offset<NumericColumn> CreateNumericColumn(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    uint64_t value = 0) {
  NumericColumnBuilder builder_(_fbb);
  builder_.add_value(value);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<NumericColumn> CreateNumericColumnDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    uint64_t value = 0) {
  auto name__ = name ? _fbb.CreateString(name) : 0;
  return CreateNumericColumn(
      _fbb,
      name__,
      value);
}

struct FbRow FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_NUMCOLS = 4
  };
  const flatbuffers::Vector<flatbuffers::Offset<NumericColumn>> *numcols() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<NumericColumn>> *>(VT_NUMCOLS);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_NUMCOLS) &&
           verifier.VerifyVector(numcols()) &&
           verifier.VerifyVectorOfTables(numcols()) &&
           verifier.EndTable();
  }
};

struct FbRowBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_numcols(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<NumericColumn>>> numcols) {
    fbb_.AddOffset(FbRow::VT_NUMCOLS, numcols);
  }
  explicit FbRowBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  FbRowBuilder &operator=(const FbRowBuilder &);
  flatbuffers::Offset<FbRow> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<FbRow>(end);
    return o;
  }
};

inline flatbuffers::Offset<FbRow> CreateFbRow(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<NumericColumn>>> numcols = 0) {
  FbRowBuilder builder_(_fbb);
  builder_.add_numcols(numcols);
  return builder_.Finish();
}

inline flatbuffers::Offset<FbRow> CreateFbRowDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<flatbuffers::Offset<NumericColumn>> *numcols = nullptr) {
  auto numcols__ = numcols ? _fbb.CreateVector<flatbuffers::Offset<NumericColumn>>(*numcols) : 0;
  return CreateFbRow(
      _fbb,
      numcols__);
}

inline const FbRow *GetFbRow(const void *buf) {
  return flatbuffers::GetRoot<FbRow>(buf);
}

inline const FbRow *GetSizePrefixedFbRow(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<FbRow>(buf);
}

inline bool VerifyFbRowBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<FbRow>(nullptr);
}

inline bool VerifySizePrefixedFbRowBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<FbRow>(nullptr);
}

inline void FinishFbRowBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<FbRow> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedFbRowBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<FbRow> root) {
  fbb.FinishSizePrefixed(root);
}

#endif  // FLATBUFFERS_GENERATED_DATA_H_

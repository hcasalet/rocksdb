#include "distributor.h"
#include "converter.h"
#include "augmenter.h"
#include "mynooper.h"

std::shared_ptr<Transformer> NewTransformer(const TransformerType transformer_type) {
    switch (transformer_type) {
        case TransformerType::DISTRIBUTOR:
            return std::make_shared<Distributor>();
        case TransformerType::CONVERTER:
            return std::make_shared<Converter>();
        case TransformerType::AUGMENTER:
            return std::make_shared<Augmenter>();
        case TransformerType::MYNOOPER:
            return std::make_shared<Mynooper>();
        case TransformerType::NOTRANSFORMATION:
        default:
            return nullptr;  // Handle invalid type
    }
}
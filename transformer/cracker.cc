#include <iostream>
#include "cracker.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Cracker::Transform(InternalIterator* input_iter, std::vector<VectorIterator*>* output_iters, int splits)
{
    std::vector<std::string> keys;
    std::vector<std::vector<std::string> > values_set; 

    input_iter->SeekToFirst();
    data::Row row;
    int group_size = 0; 
    if (input_iter->Valid()) {
        row.ParseFromString(input_iter->value().data());
        group_size = row.columns_size()/splits;
        if (group_size < 1) {
            group_size = 1;
        }
        if (splits > row.columns_size()) {
            splits = row.columns_size();
        }
        for (int i = 0; i < splits; i++) {
            std::vector<std::string> values;
            values_set.push_back(values);
        }
    }
    
    
    while (input_iter->Valid()) {
        if (keys.size() > 0) {
            row.ParseFromString(input_iter->value().data());
        }
        keys.push_back(input_iter->key().data());
        
        for (int i = 0; i < splits; i++) {
            data::Row splittedRow;
            for (int j = 0; j < group_size; j++) {
                data::Column* newColumn = splittedRow.add_columns();
                newColumn->set_name(row.columns(i*group_size+j).name());
                newColumn->set_value(row.columns(i*group_size+j).value());
            }
            std::string serializedRow;
            splittedRow.SerializeToString(&serializedRow);
            values_set[i].push_back(serializedRow);
        }

        input_iter->Next();
    }

    for (int i = 0; i < splits; i++) {
        VectorIterator* output_iter = new VectorIterator(keys, values_set[i]);
        output_iters->push_back(output_iter);
    }
}

void Cracker::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) {
    data::Row row;
    row.ParseFromString(input);
    int group_size = row.columns_size()/splits;
    if (group_size < 1) {
        group_size = 1;
    }
   
    for (int i = 0; i < splits; i++) {
        data::Row splittedRow;
        for (int j = 0; j < group_size; j++) {
            data::Column* newColumn = splittedRow.add_columns();
            newColumn->set_name(row.columns(i*group_size+j).name());
            newColumn->set_value(row.columns(i*group_size+j).value());
        }
        std::string serializedRow;
        splittedRow.SerializeToString(&serializedRow);
        outputs->push_back(serializedRow);
    }

    if (splits > 0 && extra_write) {
        outputs->push_back(input);
    }
}

}
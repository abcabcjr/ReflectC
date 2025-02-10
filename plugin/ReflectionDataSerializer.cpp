#include "ReflectionDataSerializer.hpp"
#include <fstream>
#include <iostream>

ReflectionDataSerializer::ReflectionDataSerializer(std::string outputFile)
    : OutputFile(std::move(outputFile)) {}

bool ReflectionDataSerializer::serialize(const std::unordered_map<std::string, BaseType> &baseTypes,
                                           const std::vector<RecordInfo> &records,
                                           const std::vector<EnumInfo> &enums,
                                           const size_t archSize) const {
    std::ofstream outFile(OutputFile, std::ios::out);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file " << OutputFile << " for writing\n";
        return false;
    }

    outFile << "arch " << archSize << "\n";

    for (const auto &[fst, snd] : baseTypes) {
        const BaseType &bt = snd;
        if (bt.Variant != TypeVariant::Base) {
            continue;
        }
        outFile << "base\n";
        outFile << "name " << bt.Name << "\n";
        for (const auto &alias : bt.Aliases) {
            if (alias != bt.Name) {
                outFile << "alias " << alias << "\n";
            }
        }
        outFile << "size " << bt.Size << "\n";
    }

    for (const auto &rec : records) {
        outFile << ((rec.Variant == TypeVariant::Struct) ? "struct" : "union") << "\n";
        outFile << "name " << rec.Name << "\n";
        for (const auto &alias : rec.Aliases) {
            if (alias != rec.Name) {
                outFile << "alias " << alias << "\n";
            }
        }
        outFile << "size " << rec.Size << "\n";

        for (const auto &field : rec.Fields) {
            outFile << "field\n"
                    << "name " << field.Name << "\n"
                    << "type " << (field.IsStructOrUnion ? field.StructOrUnionName : field.Type) << "\n"
                    << "offset " << field.Offset << "\n"
                    << "pdepth " << field.PointerDepth << "\n"
                    << "arrsize " << field.ArraySize << "\n"
                    << "const " << (field.IsConst ? "true" : "false") << "\n"
                    << "isstruct " << (field.IsStructOrUnion ? "true" : "false") << "\n";
        }
    }

    for (const auto &en : enums) {
        outFile << "enum\n";
        outFile << "name " << en.Name << "\n";
        for (const auto &alias : en.Aliases) {
            if (alias != en.Name) {
                outFile << "alias " << alias << "\n";
            }
        }
        outFile << "size " << en.Size << "\n";
        for (const auto &[ename, evalue] : en.Enumerators) {
            outFile << "enumerator\n";
            outFile << "ek " << ename << "\n";
            outFile << "ev " << evalue << "\n";
        }
    }

    outFile.close();
    return true;
}

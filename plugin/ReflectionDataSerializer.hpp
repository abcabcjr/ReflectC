#pragma once

#include "ReflectionTypes.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class ReflectionDataSerializer {
public:
    explicit ReflectionDataSerializer(std::string outputFile);
    [[nodiscard]] bool serialize(const std::unordered_map<std::string, BaseType> &baseTypes,
                   const std::vector<RecordInfo> &records,
                   const std::vector<EnumInfo> &enums,
                   size_t archSize) const;

private:
    std::string OutputFile;
};

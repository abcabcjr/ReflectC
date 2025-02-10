#include "ReflectionTypes.hpp"

RecordInfo::RecordInfo(const TypeVariant variant, const std::string &name, const size_t size, const size_t typeId)
    : BaseType(variant, name, size, typeId), RD(nullptr) {}

void RecordInfo::AddField(const FieldInfo &field) {
    Fields.push_back(field);
}

EnumInfo::EnumInfo(const void *ED, const std::string &name, const size_t size, const size_t typeId)
    : BaseType(TypeVariant::Enum, name, size, typeId), ED(ED) {}

void EnumInfo::AddEnumerator(const std::string &name, int64_t value) {
    Enumerators.emplace_back(name, value);
}

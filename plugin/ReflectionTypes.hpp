#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

enum class TypeVariant {
    Base,
    Struct,
    Union,
    Enum
};

class BaseType {
public:
    BaseType(const TypeVariant variant, std::string name, const size_t size, const size_t typeId)
        : Variant(variant), Name(std::move(name)), Size(size), TypeID(typeId) {}
    virtual ~BaseType() = default;

    TypeVariant Variant;
    std::string Name;
    size_t Size;
    size_t TypeID;
    // typedef aliases
    std::vector<std::string> Aliases; // this is technically only used for records and enums (typedef'd basetypes are added separately), should probably rework this
};

class FieldInfo {
public:
    FieldInfo(std::string name, std::string type, const size_t offset)
        : Name(std::move(name)), Type(std::move(type)), Offset(offset),
          PointerDepth(0), ArraySize(0), IsConst(false), IsStructOrUnion(false) {}

    std::string Name;
    std::string Type; // canonical type name
    size_t Offset;
    int PointerDepth;
    size_t ArraySize; // zero if not an array; otherwise total elements
    bool IsConst;
    bool IsStructOrUnion;
    std::string StructOrUnionName; // for struct/union fields (TODO: get rid of this)
    std::string Alias;
};

class RecordInfo final : public BaseType {
public:
    RecordInfo(TypeVariant variant, const std::string &name, size_t size, size_t typeId);
    void AddField(const FieldInfo &field);

    std::vector<FieldInfo> Fields;
    const void *RD; // pointer to RecordDecl
};

class EnumInfo final : public BaseType {
public:
    EnumInfo(const void *ED, const std::string &name, size_t size, size_t typeId);
    void AddEnumerator(const std::string &name, int64_t value);

    const void *ED; // pointer to EnumDecl
    std::vector<std::pair<std::string, int64_t>> Enumerators;
};

#pragma once

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "ReflectionTypes.hpp"

#include <unordered_map>
#include <vector>
#include <string>

namespace clang {
    class RecordDecl;
    class EnumDecl;
    class TypedefNameDecl;
    class FieldDecl;
    class ConstantArrayType;
}

class ReflectClangVisitor : public clang::RecursiveASTVisitor<ReflectClangVisitor> {
public:
    explicit ReflectClangVisitor(clang::ASTContext &context);

    bool VisitRecordDecl(clang::RecordDecl *RD);
    bool VisitEnumDecl(const clang::EnumDecl *ED);
    bool VisitTypedefDecl(const clang::TypedefDecl *TD);

    void MergePendingAliases();
    void ReflectNestedFields(const clang::RecordDecl *nestedRD, const std::string &prefix,
                              RecordInfo &record, size_t baseOffset);

    [[nodiscard]] const std::vector<RecordInfo>& GetResults() const;
    [[nodiscard]] const std::vector<EnumInfo>& GetEnumResults() const;
    [[nodiscard]] const std::unordered_map<std::string, BaseType>& GetBaseTypes() const;

private:
    FieldInfo CreateFieldInfo(const clang::FieldDecl *field, clang::QualType fieldType, size_t offset) const;

    template <typename T>
    void MergePendingAliasesFor(const clang::TagDecl *canonTD, T &target);

    void RegisterBaseTypes();
    void RegisterBaseType(const std::string &name, size_t size);

    clang::ASTContext &Context;
    std::vector<RecordInfo> Results;
    std::vector<EnumInfo> EnumResults;
    size_t NextTypeID;
    std::unordered_map<std::string, BaseType> BaseTypes;
    std::unordered_map<const clang::TagDecl*, std::vector<std::string>> PendingAliases;
};

template <typename T>
void ReflectClangVisitor::MergePendingAliasesFor(const clang::TagDecl *canonTD, T &target) {
    if (const auto it = PendingAliases.find(canonTD); it != PendingAliases.end()) {
        for (const auto &alias : it->second) {
            if (std::find(target.Aliases.begin(), target.Aliases.end(), alias) == target.Aliases.end()) {
                target.Aliases.push_back(alias);
            }
        }
        PendingAliases.erase(it);
    }
}

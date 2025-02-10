#pragma once

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "ReflectVisitor.hpp"
#include <string>

class ReflectClangConsumer final : public clang::ASTConsumer {
public:
    explicit ReflectClangConsumer(clang::ASTContext &context, std::string outputFile);
    void HandleTranslationUnit(clang::ASTContext &context) override;

private:
    ReflectClangVisitor Visitor;
    std::string OutputFile;
};

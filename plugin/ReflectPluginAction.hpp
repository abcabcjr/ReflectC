#pragma once

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTConsumer.h"
#include <memory>
#include <string>

class ReflectClangPluginAction final : public clang::PluginASTAction {
protected:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI,
                                                          llvm::StringRef) override;
    bool ParseArgs(const clang::CompilerInstance &CI,
                   const std::vector<std::string> &args) override;
};

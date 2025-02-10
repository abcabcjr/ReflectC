#include "ReflectPluginAction.hpp"
#include "ReflectConsumer.hpp"
#include "clang/Frontend/CompilerInstance.h"

std::unique_ptr<clang::ASTConsumer> ReflectClangPluginAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef) {

    std::string outputFile = CI.getFrontendOpts().OutputFile.substr(0,
                         CI.getFrontendOpts().OutputFile.find_last_of('.')) + ".reflection.dat";

    return std::make_unique<ReflectClangConsumer>(CI.getASTContext(), outputFile);
}

bool ReflectClangPluginAction::ParseArgs(const clang::CompilerInstance &CI,
                                           const std::vector<std::string> &args) {
    return true;
}

static clang::FrontendPluginRegistry::Add<ReflectClangPluginAction>
X("reflect-clang-plugin", "Reflect struct, union, and enum information");

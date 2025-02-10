#include "ReflectConsumer.hpp"
#include <clang/Basic/TargetInfo.h>

#include "ReflectionDataSerializer.hpp"
#include "clang/AST/ASTContext.h"
#include "llvm/Support/raw_ostream.h"

ReflectClangConsumer::ReflectClangConsumer(clang::ASTContext &context, std::string outputFile)
    : Visitor(context), OutputFile(std::move(outputFile)) {}

void ReflectClangConsumer::HandleTranslationUnit(clang::ASTContext &context) {
    Visitor.TraverseDecl(context.getTranslationUnitDecl());
    Visitor.MergePendingAliases();

    const auto &baseTypesMap = Visitor.GetBaseTypes();
    const auto &enumResults  = Visitor.GetEnumResults();
    const auto &recResults   = Visitor.GetResults();

    // TODO: move this to some function
    const size_t archSize = context.getTargetInfo().getPointerWidth(clang::LangAS::Default) / 8;

    if (const ReflectionDataSerializer serializer(OutputFile); !serializer.serialize(baseTypesMap, recResults, enumResults, archSize)) {
        llvm::errs() << "Error: reflection.dat serialization failed.\n";
    }
}

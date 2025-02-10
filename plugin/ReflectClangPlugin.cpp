#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <regex>
#include <fstream>
#include <vector>

using namespace clang;

enum class TypeVariant {
    Base,
    Struct,
    Union,
    Enum
};

class BaseType {
public:
    BaseType(TypeVariant variant, std::string name, size_t size, size_t typeId)
        : Variant(variant), Name(std::move(name)), Size(size), TypeID(typeId) {
    }

    virtual ~BaseType() = default;

    TypeVariant Variant;
    std::string Name;
    size_t Size;
    size_t TypeID;
    std::vector<std::string> Aliases; // typedefs garbage
};

class FieldInfo {
public:
    FieldInfo(std::string name, std::string type, size_t offset)
        : Name(std::move(name)), Type(std::move(type)), Offset(offset),
          PointerDepth(0), ArraySize(0), IsConst(false), IsStructOrUnion(false) {
    }

    std::string Name;
    std::string Type; // clean, canonical base type name
    size_t Offset;
    int PointerDepth;
    size_t ArraySize; // 0 - not array, otherwise it's the total amount of elements, e.g. int x[10][10] => 100
    bool IsConst;
    bool IsStructOrUnion;
    std::string StructOrUnionName; // for struct/union fields: tag name or typedef alias - TODO: get rid of this
    std::string Alias;
};

class RecordInfo : public BaseType {
public:
    RecordInfo(TypeVariant variant, const std::string &name, size_t size, size_t typeId)
        : BaseType(variant, name, size, typeId), RD(nullptr) {
    }

    void AddField(const FieldInfo &field) {
        Fields.push_back(field);
    }

    std::vector<FieldInfo> Fields;
    const RecordDecl *RD;
};

class EnumInfo : public BaseType {
public:
    EnumInfo(const EnumDecl *ED, const std::string &name, size_t size, size_t typeId)
        : BaseType(TypeVariant::Enum, name, size, typeId), ED(ED) {
    }

    void AddEnumerator(const std::string &name, int64_t value) {
        Enumerators.emplace_back(name, value);
    }

    const EnumDecl *ED;
    std::vector<std::pair<std::string, int64_t>> Enumerators;
};

class ReflectClangVisitor : public RecursiveASTVisitor<ReflectClangVisitor> {
public:
    explicit ReflectClangVisitor(ASTContext &context)
        : Context(context), NextTypeID(1) {
        RegisterBaseTypes();
    }

    bool VisitRecordDecl(RecordDecl *RD) {
        if (!RD->isCompleteDefinition())
            return true;

        const ASTRecordLayout &layout = Context.getASTRecordLayout(RD);
        TypeVariant variant = RD->isStruct() ? TypeVariant::Struct : TypeVariant::Union;

        std::string recName = RD->getNameAsString();
        if (recName.empty()) {
            if (TypedefNameDecl *tnd = RD->getTypedefNameForAnonDecl()) {
                recName = tnd->getNameAsString();
            } else {
                llvm::errs() << "Record name is empty and no typedef alias available.\n";
                return true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(TypeIDMutex);

            RecordInfo record(variant, recName, layout.getSize().getQuantity(), NextTypeID++);
            record.RD = llvm::cast<RecordDecl>(RD->getCanonicalDecl());

            MergePendingAliasesFor(record.RD, record);

            unsigned index = 0;
            for (const FieldDecl *field : RD->fields()) {
                QualType fieldType = field->getType();
                FieldInfo fi = CreateFieldInfo(field, fieldType, layout.getFieldOffset(index) / 8);

                if (!fi.IsStructOrUnion) {
                    if (BaseTypes.find(fi.Type) == BaseTypes.end()) {
                        llvm::errs() << "Warning: Adding unknown base type: " << fi.Type << " with size 0\n";
                        RegisterBaseType(fi.Type, 0, false);
                    }
                    
                    if (!fi.Alias.empty() && fi.Alias != fi.Type) {
                        if (BaseTypes.find(fi.Alias) == BaseTypes.end()) {
                            size_t size = 0;
                            auto itCanon = BaseTypes.find(fi.Type);
                            if (itCanon != BaseTypes.end())
                                size = itCanon->second.Size;
                            //llvm::errs() << "Registering alias base type: " << fi.Alias
                            //        << " for canonical type " << fi.Type << "\n";
                            RegisterBaseType(fi.Alias, size, false);
                        }
                    }
                }

                record.AddField(fi);

                if (const RecordType *RT = fieldType->getAs<RecordType>()) {
                    RecordDecl *nestedRD = RT->getDecl();
                    if (nestedRD->isCompleteDefinition()) {
                        auto name = field->getNameAsString();
                        ReflectNestedFields(nestedRD, name.empty() ? "" : (name + "."),
                                            record, layout.getFieldOffset(index) / 8);
                    }
                }
                ++index;
            }

            Results.push_back(record);
            BaseTypes.emplace(record.Name, record);
        }
        return true;
    }

    bool VisitEnumDecl(const EnumDecl *ED) {
        if (!ED->isCompleteDefinition())
            return true;

        std::lock_guard<std::mutex> lock(TypeIDMutex);

        std::string enumName = ED->getNameAsString();
        if (enumName.empty()) {
            if (const TypedefNameDecl *Typedef = ED->getTypedefNameForAnonDecl()) {
                enumName = Typedef->getNameAsString();
            }
        }

        if (enumName.empty()) {
            llvm::errs() << "Warning: Enum has no name and no typedef alias.\n";
            return true;
        }

        QualType enumType = Context.getTypeDeclType(ED);
        size_t enumSize = Context.getTypeSizeInChars(enumType).getQuantity();

        EnumInfo en(ED->getCanonicalDecl(), enumName, enumSize, NextTypeID++);
        MergePendingAliasesFor(ED->getCanonicalDecl(), en);

        for (const auto *e : ED->enumerators()) {
            en.AddEnumerator(e->getNameAsString(), e->getInitVal().getExtValue());
        }

        EnumResults.push_back(en);
        BaseTypes.emplace(enumName, en);

        return true;
    }

    bool VisitTypedefDecl(const TypedefDecl *TD) {
        const QualType underlying = TD->getUnderlyingType();
        const QualType canon = underlying.getCanonicalType();

        if (const auto *TT = canon->getAs<TagType>()) {
            const TagDecl *tag = TT->getDecl()->getCanonicalDecl();
            PendingAliases[tag].push_back(TD->getNameAsString());
        }
        return true;
    }

    void MergePendingAliases() {
        for (auto &pair : PendingAliases) {
            const TagDecl *canonTD = pair.first;
            const std::vector<std::string> &aliases = pair.second;

            for (auto &record : Results) {
                if (record.RD == canonTD) {
                    for (const auto &alias : aliases) {
                        if (std::find(record.Aliases.begin(), record.Aliases.end(), alias) == record.Aliases.end()) {
                            record.Aliases.push_back(alias);
                        }
                    }
                }
            }

            for (auto &en : EnumResults) {
                if (en.ED == canonTD) {
                    for (const auto &alias : aliases) {
                        if (std::find(en.Aliases.begin(), en.Aliases.end(), alias) == en.Aliases.end()) {
                            en.Aliases.push_back(alias);
                        }
                    }
                }
            }
        }
        PendingAliases.clear();
    }

    void ReflectNestedFields(const RecordDecl *nestedRD, const std::string &prefix,
                             RecordInfo &record, size_t baseOffset);

    const std::vector<RecordInfo> &GetResults() const {
        return Results;
    }

    const std::vector<EnumInfo> &GetEnumResults() const {
        return EnumResults;
    }

    const std::unordered_map<std::string, BaseType> &GetBaseTypes() const {
        return BaseTypes;
    }

private:
    FieldInfo CreateFieldInfo(const FieldDecl *field, QualType fieldType, size_t offset) const {
        std::string aliasName;
        if (field->getTypeSourceInfo()) {
            TypeLoc tl = field->getTypeSourceInfo()->getTypeLoc();
            while (true) {
                if (const auto tdtl = tl.getAs<TypedefTypeLoc>()) {
                    aliasName = tdtl.getTypedefNameDecl()->getNameAsString();
                    break;
                }
                if (const auto etl = tl.getAs<ElaboratedTypeLoc>()) {
                    tl = etl.getNamedTypeLoc();
                    continue;
                }
                break;
            }
        }

        QualType canonicalType = fieldType.getDesugaredType(Context);
        int pointerDepth = 0;
        uint64_t totalElements = 1;
        QualType underlyingType;

        if (const ConstantArrayType *cat = Context.getAsConstantArrayType(canonicalType)) {
            QualType arrType = canonicalType;
            while (const ConstantArrayType *cat2 = Context.getAsConstantArrayType(arrType)) {
                totalElements *= cat2->getSize().getZExtValue();
                arrType = cat2->getElementType();
            }

            QualType peeled = arrType;
            while (const auto *pt = peeled->getAs<PointerType>()) {
                pointerDepth++;
                peeled = pt->getPointeeType();
            }
            underlyingType = peeled;
        } else {
            QualType peeled = canonicalType;
            while (const auto *pt = peeled->getAs<PointerType>()) {
                pointerDepth++;
                peeled = pt->getPointeeType();
            }
            underlyingType = peeled;
        }

        FieldInfo info(field->getNameAsString(), "", offset);
        info.PointerDepth = pointerDepth;
        info.ArraySize = (totalElements > 1 ? totalElements : 0);
        info.IsConst = underlyingType.isConstQualified();
        info.Alias = aliasName;

        QualType finalCanonical = underlyingType.getCanonicalType().getUnqualifiedType();
        std::string cleanName = finalCanonical.getAsString();

        static const std::string structPrefix = "struct ";
        static const std::string unionPrefix  = "union ";
        static const std::string enumPrefix   = "enum ";
        if (cleanName.compare(0, structPrefix.size(), structPrefix) == 0) {
            cleanName = cleanName.substr(structPrefix.size());
        } else if (cleanName.compare(0, unionPrefix.size(), unionPrefix) == 0) {
            cleanName = cleanName.substr(unionPrefix.size());
        } else if (cleanName.compare(0, enumPrefix.size(), enumPrefix) == 0) {
            cleanName = cleanName.substr(enumPrefix.size());
        }

        if (finalCanonical->isRecordType()) {
            if (const RecordType *rt = finalCanonical->getAs<RecordType>()) {
                const RecordDecl *rd = rt->getDecl();
                std::string tagName = rd->getNameAsString();
                if (tagName.empty()) {
                    if (TypedefNameDecl *tnd = rd->getTypedefNameForAnonDecl()) {
                        tagName = tnd->getNameAsString();
                    }
                }
                info.IsStructOrUnion = true;
                info.StructOrUnionName = tagName;
                cleanName = tagName;
            }
        }

        info.Type = cleanName;
        return info;
    }

    template <typename T>
    void MergePendingAliasesFor(const TagDecl *canonTD, T &target) {
        auto it = PendingAliases.find(canonTD);
        if (it != PendingAliases.end()) {
            for (const auto &alias : it->second) {
                if (std::find(target.Aliases.begin(), target.Aliases.end(), alias) == target.Aliases.end()) {
                    target.Aliases.push_back(alias);
                }
            }
            PendingAliases.erase(it);
        }
    }

    void RegisterBaseTypes() {
        size_t archSize = Context.getTargetInfo().getPointerWidth(LangAS::Default) / 8;
        RegisterBaseType("void", 0);
        RegisterBaseType("int", 4);
        RegisterBaseType("unsigned int", 4);
        RegisterBaseType("float", 4);
        RegisterBaseType("double", 8);
        RegisterBaseType("char", 1);
        RegisterBaseType("unsigned char", 1);
        RegisterBaseType("short", 2);
        RegisterBaseType("unsigned short", 2);
        RegisterBaseType("long", 4);
        RegisterBaseType("unsigned long", 4);
        RegisterBaseType("_Bool", 1);
        RegisterBaseType("bool", 1);
        RegisterBaseType("unsigned long long", archSize);
        RegisterBaseType("long long", archSize);
    }

    void RegisterBaseType(const std::string &name, size_t size, bool lockMutex = true) {
        if (lockMutex) {
            std::lock_guard<std::mutex> lock(TypeIDMutex);
        }
        BaseType newBase(TypeVariant::Base, name, size, NextTypeID++);
        BaseTypes.emplace(name, newBase);
    }

    ASTContext &Context;
    std::vector<RecordInfo> Results;
    std::vector<EnumInfo> EnumResults;

    std::atomic<size_t> NextTypeID;
    std::mutex TypeIDMutex;

    std::unordered_map<std::string, BaseType> BaseTypes;

    std::unordered_map<const TagDecl*, std::vector<std::string>> PendingAliases;
};

void ReflectClangVisitor::ReflectNestedFields(const RecordDecl *nestedRD,
                                              const std::string &prefix,
                                              RecordInfo &record,
                                              size_t baseOffset) {
    const ASTRecordLayout &nestedLayout = Context.getASTRecordLayout(nestedRD);
    unsigned nestedIndex = 0;
    for (const FieldDecl *nestedField : nestedRD->fields()) {
        QualType nestedFieldType = nestedField->getType();
        FieldInfo nestedFieldInfo =
            CreateFieldInfo(nestedField, nestedFieldType,
                            baseOffset + (nestedLayout.getFieldOffset(nestedIndex) / 8));
        nestedFieldInfo.Name = prefix + nestedField->getNameAsString();
        record.AddField(nestedFieldInfo);

        if (const RecordType *nestedRT = nestedFieldType->getAs<RecordType>()) {
            if (const RecordDecl *nestedNestedRD = nestedRT->getDecl();
                nestedNestedRD->isCompleteDefinition()) {

                // check if record is anon
                auto newPrefix = nestedField->getNameAsString().empty() ? prefix : (prefix + nestedField->getNameAsString() + ".");

                ReflectNestedFields(nestedNestedRD,
                                    newPrefix,
                                    record,
                                    baseOffset + (nestedLayout.getFieldOffset(nestedIndex) / 8));
            }
        }
        ++nestedIndex;
    }
}

class ReflectClangConsumer : public ASTConsumer {
public:
    explicit ReflectClangConsumer(ASTContext &context, std::string outputFile)
        : Visitor(context), OutputFile(std::move(outputFile)) {
    }

    void HandleTranslationUnit(ASTContext &context) override {
        Visitor.TraverseDecl(context.getTranslationUnitDecl());
        Visitor.MergePendingAliases();

        std::ofstream outFile(OutputFile, std::ios::out);
        if (!outFile.is_open()) {
            llvm::errs() << "Error: Could not open file " << OutputFile << " for writing\n";
            return;
        }

        const auto &baseTypesMap = Visitor.GetBaseTypes();
        const auto &enumResults  = Visitor.GetEnumResults();
        const auto &recResults   = Visitor.GetResults();
        const size_t archSize    = context.getTargetInfo().getPointerWidth(LangAS::Default) / 8;

        outFile << "arch " << archSize << "\n";

        for (const auto &pair : baseTypesMap) {
            const BaseType &bt = pair.second;
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

        for (const auto &rec : recResults) {
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

        for (const auto &en : enumResults) {
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
    }

private:
    ReflectClangVisitor Visitor;
    std::string OutputFile;
};

class ReflectClangPluginAction : public PluginASTAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
        std::string outputFile =
            CI.getFrontendOpts().OutputFile.substr(0,
               CI.getFrontendOpts().OutputFile.find_last_of('.')) + ".reflection.dat";
        return std::make_unique<ReflectClangConsumer>(CI.getASTContext(), outputFile);
    }

    bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &Args) override {
        return true;
    }
};

static FrontendPluginRegistry::Add<ReflectClangPluginAction>
X("reflect-clang-plugin", "Reflect struct, union, and enum information");

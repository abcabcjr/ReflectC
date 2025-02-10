#include "ReflectVisitor.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "llvm/Support/raw_ostream.h"
#include <clang/Basic/TargetInfo.h>

using namespace clang;

ReflectClangVisitor::ReflectClangVisitor(ASTContext &context)
    : Context(context), NextTypeID(1)
{
    RegisterBaseTypes();
}

bool ReflectClangVisitor::VisitRecordDecl(RecordDecl *RD) {
    // skip forward decls
    if (!RD->isCompleteDefinition())
        return true;

    const ASTRecordLayout &layout = Context.getASTRecordLayout(RD);
    TypeVariant variant = RD->isStruct() ? TypeVariant::Struct : TypeVariant::Union;

    std::string recName = RD->getNameAsString();
    if (recName.empty()) {
        if (TypedefNameDecl *tnd = RD->getTypedefNameForAnonDecl()) {
            recName = tnd->getNameAsString();
        } else {
            llvm::errs() << "Warning: Record name is empty and no typedef alias available.\n";
            return true;
        }
    }

    RecordInfo record(variant, recName, layout.getSize().getQuantity(), NextTypeID++);
    record.RD = static_cast<const void*>(RD->getCanonicalDecl());

    MergePendingAliasesFor(RD->getCanonicalDecl(), record);

    unsigned index = 0;
    for (const FieldDecl *field : RD->fields()) {
        QualType fieldType = field->getType();
        FieldInfo fi = CreateFieldInfo(field, fieldType, layout.getFieldOffset(index) / 8);

        if (!fi.IsStructOrUnion) {
            if (BaseTypes.find(fi.Type) == BaseTypes.end()) {
                llvm::errs() << "Warning: Adding unknown base type: " << fi.Type << " with size 0\n";
                RegisterBaseType(fi.Type, 0);
            }

            // Base type typedefs are added to BaseTypes here as new base types
            // Honestly not too sure if this should be kept like this
            if (!fi.Alias.empty() && fi.Alias != fi.Type) {
                if (BaseTypes.find(fi.Alias) == BaseTypes.end()) {
                    size_t size = 0;
                    if (auto itCanon = BaseTypes.find(fi.Type); itCanon != BaseTypes.end())
                        size = itCanon->second.Size;
                    RegisterBaseType(fi.Alias, size);
                }
            }
        }

        record.AddField(fi);

        if (const RecordType *RT = fieldType->getAs<RecordType>()) {
            // Nested field logic
            if (RecordDecl *nestedRD = RT->getDecl(); nestedRD->isCompleteDefinition()) {
                // If we have an anonymous record, its nested fields are "flattened"
                std::string prefix = field->getNameAsString().empty() ? "" : (field->getNameAsString() + ".");
                ReflectNestedFields(nestedRD, prefix, record, layout.getFieldOffset(index) / 8);
            }
        }
        ++index;
    }

    Results.push_back(record);
    BaseTypes.emplace(record.Name, record);

    return true;
}

bool ReflectClangVisitor::VisitEnumDecl(const EnumDecl *ED) {
    if (!ED->isCompleteDefinition())
        return true;

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

    const QualType enumType = Context.getTypeDeclType(ED);
    const size_t enumSize = Context.getTypeSizeInChars(enumType).getQuantity();

    EnumInfo en(ED->getCanonicalDecl(), enumName, enumSize, NextTypeID++);
    MergePendingAliasesFor(ED->getCanonicalDecl(), en);

    for (const auto *e : ED->enumerators()) {
        en.AddEnumerator(e->getNameAsString(), e->getInitVal().getExtValue());
    }

    EnumResults.push_back(en);
    BaseTypes.emplace(enumName, en);

    return true;
}

bool ReflectClangVisitor::VisitTypedefDecl(const TypedefDecl *TD) {
    const QualType underlying = TD->getUnderlyingType();
    const QualType canon = underlying.getCanonicalType();

    if (const auto *TT = canon->getAs<TagType>()) {
        const TagDecl *tag = TT->getDecl()->getCanonicalDecl();
        PendingAliases[tag].push_back(TD->getNameAsString());
    }
    return true;
}

void ReflectClangVisitor::MergePendingAliases() {
    // Process any pending aliases that were not merged already.
    for (auto &[canon, aliases] : PendingAliases) {
        const TagDecl *canonTD = canon;

        for (auto &record : Results) {
            if (record.RD == static_cast<const void*>(canonTD)) {
                for (const auto &alias : aliases) {
                    if (std::find(record.Aliases.begin(), record.Aliases.end(), alias) == record.Aliases.end()) {
                        record.Aliases.push_back(alias);
                    }
                }
            }
        }

        for (auto &en : EnumResults) {
            if (en.ED == static_cast<const void*>(canonTD)) {
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

void ReflectClangVisitor::ReflectNestedFields(const RecordDecl *nestedRD, const std::string &prefix,
                                                RecordInfo &record, size_t baseOffset) {
    const ASTRecordLayout &nestedLayout = Context.getASTRecordLayout(nestedRD);
    unsigned nestedIndex = 0;
    for (const FieldDecl *nestedField : nestedRD->fields()) {
        const QualType nestedFieldType = nestedField->getType();

        FieldInfo nestedFieldInfo = CreateFieldInfo(nestedField, nestedFieldType,
            baseOffset + (nestedLayout.getFieldOffset(nestedIndex) / 8)); // field offset is in bits

        nestedFieldInfo.Name = prefix + nestedField->getNameAsString();
        record.AddField(nestedFieldInfo);

        if (const RecordType *nestedRT = nestedFieldType->getAs<RecordType>()) {
            if (const RecordDecl *nestedNestedRD = nestedRT->getDecl();
                nestedNestedRD->isCompleteDefinition()) {
                // anon struct handling again
                std::string newPrefix = nestedField->getNameAsString().empty() ? prefix :
                                         (prefix + nestedField->getNameAsString() + ".");
                ReflectNestedFields(nestedNestedRD, newPrefix, record,
                    baseOffset + (nestedLayout.getFieldOffset(nestedIndex) / 8));
            }
        }
        ++nestedIndex;
    }
}

const std::vector<RecordInfo>& ReflectClangVisitor::GetResults() const {
    return Results;
}

const std::vector<EnumInfo>& ReflectClangVisitor::GetEnumResults() const {
    return EnumResults;
}

const std::unordered_map<std::string, BaseType>& ReflectClangVisitor::GetBaseTypes() const {
    return BaseTypes;
}

FieldInfo ReflectClangVisitor::CreateFieldInfo(const FieldDecl *field, QualType fieldType, size_t offset) const {
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

void ReflectClangVisitor::RegisterBaseTypes() {
    // Determine pointer size (in bytes) for the current target
    const size_t archSize = Context.getTargetInfo().getPointerWidth(LangAS::Default) / 8;

    RegisterBaseType("size_t", archSize);
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
    RegisterBaseType("unsigned long long", 8);
    RegisterBaseType("long long", 8);
}

void ReflectClangVisitor::RegisterBaseType(const std::string &name, size_t size) {
    BaseType newBase(TypeVariant::Base, name, size, NextTypeID++);
    BaseTypes.emplace(name, newBase);
}

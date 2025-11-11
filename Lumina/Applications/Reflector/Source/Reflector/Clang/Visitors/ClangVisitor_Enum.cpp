#include "ClangVisitor.h"
#include "Reflector/Clang/ClangParserContext.h"
#include "Reflector/Clang/Utils.h"
#include "Reflector/Types/ReflectedType.h"

namespace Lumina::Reflection::Visitor
{
    static CXChildVisitResult VisitEnumContents(CXCursor Cursor, CXCursor parent, CXClientData pClientData)
    {
        FClangParserContext* pContext = static_cast<FClangParserContext*>(pClientData);
        FReflectedEnum* Enum = (FReflectedEnum*)pContext->ParentReflectedType;
        
        CXCursorKind kind = clang_getCursorKind(Cursor);
        
        if (kind == CXCursor_EnumConstantDecl)
        {
            eastl::string DisplayName = ClangUtils::GetCursorDisplayName(Cursor);
            const clang::EnumConstantDecl* EnumConstantDecl = (const clang::EnumConstantDecl*)Cursor.data[0];

            const llvm::APSInt& initVal = EnumConstantDecl->getInitVal();
            uint32_t Value = (int32_t)initVal.getExtValue();
            
            FReflectedEnum::FConstant Constant;
            Constant.Label = DisplayName;
            Constant.ID = eastl::string(DisplayName);
            Constant.Value = Value;
            
            const CXString CommentString = clang_Cursor_getBriefCommentText(Cursor);
            if (CommentString.data != nullptr)
            {
                Constant.Description = clang_getCString(CommentString);
            }
            clang_disposeString(CommentString);

            Enum->AddConstant(Constant);
        }

        return CXChildVisit_Continue;
    }
    
    CXChildVisitResult VisitEnum(CXCursor Cursor, CXCursor Parent, FClangParserContext* Context)
    {
        eastl::string CursorName = ClangUtils::GetCursorDisplayName(Cursor);

        void* Data = clang_getCursorType(Cursor).data[0];
        if(Data == nullptr)
        {
            return CXChildVisit_Break;
        }
        
        eastl::string FullyQualifiedName;
        if (!ClangUtils::GetQualifiedNameForType(clang::QualType::getFromOpaquePtr(Data), FullyQualifiedName))
        {
            return CXChildVisit_Break;
        }

        
        FReflectionMacro Macro;
        if(!Context->TryFindMacroForCursor(Context->ReflectedHeader->HeaderPath, Cursor, Macro))
        {
            return CXChildVisit_Continue;
        }

        if(Macro.Type != EReflectionMacro::Enum)
        {
            return CXChildVisit_Continue;
        }
        
        const clang::EnumDecl* pEnumDecl = (const clang::EnumDecl*) Cursor.data[0];
        clang::QualType IntegerType = pEnumDecl->getIntegerType();

        if (IntegerType.isNull())
        {
            return CXChildVisit_Break;
        }
        
        FReflectedEnum* ReflectedEnum = Context->ReflectionDatabase.GetOrCreateReflectedType<FReflectedEnum>(FStringHash(FullyQualifiedName));
        ReflectedEnum->DisplayName = CursorName;
        ReflectedEnum->Project = Context->Project->Name;
        ReflectedEnum->Type = FReflectedType::EType::Enum;
        ReflectedEnum->LineNumber = ClangUtils::GetCursorLineNumber(Cursor);
        ReflectedEnum->HeaderID = Context->ReflectedHeader->HeaderPath;
        ReflectedEnum->GenerateMetadata(Macro.MacroContents);

        if (!Context->CurrentNamespace.empty())
        {
            ReflectedEnum->Namespace = Context->CurrentNamespace;
        }
        
        FReflectedType* PreviousParentType = Context->ParentReflectedType;
        
        Context->ParentReflectedType = ReflectedEnum;
        if (!Context->bInitialPass)
        {
            clang_visitChildren(Cursor, VisitEnumContents, Context);
        }
        Context->ParentReflectedType = PreviousParentType;
        
        
        Context->ReflectionDatabase.AddReflectedType(ReflectedEnum);
        
        return CXChildVisit_Continue;

    }
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "uLang/Semantics/DataDefinition.h"
#include "uLang/Semantics/SemanticClass.h"
#include "uLang/Semantics/SemanticFunction.h"
#include "uLang/Semantics/SemanticInterface.h"
#include "uLang/Semantics/SemanticProgram.h"
#include "uLang/Semantics/SemanticScope.h"

namespace uLang
{
void CDataDefinition::SetAstNode(CExprDefinition* AstNode)
{
    CDefinition::SetAstNode(AstNode);
}
CExprDefinition* CDataDefinition::GetAstNode() const
{
    return static_cast<CExprDefinition*>(CDefinition::GetAstNode());
}

void CDataDefinition::SetIrNode(CExprDefinition* AstNode)
{
    CDefinition::SetIrNode(AstNode);
}
CExprDefinition* CDataDefinition::GetIrNode(bool bForce) const
{
    return static_cast<CExprDefinition*>(CDefinition::GetIrNode(bForce));
}

CUTF8String CDataDefinition::GetScopePath(uLang::UTF8Char SeparatorChar, CScope::EPathMode Mode) const
{
    CUTF8String EnclosingScopePath = _EnclosingScope.GetScopePath(SeparatorChar, Mode);
    if (EnclosingScopePath.IsEmpty())
    {
        return AsNameStringView();
    }
    return CUTF8String("%s.%s", *EnclosingScopePath, AsNameCString());
}

bool CDataDefinition::HasInitializer() const
{
    const CExprDefinition* CurExpr = GetAstNode();
    /*
    NOTE: (YiLiangSiew) We guard against cases where the AST node is `nullptr`, for example, for the
    following syntax:

    Foo(ParamInt:int, stub{UnnamedParameter}:stub{ type }):void =
        return

    Bar():void =
        Foo(5)
        return

    When `AnalyzeInvocation` is called to check for default parameters, the second parameter will not
    have a valid AST node for it so this would crash otherwise.
     */
    if (CurExpr == nullptr)
    {
        return false;
    }
    return CurExpr->Value().IsValid();
}

bool CDataDefinition::IsVarWritableFrom(const CScope& Scope) const
{
    const CDataDefinition& Definition = GetDefinitionVarAccessibilityRoot();
    return Scope.CanAccess(Definition, Definition.DerivedVarAccessLevel());
}

bool CDataDefinition::IsModuleScopedVar() const
{
    if (!IsVar())
    {
        return false;
    }
    if (_EnclosingScope.GetLogicalScope().GetKind() != CScope::EKind::Module)
    {
        return false;
    }
    return true;
}

void CDataDefinition::MarkPersistenceCompatConstraint() const
{
    if (IsPersistenceCompatConstraint())
    {
        return;
    }
    _bPersistenceCompatConstraint = true;
    if (const CModule* EnclosingModule = _EnclosingScope.GetModule())
    {
        EnclosingModule->MarkPersistenceCompatConstraint();
    }
}

bool CDataDefinition::IsPersistenceCompatConstraint() const
{
    return _bPersistenceCompatConstraint;
}

bool CDataDefinition::CanHaveCustomAccessors() const
{
    return IsVar()
        && _EnclosingScope.GetLogicalScope().GetKind() == uLang::CScope::EKind::Class
        && GetType()->GetNormalType().AsChecked<uLang::CPointerType>().NegativeValueType()->CanBeCustomAccessorDataType();
}
}    // namespace uLang

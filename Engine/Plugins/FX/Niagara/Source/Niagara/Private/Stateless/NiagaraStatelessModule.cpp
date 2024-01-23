// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessModule.h"

#if DO_CHECK
void FNiagaraStatelessSetShaderParameterContext::ValidateIncludeStructType(uint32 StructOffset, const FShaderParametersMetadata* StructMetaData) const
{
	for (const FShaderParametersMetadata::FMember& Member : ShaderParametersMetadata->GetMembers())
	{
		if (Member.GetOffset() != StructOffset)
		{
			continue;
		}

		if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT && Member.GetStructMetadata()->GetLayout() == StructMetaData->GetLayout())
		{
			return;
		}

		UE_LOG(LogNiagara, Fatal, TEXT("Shader parameter struct member (%s) at offset (%u) is not of type (%s)"), Member.GetName(), StructOffset, StructMetaData->GetStructTypeName());
		return;
	}

	UE_LOG(LogNiagara, Fatal, TEXT("Failed to find shader parameter struct member type (%s) at offset (%u)"), StructMetaData->GetStructTypeName(), StructOffset);
}
#endif //DO_CHECK

#if WITH_EDITOR
bool UNiagaraStatelessModule::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty != nullptr)
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraStatelessModule, bModuleEnabled))
		{
			return CanDisableModule();
		}
		else if (CanDisableModule() && !IsModuleEnabled())
		{
			return false;
		}

		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraStatelessModule, bDebugDrawEnabled))
		{
			return CanDebugDraw();
		}
	}

	return true;
}
#endif //WITH_EDITOR

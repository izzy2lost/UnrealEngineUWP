// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessModule.h"

#if WITH_EDITORONLY_DATA
const FName UNiagaraStatelessModule::PrivateMemberNames::bModuleEnabled = GET_MEMBER_NAME_CHECKED(UNiagaraStatelessModule, bModuleEnabled);
const FName UNiagaraStatelessModule::PrivateMemberNames::bDebugDrawEnabled = GET_MEMBER_NAME_CHECKED(UNiagaraStatelessModule, bDebugDrawEnabled);
#endif

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

void UNiagaraStatelessModule::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FStructProperty* StructProperty = CastField<FStructProperty>(PropertyChangedEvent.MemberProperty);
	if (StructProperty != nullptr && StructProperty->Struct->IsChildOf(FNiagaraDistributionBase::StaticStruct()))
	{
		FNiagaraDistributionBase* ValuePtr = StructProperty->ContainerPtrToValuePtr<FNiagaraDistributionBase>(this);
		if (ValuePtr != nullptr)
		{
			ValuePtr->UpdateValuesFromDistribution();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR

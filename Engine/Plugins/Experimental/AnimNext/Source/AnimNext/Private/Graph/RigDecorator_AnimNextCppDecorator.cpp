// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigDecorator_AnimNextCppDecorator.h"

#if WITH_EDITOR
#include "RigVMModel/RigVMController.h"
#include "DecoratorBase/DecoratorRegistry.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigDecorator_AnimNextCppDecorator)

#if WITH_EDITOR
void FRigDecorator_AnimNextCppDecorator::GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, FRigVMPinInfoArray& OutPinArray) const
{
	if (DecoratorSharedDataStruct == nullptr)
	{
		return;
	}

	FStructOnScope DefaultValueMemoryScope(DecoratorSharedDataStruct);

	OutPinArray.AddPins(DecoratorSharedDataStruct, InController, ERigVMPinDirection::Invalid, InParentPinIndex, DefaultValueMemoryScope.GetStructMemory(), true);
}

const UE::AnimNext::FDecorator* FRigDecorator_AnimNextCppDecorator::GetDecorator() const
{
	return UE::AnimNext::FDecoratorRegistry::Get().Find(DecoratorSharedDataStruct);
}
#endif

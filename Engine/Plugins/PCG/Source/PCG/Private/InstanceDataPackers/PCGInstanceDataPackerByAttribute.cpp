// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataPackers/PCGInstanceDataPackerByAttribute.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGSpatialData.h"
#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInstanceDataPackerByAttribute)

#define LOCTEXT_NAMESPACE "PCGInstanceDataPackerByAttribute"

#if WITH_EDITOR
void UPCGInstanceDataPackerByAttribute::PostLoad()
{
	Super::PostLoad();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (FName AttributeName : AttributeNames)
	{
		FPCGAttributePropertyInputSelector& Selector = AttributeSelectors.Emplace_GetRef();
		Selector.SetAttributeName(AttributeName);
	}

	AttributeNames.Reset();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

void UPCGInstanceDataPackerByAttribute::PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const
{
	if (!InSpatialData || !InSpatialData->Metadata)
	{
		PCGLog::InputOutput::LogInvalidInputDataError(&Context);
		return;
	}

	TArray<TUniquePtr<const IPCGAttributeAccessor>> SelectedAccessors;
	TArray<TUniquePtr<const IPCGAttributeAccessorKeys>> SelectedKeys;

	SelectedAccessors.Reserve(AttributeSelectors.Num());
	SelectedKeys.Reserve(AttributeSelectors.Num());

	// Find attributes and calculate NumCustomDataFloats
	for (const FPCGAttributePropertyInputSelector& Selector : AttributeSelectors)
	{
		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InSpatialData, Selector);
		TUniquePtr<const IPCGAttributeAccessorKeys> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(InstanceList.InstancesMetadataEntry);

		if (!Accessor.IsValid() || !Keys.IsValid())
		{
			PCGLog::Metadata::LogFailToCreateAccessor(Selector, &Context);
			continue;
		}

		if (!AddTypeToPacking(Accessor->GetUnderlyingType(), OutPackedCustomData))
		{
			PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("AttributeInvalidType", "Attribute/property '{0}' is not a valid type - skipped."), Selector.GetDisplayText()), &Context);
			continue;
		}

		SelectedAccessors.Add(std::move(Accessor));
		SelectedKeys.Add(std::move(Keys));
	}

	PackCustomDataFromAccessors(InstanceList, std::move(SelectedAccessors), std::move(SelectedKeys), OutPackedCustomData);
}

#undef LOCTEXT_NAMESPACE

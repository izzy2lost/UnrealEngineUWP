// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGMetadataHelpers.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"	
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGMetadataHelpers"

namespace PCGMetadataHelpers
{
	bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2)
	{
		return Metadata1 && Metadata2 && Metadata1->GetRoot() == Metadata2->GetRoot();
	}

	const UPCGMetadata* GetParentMetadata(const UPCGMetadata* Metadata)
	{
		check(Metadata);
		TWeakObjectPtr<const UPCGMetadata> Parent = Metadata->GetParentPtr();

		// We're expecting the parent to either be null, or to be valid - if not, then it has been deleted
		// which is going to cause some issues.
		//check(Parent.IsExplicitlyNull() || Parent.IsValid());
		return Parent.Get();
	}

	const UPCGMetadata* GetConstMetadata(const UPCGData* InData)
	{
		return InData ? InData->ConstMetadata() : nullptr;
	}

	UPCGMetadata* GetMutableMetadata(UPCGData* InData)
	{
		return InData ? InData->MutableMetadata() : nullptr;
	}

	bool CreateObjectPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter)
	{
		if (!InAttributeBase)
		{
			return false;
		}

		if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				FString Path = static_cast<const FPCGMetadataAttribute<FString>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
				OutSoftObjectPath = FSoftObjectPath(Path);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftObjectPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}

		return false;
	}

	bool CreateObjectOrClassPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter)
	{
		if (!InAttributeBase)
		{
			return false;
		}

		if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				FString Path = static_cast<const FPCGMetadataAttribute<FString>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
				OutSoftObjectPath = FSoftObjectPath(Path);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftObjectPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}
		else if (InAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FSoftClassPath>::Id)
		{
			OutGetter = [InAttributeBase](int64 InMetadataKey, FSoftObjectPath& OutSoftObjectPath)
			{
				OutSoftObjectPath = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(InAttributeBase)->GetValueFromItemKey(InMetadataKey);
			};

			return true;
		}

		return false;
	}

	bool CopyAttributes(UPCGData* TargetData, const UPCGData* SourceData, const TArray<TPair<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector>>& AttributeSelectors, bool bSameOrigin, FPCGContext* OptionalContext)
	{
		check(TargetData && SourceData);
		const UPCGMetadata* SourceMetadata = SourceData->ConstMetadata();
		UPCGMetadata* TargetMetadata = TargetData->MutableMetadata();

		if (!SourceMetadata || !TargetMetadata)
		{
			return false;
		}

		bool bSuccess = false;

		for (const auto& SelectorPair : AttributeSelectors)
		{
			const FPCGAttributePropertyInputSelector& InputSource = SelectorPair.Key;
			const FPCGAttributePropertyOutputSelector& OutputTarget = SelectorPair.Value;

			const FName LocalSourceAttribute = InputSource.GetName();
			const FName LocalDestinationAttribute = OutputTarget.GetName();

			if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute && !SourceMetadata->HasAttribute(LocalSourceAttribute))
			{
				PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("InputMissingAttribute", "Input does not have the '{0}' attribute"), FText::FromName(LocalSourceAttribute)), OptionalContext);
				continue;
			}

			// We need accessors if we have a multi entry source attribute or we have extractors
			const bool bIsMultiEntries = SourceData->IsA<UPCGParamData>() && SourceMetadata->GetLocalItemCount() > 1;
			const bool bInputHasAnyExtra = !InputSource.GetExtraNames().IsEmpty();
			const bool bOutputHasAnyExtra = !OutputTarget.GetExtraNames().IsEmpty();
			const bool bSourceIsAttribute = InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute;
			const bool bTargetIsAttribute = OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute;

			const bool bNeedAccessors = bIsMultiEntries || bInputHasAnyExtra || bOutputHasAnyExtra || !bSourceIsAttribute || !bTargetIsAttribute;

			// If no accessor, copy over the attribute
			if (!bNeedAccessors)
			{
				if (bSameOrigin && LocalSourceAttribute == LocalDestinationAttribute)
				{
					// Nothing to do if we try to copy an attribute into itself in the original data.
					continue;
				}

				const FPCGMetadataAttributeBase* SourceAttribute = SourceMetadata->GetConstAttribute(LocalSourceAttribute);
				// Presence of attribute was already checked before, this should not return null
				check(SourceAttribute);

				// Copy the attribute using the first entry of the source attribute as the default value (there is just a single entry or none). If there is no first entry, will be the default value anyway.
				auto CreateAttribute = [TargetMetadata, SourceAttribute, LocalDestinationAttribute](auto Dummy) -> FPCGMetadataAttributeBase*
				{
					using AttributeType = decltype(Dummy);
					AttributeType DefaultValue = static_cast<const FPCGMetadataAttribute<AttributeType>*>(SourceAttribute)->GetValue(PCGMetadataEntryKey(0));
					return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(TargetMetadata, LocalDestinationAttribute, DefaultValue);
				};

				if (!PCGMetadataAttribute::CallbackWithRightType(SourceAttribute->GetTypeId(), std::move(CreateAttribute)))
				{
					PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailedCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)));
					continue;
				}
			}
			else // Create a new attribute of the accessed field's type manually
			{
				TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, InputSource);
				TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, InputSource);

				if (!InputAccessor.IsValid() || !InputKeys.IsValid())
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor or iterator"), OptionalContext);
					continue;
				}

				// If the target is an attribute, only create a new one if the attribute we don't have any extra.
				// If it has any extra, it will try to write to it.
				if (!bOutputHasAnyExtra && bTargetIsAttribute)
				{
					auto CreateAttribute = [TargetMetadata, LocalDestinationAttribute](auto Dummy)
					{
						using AttributeType = decltype(Dummy);
						return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(TargetMetadata, LocalDestinationAttribute) != nullptr;
					};

					if (!PCGMetadataAttribute::CallbackWithRightType(InputAccessor->GetUnderlyingType(), CreateAttribute))
					{
						PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)), OptionalContext);
						continue;
					}
				}

				TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TargetData, OutputTarget);
				TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(TargetData, OutputTarget);

				if (!OutputAccessor.IsValid() || !OutputKeys.IsValid())
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("FailedToCreateOutputAccessor", "Failed to create output accessor or iterator"), OptionalContext);
					continue;
				}

				if (OutputAccessor->IsReadOnly())
				{
					PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), OutputTarget.GetDisplayText()), OptionalContext);
					continue;
				}

				// Final verification, if we can put the value of input into output
				if (!PCG::Private::IsBroadcastableOrConstructible(InputAccessor->GetUnderlyingType(), OutputAccessor->GetUnderlyingType()))
				{
					PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("CannotConvertTypes", "Cannot convert input type {0} into output type {1}"), PCG::Private::GetTypeNameText(InputAccessor->GetUnderlyingType()), PCG::Private::GetTypeNameText(OutputAccessor->GetUnderlyingType())), OptionalContext);
					continue;
				}

				// At this point, we are ready.
				PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams Params;
				Params.InKeys = InputKeys.Get();
				Params.InAccessor = InputAccessor.Get();
				Params.OutKeys = OutputKeys.Get();
				Params.OutAccessor = OutputAccessor.Get();
				Params.IterationCount = PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams::Out;
				Params.Flags = EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible;

				if (!PCGMetadataElementCommon::CopyFromAccessorToAccessor(Params))
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("ErrorGettingSettingValues", "Error while getting/setting values"), OptionalContext);
					continue;
				}
			}

			bSuccess = true;
		}

		return bSuccess;
	}

	bool CopyAttributes(const UPCGData* SourceData, const FPCGAttributePropertyInputSelector& _InputSource, UPCGData* TargetData, const FPCGAttributePropertyOutputSelector& _OutputTarget, bool bSameOrigin, FPCGContext* OptionalContext)
	{
		if (!TargetData || !SourceData)
		{
			return false;
		}

		TArray<TPair<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector>> AttributeSelectors;
		FPCGAttributePropertyInputSelector InputSource = _InputSource.CopyAndFixLast(SourceData);
		FPCGAttributePropertyOutputSelector OutputTarget = _OutputTarget.CopyAndFixSource(&InputSource, SourceData);

		AttributeSelectors.Emplace(MoveTemp(InputSource), MoveTemp(OutputTarget));
		return CopyAttributes(TargetData, SourceData, AttributeSelectors, bSameOrigin, OptionalContext);
	}

	bool CopyAllAttributes(const UPCGData* SourceData, UPCGData* TargetData, FPCGContext* OptionalContext)
	{
		if (!TargetData || !SourceData)
		{
			return false;
		}

		const UPCGMetadata* SourceMetadata = SourceData->ConstMetadata();
		if (!SourceMetadata)
		{
			return false;
		}

		TArray<TPair<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector>> AttributeSelectors;
		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		SourceMetadata->GetAttributes(AttributeNames, AttributeTypes);

		for (const FName& AttributeName : AttributeNames)
		{
			TPair<FPCGAttributePropertyInputSelector, FPCGAttributePropertyOutputSelector>& Selectors = AttributeSelectors.Emplace_GetRef();
			Selectors.Key.SetAttributeName(AttributeName);
			Selectors.Value.SetAttributeName(AttributeName);
		}

		return CopyAttributes(TargetData, SourceData, AttributeSelectors, /*bSameOrigin=*/false, OptionalContext);
	}
}

#undef LOCTEXT_NAMESPACE
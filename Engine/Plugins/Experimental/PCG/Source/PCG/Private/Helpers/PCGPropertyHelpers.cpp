// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGPropertyHelpers.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "UObject/Field.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PCGPropertyHelpers"

EPCGMetadataTypes PCGPropertyHelpers::GetMetadataTypeFromProperty(const FProperty* InProperty)
{
	if (!InProperty)
	{
		return EPCGMetadataTypes::Unknown;
	}

	// Object are not yet supported as accessors
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty))
	{
		return EPCGMetadataTypes::String;
	}

	TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(InProperty);

	return PropertyAccessor.IsValid() ? EPCGMetadataTypes(PropertyAccessor->GetUnderlyingType()) : EPCGMetadataTypes::Unknown;
}

UPCGParamData* PCGPropertyHelpers::ExtractPropertyAsAttributeSet(const PCGPropertyHelpers::FExtractorParameters& Parameters, FPCGContext* InOptionalContext)
{
	check(Parameters.Container && Parameters.Class);

	auto LogError = [InOptionalContext](const FText& ErrorMessage)
	{
		if (InOptionalContext)
		{
			PCGE_LOG_C(Error, GraphAndLog, InOptionalContext, ErrorMessage);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("%s"), *ErrorMessage.ToString());
		}
	};

	// Try to get the property
	FProperty* Property = FindFProperty<FProperty>(Parameters.Class, Parameters.PropertyName);
	if (!Property)
	{
		LogError(FText::Format(LOCTEXT("PropertyDoesNotExist", "Property '{0}' does not exist."), FText::FromName(Parameters.PropertyName)));
		return nullptr;
	}

	// Make sure the property is visible, if requested
	const uint64 ExcludePropertyFlags = CPF_DisableEditOnInstance;
	const uint64 IncludePropertyFlags = CPF_BlueprintVisible;
	if (Parameters.bPropertyNeedsToBeVisible)
	{
		if (Property->HasAnyPropertyFlags(ExcludePropertyFlags) || !Property->HasAnyPropertyFlags(IncludePropertyFlags))
		{
			LogError(FText::Format(LOCTEXT("PropertyExistsButNotVisible", "Property '{0}' does exist, but is not visible."), FText::FromName(Parameters.PropertyName)));
			return nullptr;
		}
	}

	// If the property is an array, we will work on the underlying property, and extract each element as an entry in the param data
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
	if (ArrayProperty)
	{
		Property = ArrayProperty->Inner;
	}

	using GetAddressFunc = TFunction<const void* (const void*)>;
	using ExtractablePropertyTuple = TTuple<FName, const FProperty*>;
	TArray<ExtractablePropertyTuple> ExtractableProperties;

	GetAddressFunc AddressFunc;

	// Force extraction if the property is not supported by accessors.
	const bool bShouldExtract = Parameters.bShouldExtract || !PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(Property);

	// Special case where the property is a struct/object, that is not supported by our metadata, we will try to break it down to multiple attributes in the resulting param data, if asked.
	if ((Property->IsA<FStructProperty>() || Property->IsA<FObjectProperty>()) && bShouldExtract)
	{
		UScriptStruct* UnderlyingStruct = nullptr;
		UClass* UnderlyingClass = nullptr;

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UnderlyingStruct = StructProperty->Struct;
			AddressFunc = [StructProperty](const void* InAddress) { return StructProperty->ContainerPtrToValuePtr<void>(InAddress); };
		}
		else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			UnderlyingClass = ObjectProperty->PropertyClass;
			AddressFunc = [ObjectProperty](const void* InAddress) { return ObjectProperty->GetObjectPropertyValue_InContainer(InAddress); };
		}

		check(UnderlyingStruct || UnderlyingClass);
		check(!!AddressFunc);

		// Re-use code from overridable params
		// Limit ourselves to not recurse into more structs.
		PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
		Config.bUseSeed = true;
		Config.bExcludeSuperProperties = true;
		Config.MaxStructDepth = 0;
		// Can only get exposed properties and visible if requested
		if (Parameters.bPropertyNeedsToBeVisible)
		{
			Config.ExcludePropertyFlags = ExcludePropertyFlags;
			Config.IncludePropertyFlags = IncludePropertyFlags;
		}
		TArray<FPCGSettingsOverridableParam> AllChildProperties = UnderlyingStruct ? PCGSettingsHelpers::GetAllOverridableParams(UnderlyingStruct, Config) : PCGSettingsHelpers::GetAllOverridableParams(UnderlyingClass, Config);

		for (const FPCGSettingsOverridableParam& Param : AllChildProperties)
		{
			if (ensure(!Param.PropertiesNames.IsEmpty()))
			{
				const FName ChildPropertyName = Param.PropertiesNames[0];
				if (const FProperty* ChildProperty = (UnderlyingStruct ? UnderlyingStruct->FindPropertyByName(ChildPropertyName) : UnderlyingClass->FindPropertyByName(ChildPropertyName)))
				{
					// We use authored name as attribute name to avoid issue with noisy property names, like in UUserDefinedStructs, where some random number is appended to the property name.
					// By default, it will just return the property name anyway.
					const FString AuthoredName = UnderlyingStruct ? UnderlyingStruct->GetAuthoredNameForField(ChildProperty) : UnderlyingClass->GetAuthoredNameForField(ChildProperty);
					ExtractableProperties.Emplace(FName(AuthoredName), ChildProperty);
				}
			}
		}
	}
	else
	{
		// For non struct/object, there is just a single property to extract with no shenanigans for address indirection.
		const FName AttributeName = (Parameters.OutputAttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName) ? Property->GetFName() : Parameters.OutputAttributeName;
		ExtractableProperties.Emplace(AttributeName, Property);
		// Identity
		AddressFunc = [](const void* InAddress) { return InAddress; };
	}

	if (ExtractableProperties.IsEmpty())
	{
		LogError(LOCTEXT("NoPropertiesFound", "No properties found to extract"));
		return nullptr;
	}

	// Before we need to compute all the addresses for each entry in our array (or just a single entry if there is no array)
	TArray<const void*, TInlineAllocator<16>> ElementAddresses;
	if (ArrayProperty)
	{
		FScriptArrayHelper_InContainer Helper(ArrayProperty, Parameters.Container);
		ElementAddresses.Reserve(Helper.Num());
		for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
		{
			ElementAddresses.Add(Helper.GetRawPtr(DynamicIndex));
		}
	}
	else
	{
		ElementAddresses.Add(Parameters.Container);
	}

	// From there, we should be able to create the data.
	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = ParamData->MutableMetadata();
	check(Metadata);

	bool bValidOperation = true;

	for (const void* ElementAddress : ElementAddresses)
	{
		if (!bValidOperation)
		{
			break;
		}

		// Add a new entry for all elements
		PCGMetadataEntryKey EntryKey = Metadata->AddEntry();

		for (ExtractablePropertyTuple& ExtractableProperty : ExtractableProperties)
		{
			const FName AttributeName = ExtractableProperty.Get<0>();
			const FProperty* FinalProperty = ExtractableProperty.Get<1>();

			// Offset the address if needed
			const void* ContainerPtr = AddressFunc(ElementAddress);

			if (!Metadata->SetAttributeFromDataProperty(AttributeName, EntryKey, ContainerPtr, FinalProperty, /*bCreate=*/ true))
			{
				LogError(FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating an attribute for property '{0}'. Either the property type is not supported by PCG or attribute creation failed."), FText::FromString(FinalProperty->GetName())));
				bValidOperation = false;
				break;
			}
		}
	}

	return bValidOperation ? ParamData : nullptr;
}

#undef LOCTEXT_NAMESPACE
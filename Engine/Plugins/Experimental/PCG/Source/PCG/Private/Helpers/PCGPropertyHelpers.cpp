// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGPropertyHelpers.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Engine/UserDefinedStruct.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PCGPropertyHelpers"

namespace PCGPropertyHelpers
{
	static constexpr uint64 ExcludePropertyFlags = CPF_DisableEditOnInstance;
	static constexpr uint64 IncludePropertyFlags = CPF_BlueprintVisible;

	void LogError(const FText& ErrorMessage, FPCGContext* InOptionalContext)
	{
		if (InOptionalContext)
		{
			PCGE_LOG_C(Error, GraphAndLog, InOptionalContext, ErrorMessage);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("%s"), *ErrorMessage.ToString());
		}
	}

	/**
	* Recursive function to go down the property chain to find the property and its container address.
	* @param CurrentClass      Struct/Class for the current container
	* @param CurrentName       Property name to look for in the container class.
	* @param NextNames         List of property names to continue extracting at a deeper level.
	* @param bNeedsToBeVisible Discard properties that are not visibile in Blueprint
	* @param OutContainer      Raw address for the current container. Will be write to at each recursive call.
	* @param OptionalContext   Optional context used for logging.
	* @returns                 The last property of the chain (and its container address is in OutContainer)
	*/
	const FProperty* ExtractPropertyChain(const UStruct* CurrentClass, const FName CurrentName, TArrayView<const FString> NextNames, const bool bNeedsToBeVisible, const void*& OutContainer, FPCGContext* OptionalContext)
	{
		check(CurrentClass);

		const FProperty* Property = nullptr;
		// Try to get the property. If it is coming from a user struct, we need to iterate on all properties because the property name is mangled
		if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentClass))
		{
			for (TFieldIterator<const FProperty> PropIt(UserDefinedStruct, EFieldIterationFlags::IncludeSuper); PropIt; ++PropIt)
			{
				const FName PropertyName = *UserDefinedStruct->GetAuthoredNameForField(*PropIt);
				if (PropertyName == CurrentName)
				{
					Property = *PropIt;
					break;
				}
			}
		}
		else
		{
			Property = FindFProperty<FProperty>(CurrentClass, CurrentName);
		}

		if (!Property)
		{
			LogError(FText::Format(LOCTEXT("PropertyDoesNotExist", "Property '{0}' does not exist in {1}."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
			return nullptr;
		}

		// Make sure the property is visible, if requested
		if (bNeedsToBeVisible && (Property->HasAnyPropertyFlags(ExcludePropertyFlags) || !Property->HasAnyPropertyFlags(IncludePropertyFlags)))
		{
			LogError(FText::Format(LOCTEXT("PropertyExistsButNotVisible", "Property '{0}' does exist in {1}, but is not visible."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
			return nullptr;
		}

		if (!NextNames.IsEmpty())
		{
			UStruct* NextClass = nullptr;

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				NextClass = StructProperty->Struct;
				OutContainer = StructProperty->ContainerPtrToValuePtr<void>(OutContainer);
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				NextClass = ObjectProperty->PropertyClass;
				OutContainer = ObjectProperty->GetObjectPropertyValue_InContainer(OutContainer);
			}
			else
			{
				LogError(FText::Format(LOCTEXT("PropertyIsNotExtractable", "Property '{0}' does exist in {1}, but is not extractable."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
				return nullptr;
			}

			return ExtractPropertyChain(NextClass, FName(NextNames[0]), NextNames.RightChop(1), bNeedsToBeVisible, OutContainer, OptionalContext);
		}
		else
		{
			return Property;
		}
	}
}

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

	const void* Container = Parameters.Container;
	const FProperty* Property = nullptr;
	const FName PropertyName = Parameters.PropertySelector.GetName();
	const bool ExtractRoot = (PropertyName == NAME_None);
	// If Name is none, extract the container as-is, using Parameters.Class, otherwise, extract the chain.
	if (!ExtractRoot)
	{
		Property = ExtractPropertyChain(Parameters.Class, PropertyName, Parameters.PropertySelector.GetExtraNames(), Parameters.bPropertyNeedsToBeVisible, Container, InOptionalContext);
		if (!Property)
		{
			return nullptr;
		}
	}

	// If the property is an array, we will work on the underlying property, and extract each element as an entry in the param data
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
	if (ArrayProperty)
	{
		Property = ArrayProperty->Inner;
	}

	using ExtractablePropertyTuple = TTuple<FName, const FProperty*>;
	TArray<ExtractablePropertyTuple> ExtractableProperties;

	using GetAddressFunc = TFunction<const void* (const void*)>;
	GetAddressFunc AddressFunc;

	// Force extraction if the property is not supported by accessors.
	const bool bShouldExtract = Parameters.bShouldExtract || !PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(Property);

	// Special case where the property is a struct/object, that is not supported by our metadata, we will try to break it down to multiple attributes in the resulting param data, if asked.
	if (ExtractRoot || ((Property->IsA<FStructProperty>() || Property->IsA<FObjectProperty>()) && bShouldExtract))
	{
		const UStruct* UnderlyingClass = nullptr;

		if (ExtractRoot)
		{
			UnderlyingClass = Parameters.Class;
			// Identity
			AddressFunc = [](const void* InAddress) { return InAddress; };
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UnderlyingClass = StructProperty->Struct;
			AddressFunc = [StructProperty](const void* InAddress) { return StructProperty->ContainerPtrToValuePtr<void>(InAddress); };
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			UnderlyingClass = ObjectProperty->PropertyClass;
			AddressFunc = [ObjectProperty](const void* InAddress) { return ObjectProperty->GetObjectPropertyValue_InContainer(InAddress); };
		}

		check(UnderlyingClass);
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
		TArray<FPCGSettingsOverridableParam> AllChildProperties = PCGSettingsHelpers::GetAllOverridableParams(UnderlyingClass, Config);

		for (const FPCGSettingsOverridableParam& Param : AllChildProperties)
		{
			if (ensure(!Param.PropertiesNames.IsEmpty()))
			{
				const FName ChildPropertyName = Param.PropertiesNames[0];
				if (const FProperty* ChildProperty = UnderlyingClass->FindPropertyByName(ChildPropertyName))
				{
					// We use authored name as attribute name to avoid issue with noisy property names, like in UUserDefinedStructs, where some random number is appended to the property name.
					// By default, it will just return the property name anyway.
					const FString AuthoredName = UnderlyingClass->GetAuthoredNameForField(ChildProperty);
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
		LogError(LOCTEXT("NoPropertiesFound", "No properties found to extract"), InOptionalContext);
		return nullptr;
	}

	// Before we need to compute all the addresses for each entry in our array (or just a single entry if there is no array)
	TArray<const void*, TInlineAllocator<16>> ElementAddresses;
	if (ArrayProperty)
	{
		FScriptArrayHelper_InContainer Helper(ArrayProperty, Container);
		ElementAddresses.Reserve(Helper.Num());
		for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
		{
			ElementAddresses.Add(Helper.GetRawPtr(DynamicIndex));
		}
	}
	else
	{
		ElementAddresses.Add(Container);
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
				LogError(FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating an attribute for property '{0}'. Either the property type is not supported by PCG or attribute creation failed."), FText::FromString(FinalProperty->GetName())), InOptionalContext);
				bValidOperation = false;
				break;
			}
		}
	}

	return bValidOperation ? ParamData : nullptr;
}

#undef LOCTEXT_NAMESPACE
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateAttribute.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCreateAttribute)

#define LOCTEXT_NAMESPACE "PCGCreateAttributeElement"

namespace PCGCreateAttributeConstants
{
	const FName NodeNameAddAttribute = TEXT("AddAttribute");
	const FText NodeTitleAddAttribute = LOCTEXT("NodeTitleAddAttribute", "Add Attribute");
	const FName NodeNameCreateAttribute = TEXT("CreateAttribute");
	const FText NodeTitleCreateAttribute = LOCTEXT("NodeTitleCreateAttribute", "Create Attribute");
	const FName AttributesLabel = TEXT("Attributes");
	const FText AttributesTooltip = LOCTEXT("AttributesTooltip", "Optional Attribute Set to create from. Not used if not connected.");
}

void UPCGCreateAttributeBaseSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if ((Type_DEPRECATED != EPCGMetadataTypes::Double) || (DoubleValue_DEPRECATED != 0.0))
	{
		AttributeTypes.Type = Type_DEPRECATED;
		AttributeTypes.DoubleValue = DoubleValue_DEPRECATED;
		AttributeTypes.FloatValue = FloatValue_DEPRECATED;
		AttributeTypes.IntValue = IntValue_DEPRECATED;
		AttributeTypes.Int32Value = Int32Value_DEPRECATED;
		AttributeTypes.Vector2Value = Vector2Value_DEPRECATED;
		AttributeTypes.VectorValue = VectorValue_DEPRECATED;
		AttributeTypes.Vector4Value = Vector4Value_DEPRECATED;
		AttributeTypes.RotatorValue = RotatorValue_DEPRECATED;
		AttributeTypes.QuatValue = QuatValue_DEPRECATED;
		AttributeTypes.TransformValue = TransformValue_DEPRECATED;
		AttributeTypes.BoolValue = BoolValue_DEPRECATED;
		AttributeTypes.StringValue = StringValue_DEPRECATED;
		AttributeTypes.NameValue = NameValue_DEPRECATED;

		Type_DEPRECATED = EPCGMetadataTypes::Double;
		DoubleValue_DEPRECATED = 0.0;
	}

	if (SourceParamAttributeName_DEPRECATED != NAME_None)
	{
		InputSource.SetAttributeName(SourceParamAttributeName_DEPRECATED);
		SourceParamAttributeName_DEPRECATED = NAME_None;
	}

	AttributeTypes.OnPostLoad();
#endif // WITH_EDITOR
}

EPCGDataType UPCGCreateAttributeBaseSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	if (!HasDynamicPins() || !InPin->IsOutputPin())
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	// Output pin narrows to union of inputs on first pin
	const EPCGDataType PrimaryInputType = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	return (PrimaryInputType != EPCGDataType::None) ? PrimaryInputType : EPCGDataType::Param; // No input (None) means param.
}

#if WITH_EDITOR
bool UPCGCreateAttributeBaseSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return !InPin || (InPin->Properties.Label != PCGCreateAttributeConstants::AttributesLabel) || InPin->IsConnected();
}

bool UPCGCreateAttributeBaseSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!InProperty || !Super::CanEditChange(InProperty))
	{
		return false;
	}

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const bool AttributesPinIsConnected = Node ? Node->IsInputPinConnected(PCGCreateAttributeConstants::AttributesLabel) : false;

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGCreateAttributeBaseSettings, InputSource))
	{
		return AttributesPinIsConnected;
	}
	else if (InProperty->GetOwnerStruct() == FPCGMetadataTypesConstantStruct::StaticStruct())
	{
		return !AttributesPinIsConnected;
	}

	return true;
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGCreateAttributeBaseSettings::CreateElement() const
{
	return MakeShared<FPCGCreateAttributeElement>();
}

FName UPCGCreateAttributeBaseSettings::AdditionalTaskNameInternal(FName NodeName) const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return NodeName;
	}

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const bool AttributesPinIsConnected = Node ? Node->IsInputPinConnected(PCGCreateAttributeConstants::AttributesLabel) : false;

	const FName OutputAttributeName = GetOutputAttributeName(AttributesPinIsConnected ? &InputSource : nullptr, nullptr);
	const FName SourceParamAttributeName = InputSource.GetName();

	if (ShouldAddAttributesPin() && AttributesPinIsConnected)
	{
		if ((OutputAttributeName == NAME_None) && (SourceParamAttributeName == NAME_None))
		{
			return NodeName;
		}
		else
		{
			const FString AttributeName = ((OutputAttributeName == NAME_None) ? SourceParamAttributeName : OutputAttributeName).ToString();
			return FName(FString::Printf(TEXT("%s %s"), *NodeName.ToString(), *AttributeName));
		}
	}
	else
	{
		return FName(FString::Printf(TEXT("%s: %s"), *OutputAttributeName.ToString(), *AttributeTypes.ToString()));
	}
}

UPCGAddAttributeSettings::UPCGAddAttributeSettings()
{
	OutputTarget.SetAttributeName(NAME_None);
}

FName UPCGAddAttributeSettings::AdditionalTaskName() const
{
	return AdditionalTaskNameInternal(PCGCreateAttributeConstants::NodeNameAddAttribute);
}

void UPCGAddAttributeSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (OutputAttributeName_DEPRECATED != NAME_None)
	{
		OutputTarget.SetAttributeName(OutputAttributeName_DEPRECATED);
		OutputAttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
FName UPCGAddAttributeSettings::GetDefaultNodeName() const
{
	return PCGCreateAttributeConstants::NodeNameAddAttribute;
}

FText UPCGAddAttributeSettings::GetDefaultNodeTitle() const
{
	return PCGCreateAttributeConstants::NodeTitleAddAttribute;
}

void UPCGAddAttributeSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	const bool AttributesPinIsConnected = InOutNode ? InOutNode->IsInputPinConnected(PCGCreateAttributeConstants::AttributesLabel) : false;

	if (DataVersion < FPCGCustomVersion::UpdateAddAttributeWithSelectors
		&& OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute
		&& OutputTarget.GetAttributeName() == NAME_None
		&& AttributesPinIsConnected)
	{
		// Previous behavior of the output target for this node was:
		// None => Source if Attributes pin is connected
		OutputTarget.SetAttributeName(PCGMetadataAttributeConstants::SourceAttributeName);
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGAddAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	PinProperties.Emplace(PCGCreateAttributeConstants::AttributesLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true, PCGCreateAttributeConstants::AttributesTooltip);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAddAttributeSettings::OutputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultOutputLabel;
	PinProperties.AllowedTypes = EPCGDataType::Any;

	return { PinProperties };
}

UPCGCreateAttributeSetSettings::UPCGCreateAttributeSetSettings()
{
	// No input pin to grab source param from
	bDisplayFromSourceParamSetting = false;
}

void UPCGCreateAttributeSetSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (OutputAttributeName_DEPRECATED != NAME_None)
	{
		OutputTarget.SetAttributeName(OutputAttributeName_DEPRECATED);
		OutputAttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
FName UPCGCreateAttributeSetSettings::GetDefaultNodeName() const
{
	return PCGCreateAttributeConstants::NodeNameCreateAttribute;
}

FText UPCGCreateAttributeSetSettings::GetDefaultNodeTitle() const
{
	return PCGCreateAttributeConstants::NodeTitleCreateAttribute;
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGCreateAttributeSetSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

TArray<FPCGPinProperties> UPCGCreateAttributeSetSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FName UPCGCreateAttributeSetSettings::AdditionalTaskName() const
{
	return AdditionalTaskNameInternal(PCGCreateAttributeConstants::NodeNameCreateAttribute);
}

bool FPCGCreateAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateAttributeElement::Execute);

	check(Context);

	const UPCGCreateAttributeBaseSettings* Settings = Context->GetInputSettings<UPCGCreateAttributeBaseSettings>();
	check(Settings);

	TArray<FPCGTaggedData> SourceParams = Context->InputData.GetInputsByPin(PCGCreateAttributeConstants::AttributesLabel);
	const UPCGParamData* SourceParamData = nullptr;
	FName SourceParamAttributeName = NAME_None;
	FName OutputAttributeName = NAME_None;

	FPCGAttributePropertyInputSelector InputSource{};

	if (!SourceParams.IsEmpty())
	{
		SourceParamData = CastChecked<UPCGParamData>(SourceParams[0].Data);

		if (!SourceParamData->Metadata)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ParamMissingMetadata", "Source Attribute Set data does not have metadata"));
			return true;
		}

		InputSource = Settings->InputSource.CopyAndFixLast(SourceParamData);

		SourceParamAttributeName = InputSource.GetName();
		OutputAttributeName = Settings->GetOutputAttributeName(&InputSource, SourceParamData);

		if (!SourceParamData->Metadata->HasAttribute(SourceParamAttributeName))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ParamMissingAttribute", "Source Attribute Set data does not have an attribute '{0}'"), FText::FromName(SourceParamAttributeName)));
			return true;
		}
	}
	else
	{
		OutputAttributeName = Settings->GetOutputAttributeName(nullptr, nullptr);
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Only if we have no input connections we'll switch from "adding attribute" to "creating param data"
	const UPCGPin* InputPin = Context->Node ? Context->Node->GetInputPin(PCGPinConstants::DefaultInputLabel) : nullptr;
	const bool bHasInputConnections = InputPin && InputPin->EdgeCount() > 0;

	// If the input is empty, we will create a new ParamData.
	// We can re-use this newly object as the output
	bool bCanReuseInputData = false;
	bool bIsParamData = false;
	if (!bHasInputConnections)
	{
		ensure(Inputs.IsEmpty());

		FPCGTaggedData& NewData = Inputs.Emplace_GetRef();
		NewData.Data = NewObject<UPCGParamData>();
		NewData.Pin = PCGPinConstants::DefaultInputLabel;
		bCanReuseInputData = true;
	}

	for (const FPCGTaggedData& InputTaggedData : Inputs)
	{
		const UPCGData* InputData = InputTaggedData.Data;
		UPCGData* OutputData = nullptr;

		UPCGMetadata* Metadata = nullptr;

		if (const UPCGSpatialData* InputSpatialData = Cast<UPCGSpatialData>(InputData))
		{
			UPCGSpatialData* NewSpatialData = InputSpatialData->DuplicateData(/*bInitializeFromData=*/false);
			NewSpatialData->InitializeFromData(InputSpatialData, /*InMetadataParentOverride=*/ nullptr, /*bInheritMetadata=*/true);

			OutputData = NewSpatialData;
			Metadata = NewSpatialData->Metadata;
		}
		else if (const UPCGParamData* InputParamData = Cast<UPCGParamData>(InputData))
		{
			// If we can reuse input data, it is safe to const_cast, as it was created by ourselves above.
			UPCGParamData* NewParamData = bCanReuseInputData ? const_cast<UPCGParamData*>(InputParamData) : NewObject<UPCGParamData>();
			NewParamData->Metadata->InitializeAsCopy(bCanReuseInputData ? nullptr : InputParamData->Metadata);

			OutputData = NewParamData;
			Metadata = NewParamData->Metadata;
			bIsParamData = true;
		}
		else
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid data as input. Only Spatial and Attribute Set data supported."));
			continue;
		}

		FPCGMetadataAttributeBase* Attribute = nullptr;

		if (SourceParamData)
		{
			// We need accessors if we have a multi entry source attribute or we have extractors
			const bool bIsMultiEntries = SourceParamData->Metadata->GetLocalItemCount() > 1;
			const bool bNeedAccessors = bIsMultiEntries || !InputSource.GetExtraNames().IsEmpty();

			// If no accessor, copy over the attribute
			if (!bNeedAccessors)
			{
				const FPCGMetadataAttributeBase* SourceAttribute = SourceParamData->Metadata->GetConstAttribute(SourceParamAttributeName);
				// Presence of attribute was already checked before, this should not return null
				check(SourceAttribute);

				// Copy the attribute using the first entry of the source attribute as the default value (there is just a single entry or none). If there is no first entry, will be the default value anyway.
				auto CreateAttribute = [Metadata, SourceAttribute, OutputAttributeName](auto Dummy) -> FPCGMetadataAttributeBase*
				{
					using AttributeType = decltype(Dummy);
					AttributeType DefaultValue = static_cast<const FPCGMetadataAttribute<AttributeType>*>(SourceAttribute)->GetValue(PCGMetadataEntryKey(0));
					return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(Metadata, OutputAttributeName, DefaultValue);
				};

				Attribute = PCGMetadataAttribute::CallbackWithRightType(SourceAttribute->GetTypeId(), std::move(CreateAttribute));
			}
			else // Create a new attribute of the accessed field's type manually
			{
				TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceParamData, InputSource);
				TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceParamData, InputSource);
				if (!InputAccessor.IsValid() || !InputKeys.IsValid())
				{
					PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor"));
					return true;
				}

				auto CreateOutputAttribute = [Metadata, OutputAttributeName, &InputAccessor, &InputKeys, OutputData, bIsParamData, bIsMultiEntries]<typename Type>(Type Dummy) -> FPCGMetadataAttributeBase*
				{
					// If we have multiple entries, use the zero value as a default value.
					// Otherwise get the first entry of the source param as default value.
					Type DefaultValue = PCG::Private::MetadataTraits<Type>::ZeroValue();
					if (!bIsMultiEntries)
					{
						InputAccessor->Get<Type>(DefaultValue, FPCGAttributeAccessorKeysEntries(PCGMetadataEntryKey(0)));
					}

					FPCGMetadataAttribute<Type>* Attribute = PCGMetadataElementCommon::ClearOrCreateAttribute<Type>(Metadata, OutputAttributeName, DefaultValue);

					FPCGAttributePropertySelector OutputSelector;
					OutputSelector.SetAttributeName(OutputAttributeName);

					TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(OutputData, OutputSelector);
					TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(OutputData, OutputSelector);

					// We just created the attribute, this should not fail
					if (!ensure(OutputAccessor.IsValid() && OutputKeys.IsValid()))
					{
						return nullptr;
					}

					PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams Params;
					Params.InKeys = InputKeys.Get();
					Params.InAccessor = InputAccessor.Get();
					Params.OutKeys = OutputKeys.Get();
					Params.OutAccessor = OutputAccessor.Get();
					Params.IterationCount = PCGMetadataElementCommon::FCopyFromAccessorToAccessorParams::Min;

					PCGMetadataElementCommon::CopyFromAccessorToAccessor(Params);

					// In case of param data, do some padding at the end if needed
					if (bIsParamData)
					{
						int32 NumInKeys = InputKeys->GetNum();
						int32 Padding = OutputKeys->GetNum() - NumInKeys;
						for (int64 Key = 0; Key < Padding; ++Key)
						{
							Attribute->SetValueFromValueKey(PCGMetadataEntryKey(NumInKeys + Key), PCGDefaultValueKey);
						}
					}

					return Attribute;
				};

				Attribute = PCGMetadataAttribute::CallbackWithRightType(InputAccessor->GetUnderlyingType(), CreateOutputAttribute);
			}
		}
		else
		{
			Attribute = ClearOrCreateAttribute(Settings, Metadata, OutputAttributeName);
		}

		if (!Attribute)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating attribute '{0}'"), FText::FromName(OutputAttributeName)));
			continue;
		}

		if (bIsParamData)
		{
			// Making sure the metadata has at least one entry.
			if (Metadata->GetLocalItemCount() == 0)
			{
				Metadata->AddEntry();
			}

			// Also make sure if the attribute has no entries, to map all the metadata entry to the default value key
			// Very important to do in the case of multi-entry attribute set.
			if (Attribute->GetNumberOfEntries() == 0)
			{
				for (PCGMetadataEntryKey Key = 0; Key < Metadata->GetLocalItemCount(); ++Key)
				{
					Attribute->SetValueFromValueKey(Key, PCGDefaultValueKey);
				}
			}
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputData;
	}

	return true;
}

FPCGMetadataAttributeBase* FPCGCreateAttributeElement::ClearOrCreateAttribute(const UPCGCreateAttributeBaseSettings* Settings, UPCGMetadata* Metadata, const FName OutputAttributeName) const
{
	check(Metadata);

	auto CreateAttribute = [Settings, Metadata, OutputAttributeName](auto&& Value) -> FPCGMetadataAttributeBase*
	{
		return PCGMetadataElementCommon::ClearOrCreateAttribute(Metadata, OutputAttributeName, std::forward<decltype(Value)>(Value));
	};

	return Settings->AttributeTypes.Dispatcher(CreateAttribute);
}

#undef LOCTEXT_NAMESPACE

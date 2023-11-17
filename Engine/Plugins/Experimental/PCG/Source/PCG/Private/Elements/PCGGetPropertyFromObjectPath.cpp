// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGGetPropertyFromObjectPath.h"

#include "PCGParamData.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "PCGGetPropertyFromObjectPathElement"

#if WITH_EDITOR
FName UPCGGetPropertyFromObjectPathSettings::GetDefaultNodeName() const
{
	return FName(TEXT("GetPropertyFromObjectPath"));
}

FText UPCGGetPropertyFromObjectPathSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Property From Object Path");
}

bool UPCGGetPropertyFromObjectPathSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return !InPin || (InPin->Properties.Label != PCGPinConstants::DefaultInputLabel) || InPin->IsConnected();
}

bool UPCGGetPropertyFromObjectPathSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!InProperty || !Super::CanEditChange(InProperty))
	{
		return false;
	}

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const bool InPinIsConnected = Node ? Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGetPropertyFromObjectPathSettings, InputSource))
	{
		return InPinIsConnected;
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGetPropertyFromObjectPathSettings, ObjectPathsToExtract))
	{
		return !InPinIsConnected;
	}

	return true;
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGGetPropertyFromObjectPathSettings::CreateElement() const
{
	return MakeShared<FPCGGetPropertyFromObjectPathElement>();
}

TArray<FPCGPinProperties> UPCGGetPropertyFromObjectPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Param);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGetPropertyFromObjectPathSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGContext* FPCGGetPropertyFromObjectPathElement::CreateContext()
{
	return new FPCGGetPropertyFromObjectPathContext();
}

bool FPCGGetPropertyFromObjectPathElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGetPropertyFromObjectPathElement::PrepareData);

	check(Context);

	const UPCGGetPropertyFromObjectPathSettings* Settings = Context->GetInputSettings<UPCGGetPropertyFromObjectPathSettings>();
	check(Settings);

	FPCGGetPropertyFromObjectPathContext* ThisContext = static_cast<FPCGGetPropertyFromObjectPathContext*>(Context);

	bool bIsDone = true;

	if (!ThisContext->bRequestSent)
	{
		const UPCGNode* Node = Context->Node;
		const bool InPinIsConnected = Node ? Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;

		// First gather all the soft objects paths to extract
		TArray<FSoftObjectPath> ObjectsToLoad;
		if (!InPinIsConnected)
		{
			ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Reserve(Settings->ObjectPathsToExtract.Num());
			for (const FSoftObjectPath& Path : Settings->ObjectPathsToExtract)
			{
				if (!Path.IsNull())
				{
					ObjectsToLoad.AddUnique(Path);
				}

				ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(Path, -1);
			}
		}
		else
		{
			const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
			for (int32 Index = 0; Index < Inputs.Num(); ++Index)
			{
				const FPCGTaggedData& Input = Inputs[Index];

				if (!Input.Data)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidData", "Invalid data for input {0}"), FText::AsNumber(Index)));
					continue;
				}

				const FPCGAttributePropertyInputSelector AttributeSelector = Settings->InputSource.CopyAndFixLast(Input.Data);
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Input.Data, AttributeSelector);
				const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Input.Data, AttributeSelector);

				if (!Accessor.IsValid() || !Keys.IsValid())
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Attribute/Property '{0}' does not exist on input {1}"), AttributeSelector.GetDisplayText(), FText::AsNumber(Index)));
					continue;
				}

				const int32 NumElementsToAdd = Keys->GetNum();
				if (NumElementsToAdd == 0)
				{
					continue;
				}

				TArray<FSoftObjectPath> InputValues;
				InputValues.SetNum(NumElementsToAdd);
				if (Accessor->GetRange(MakeArrayView(InputValues), 0, *Keys, EPCGAttributeAccessorFlags::AllowConstructible))
				{
					ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Reserve(ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Num() + NumElementsToAdd);
					for (int32 i = 0; i < InputValues.Num(); ++i)
					{
						FSoftObjectPath& Path = InputValues[i];
						if (!Path.IsNull())
						{
							ObjectsToLoad.AddUnique(Path);
							ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(std::move(Path), Index);
						}
						else
						{
							PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidPath", "Value number {0} for Attribute/Property '{1}' on input {2} is not a valid path or is null. Will be ignored."), FText::AsNumber(i), AttributeSelector.GetDisplayText(), FText::AsNumber(Index)));
							continue;
						}
					}
				}
				else
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidAttribute", "Attribute/Property '{0}'({1}) is not convertible to a SoftObjectPath on input {2}"), AttributeSelector.GetDisplayText(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType()), FText::AsNumber(Index)));
					continue;
				}
			}
		}

		if (!ObjectsToLoad.IsEmpty())
		{
			if (Settings->bSynchronousLoad)
			{
				ThisContext->LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(std::move(ObjectsToLoad));
			}
			else
			{
				bIsDone = false;
				ThisContext->bIsPaused = true;
				ThisContext->LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(std::move(ObjectsToLoad), [Context]() { Context->bIsPaused = false; });
			}
		}

		ThisContext->bRequestSent = true;
	}

	return bIsDone;
}

bool FPCGGetPropertyFromObjectPathElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGetPropertyFromObjectPathElement::Execute);

	check(Context);
	FPCGGetPropertyFromObjectPathContext* ThisContext = static_cast<FPCGGetPropertyFromObjectPathContext*>(Context);

	const UPCGGetPropertyFromObjectPathSettings* Settings = Context->GetInputSettings<UPCGGetPropertyFromObjectPathSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (const TTuple<FSoftObjectPath, int32>& SoftPathAndIndex : ThisContext->PathsToObjectsToExtractAndIncomingDataIndex)
	{
		const FSoftObjectPath& SoftPath = SoftPathAndIndex.Get<FSoftObjectPath>();
		const int32 Index = SoftPathAndIndex.Get<int32>();

		const UObject* Object = SoftPath.ResolveObject();
		if (!Object)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToLoad", "Failed to load object {0}"), FText::FromString(SoftPath.ToString())));
			continue;
		}

		const FPCGAttributePropertySelector Selector = FPCGAttributePropertySelector::CreateSelectorFromString(Settings->PropertyName.ToString());

		PCGPropertyHelpers::FExtractorParameters Parameters{ Object, Object->GetClass(), Selector, Settings->OutputAttributeName, Settings->bForceObjectAndStructExtraction, /*bPropertyNeedsToBeVisible=*/true };
		if (UPCGParamData* ParamData = PCGPropertyHelpers::ExtractPropertyAsAttributeSet(Parameters, Context))
		{
			TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
			FPCGTaggedData& Output = Outputs.Emplace_GetRef();
			Output.Data = ParamData;
			Output.Pin = PCGPinConstants::DefaultOutputLabel;
			if (Index >= 0)
			{
				Output.Tags = Inputs[Index].Tags;
			}
		}
		else
		{
			if (Selector.GetName() == NAME_None)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToExtractObject", "Fail to extract object {0}."), FText::FromString(Object->GetName())));
			}
			else
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToExtract", "Fail to extract the property '{0}' on object {1}."), Selector.GetDisplayText(), FText::FromString(Object->GetName())));
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

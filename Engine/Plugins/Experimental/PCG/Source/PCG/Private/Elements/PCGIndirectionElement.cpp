// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGIndirectionElement.h"

#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGIndirectionElement)

#define LOCTEXT_NAMESPACE "PCGIndirectionElement"

#if WITH_EDITOR

FName UPCGIndirectionSettings::GetDefaultNodeName() const
{
	return FName(TEXT("Proxy"));
}

FText UPCGIndirectionSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Proxy");
}

FText UPCGIndirectionSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Executes another settings object, which can be overriden.");
}

#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGIndirectionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGIndirectionSettings::CreateElement() const
{
	return MakeShared<FPCGIndirectionElement>();
}

bool FPCGIndirectionElement::CanExecuteOnlyOnMainThread(FPCGContext* InContext) const
{
	if (!InContext)
	{
		return true;
	}

	FPCGIndirectionContext* Context = static_cast<FPCGIndirectionContext*>(InContext);

	if (Context->InnerElement)
	{
		return Context->InnerElement->CanExecuteOnlyOnMainThread(Context->InnerContext);
	}
	else
	{
		return true;
	}
}

FPCGIndirectionContext::~FPCGIndirectionContext()
{
	if (bNeedsToUnrootInnerSettings)
	{
		const UPCGIndirectionSettings* Settings = GetInputSettings<UPCGIndirectionSettings>();
		check(Settings);

		if (UPCGSettings* InnerSettings = Settings->Settings.Get())
		{
			InnerSettings->RemoveFromRoot();
		}
	}

	delete InnerContext;
	InnerContext = nullptr;
}

FPCGContext* FPCGIndirectionElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGIndirectionContext* Context = new FPCGIndirectionContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGIndirectionElement::PrepareDataInternal(FPCGContext* InContext) const
{
	check(InContext);
	FPCGIndirectionContext* Context = static_cast<FPCGIndirectionContext*>(InContext);

	const UPCGIndirectionSettings* Settings = Context->GetInputSettings<UPCGIndirectionSettings>();
	check(Settings);

	if (UPCGSettings* InnerSettings = Settings->Settings.LoadSynchronous())
	{
		// TODO: while we can root it here, if this node or multiple indirection node were to execute in parallel,
		// the lifetime of the inner settings would be a bit unclear - we need to improve this.
		if (!InnerSettings->IsRooted())
		{
			InnerSettings->AddToRoot();
			Context->bNeedsToUnrootInnerSettings = true;
		}

		Context->InnerElement = InnerSettings->GetElement();
		check(Context->InnerElement);

		// Note: we need to pass a null node here + add the inner settings as part of the input so that they are retrieved properly
		FPCGDataCollection InnerInput = Context->InputData;
		InnerInput.TaggedData.Emplace_GetRef().Data = InnerSettings;

		// TODO: there are some types of settings that might require a bit more information to be able to do their processing correctly
		// namely (dynamic) subgraphs, so YMMV.
		Context->InnerContext = Context->InnerElement->Initialize(InnerInput, Context->SourceComponent, nullptr);
		check(Context->InnerContext);
		Context->InnerContext->InitializeSettings();

		// Unclear whether we should give those new values
		Context->InnerContext->TaskId = Context->TaskId;
		Context->InnerContext->CompiledTaskId = Context->CompiledTaskId;
		Context->InnerContext->DependenciesCrc = Context->DependenciesCrc;
		Context->InnerContext->GenerationGrid = Context->GenerationGrid;

		Context->InnerContext->AsyncState = Context->AsyncState;
	}

	return true;
}

bool FPCGIndirectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGIndirectionElement::Execute);

	check(InContext);
	FPCGIndirectionContext* Context = static_cast<FPCGIndirectionContext*>(InContext);

	const UPCGIndirectionSettings* Settings = Context->GetInputSettings<UPCGIndirectionSettings>();
	check(Settings);

	if (!Context->InnerElement || !Context->InnerContext)
	{
		return true;
	}

	// TODO: use caching when possible
	// TODO: see what we can do for inspection data
	// TODO: support pausing in inner element, might require some upstream changes in the graph executor
	Context->InnerContext->AsyncState = Context->AsyncState;
	bool bElementDone = Context->InnerElement->Execute(Context->InnerContext);

	// Implementation note: to make sure everything is clean vs. the root set, we need to copy the output data
	// regardless of whether the element is done or not
	Context->OutputData = Context->InnerContext->OutputData;

	// Finally, move the inner context pin data (which does not exist as-is on this node) to tags if required
	if (bElementDone && Settings->bTagOutputsBasedOnOutputPins)
	{
		for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
		{
			if (TaggedData.Pin != NAME_None && TaggedData.Pin != PCGPinConstants::DefaultOutputLabel)
			{
				TaggedData.Tags.Add(TaggedData.Pin.ToString());
				TaggedData.Pin = NAME_None;
			}
		}
	}

	return bElementDone;
}

#undef LOCTEXT_NAMESPACE
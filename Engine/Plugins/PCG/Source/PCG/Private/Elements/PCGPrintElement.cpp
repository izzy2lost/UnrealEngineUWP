// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPrintElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGModule.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "EngineGlobals.h"
#include "Editor/EditorEngine.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "PCGPrintElement"

namespace PCGPrintElementConstants
{
	FText Delimiter = LOCTEXT("Delimiter", "::");
}

TArray<FPCGPinProperties> UPCGPrintElementSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGPrintElementSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
	return PinProperties;
}

FPCGElementPtr UPCGPrintElementSettings::CreateElement() const
{
	return MakeShared<FPCGPrintElement>();
}

bool FPCGPrintElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPrintElement::Execute);

	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	const UPCGPrintElementSettings* Settings = Context->GetInputSettings<UPCGPrintElementSettings>();
	check(Settings);

	if (!Settings->bEnablePrint)
	{
		return true;
	}

	TArray<FString> Prefixes;

	// Build the message prefixes
	const UPCGComponent* Component = Context->SourceComponent->GetOriginalComponent();
	if (Component)
	{
		if (Settings->bPrefixWithOwner)
		{
			Prefixes.Emplace(Component->GetOwner() ? Component->GetOwner()->GetName() : LOCTEXT("MissingOwner", "Missing Owner").ToString());
		}

		if (Settings->bPrefixWithComponent)
		{
			Prefixes.Emplace(Component->GetName());
		}

		if (Settings->bPrefixWithGraph)
		{
			Prefixes.Emplace(Component->GetGraph() ? Component->GetGraph()->GetName() : LOCTEXT("MissingGraph", "Missing Graph").ToString());
		}
	}

	if (Settings->bPrefixWithNode)
	{
		Prefixes.Emplace(Context->Node ? Context->Node->GetName() : LOCTEXT("MissingNode", "Missing Node").ToString());
	}

	FString ObjectPrefix;
	if (!Prefixes.IsEmpty())
	{
		ObjectPrefix = FString::Printf(TEXT("[%s]: "), *FString::Join(Prefixes, *PCGPrintElementConstants::Delimiter.ToString()));
	}

	const FString FinalString = Settings->CustomPrefix + ObjectPrefix + Settings->PrintString;

	switch (Settings->Verbosity)
	{
		case EPCGPrintVerbosity::Error:
			PCGLog::LogErrorOnGraph(FText::FromString(FinalString), Settings->bDisplayOnNode ? Context : nullptr);
			break;
		case EPCGPrintVerbosity::Warning:
			PCGLog::LogWarningOnGraph(FText::FromString(FinalString), Settings->bDisplayOnNode ? Context : nullptr);
			break;
		case EPCGPrintVerbosity::Log:
			PCGE_LOG_C(Log, LogOnly, Context, FText::FromString(FinalString));
			break;
		default:
			checkNoEntry();
	}

#if WITH_EDITOR
	if (Settings->bPrintToScreen && GEngine)
	{
		if (UEditorEngine* Editor = static_cast<UEditorEngine*>(GEngine))
		{
			uint32 HashKey = HashCombine(GetTypeHash(Context->Node->GetFName()), Context->Node->GetUniqueID());

			if (Settings->bPrintPerComponent)
			{
				HashKey = HashCombine(HashKey, Component->GetUniqueID());
			}

			Editor->AddOnScreenDebugMessage(static_cast<uint64>(HashKey), Settings->PrintToScreenDuration, Settings->PrintToScreenColor, FinalString);
		}
	}
#endif // WITH_EDITOR
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING

	return true;
}

#undef LOCTEXT_NAMESPACE

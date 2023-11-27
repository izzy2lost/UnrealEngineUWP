// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGBranch.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Elements/PCGGather.h"

#define LOCTEXT_NAMESPACE "FPCGBranchElement"

namespace PCGBranchConstants
{
	const FName OutputLabelA = TEXT("Output A");
	const FName OutputLabelB = TEXT("Output B");
	const FText NodeTitleBase = LOCTEXT("NodeTitle", "Branch");
}

#if WITH_EDITOR
FText UPCGBranchSettings::GetDefaultNodeTitle() const
{
	// TODO: This should statically update or dynamically update, if overridden, for which branch was taken, ie. Branch (Output A)
	return PCGBranchConstants::NodeTitleBase;
}

FText UPCGBranchSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Control flow node that will route the input to either Output A or Output B, based on the 'Output To B' property - which can also be overridden.");
}

EPCGChangeType UPCGBranchSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;

	// Grid sizes are processed during graph compilation and is part of the graph structure.
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGBranchSettings, bEnabled)
		|| InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGBranchSettings, bOutputToB))
	{
		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGBranchSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGBranchConstants::OutputLabelA,
		EPCGDataType::Any,
		/*bInAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("OutputPinTooltipA", "Will only route input if 'Output To B' (overridable) is false"));
	PinProperties.Emplace(PCGBranchConstants::OutputLabelB,
		EPCGDataType::Any,
		/*bInAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("OutputPinTooltipB", "Will only route input if 'Output To B' (overridable) is true"));

	return PinProperties;
}

FPCGElementPtr UPCGBranchSettings::CreateElement() const
{
	return MakeShared<FPCGBranchElement>();
}

bool UPCGBranchSettings::IsDynamicBranch() const
{
	UPCGNode* Node = Cast<UPCGNode>(GetOuter());

	if (!Node && OriginalSettings)
	{
		Node = Cast<UPCGNode>(OriginalSettings->GetOuter());
	}

	if (!Node)
	{
		return false;
	}

	const FName PropertyName = GET_MEMBER_NAME_CHECKED(UPCGBranchSettings, bOutputToB);

	const FPCGSettingsOverridableParam* Param = CachedOverridableParams.FindByPredicate([PropertyName](const FPCGSettingsOverridableParam& ParamToCheck)
	{
		return !ParamToCheck.PropertiesNames.IsEmpty() && ParamToCheck.PropertiesNames.Last() == PropertyName;
	});

	if (Param)
	{
		if (const UPCGPin* Pin = Node->GetInputPin(Param->Label))
		{
			return Pin->IsConnected();
		}
	}

	return false;
}

bool UPCGBranchSettings::IsPinStaticallyActive(const FName& PinLabel) const
{
	// Dynamic branches are never known in advance - assume all branches are active prior to execution.
	if (IsDynamicBranch())
	{
		return true;
	}

	// Branch must be both enabled and set to B in order for output pin B to be active.
	const FName ActiveOutputPinLabel = (bEnabled && bOutputToB) ? PCGBranchConstants::OutputLabelB : PCGBranchConstants::OutputLabelA;

	return PinLabel == ActiveOutputPinLabel;
}

bool FPCGBranchElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGBranchElement::ExecuteInternal);

	const UPCGBranchSettings* Settings = Context->GetInputSettings<UPCGBranchSettings>();
	check(Settings);

	const FName SelectedPinLabel = Settings->bOutputToB ? PCGBranchConstants::OutputLabelB : PCGBranchConstants::OutputLabelA;

	// Reuse the functionality of the Gather Node
	Context->OutputData = PCGGather::GatherDataForPin(Context->InputData, PCGPinConstants::DefaultInputLabel, SelectedPinLabel);

	return true;
}

#undef LOCTEXT_NAMESPACE

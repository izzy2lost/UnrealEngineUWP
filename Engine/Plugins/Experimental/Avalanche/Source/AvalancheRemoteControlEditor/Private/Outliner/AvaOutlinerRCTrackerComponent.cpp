// Copyright Epic Games, Inc. All Rights Reserved.

#include "Outliner/AvaOutlinerRCTrackerComponent.h"
#include "AvaOutliner.h"
#include "RemoteControlTrackerComponent.h"
#include "Selection/AvaOutlinerScopedSelection.h"
#include "Subsystems/RemoteControlComponentsEditorUtils.h"

#define LOCTEXT_NAMESPACE "AvaOutlinerRemoteControlComponent"

FAvaOutlinerRCTrackerComponent::FOnAvaOutlinerRCTrackerComponentSelected FAvaOutlinerRCTrackerComponent::OnRCTrackerSelectedDelegate;

FAvaOutlinerRCTrackerComponent::FAvaOutlinerRCTrackerComponent(FAvaOutliner& InOutliner, URemoteControlTrackerComponent* InComponent)
	: FAvaOutlinerObject(InOutliner, InComponent)
	, TrackerComponentWeak(InComponent)
{
	TrackerIcon = FRemoteControlComponentsEditorUtils::GetIcon("ClassIcon.RemoteControlTracker");
}

FSlateIcon FAvaOutlinerRCTrackerComponent::GetIcon() const
{
	return TrackerIcon;
}

bool FAvaOutlinerRCTrackerComponent::ShowVisibility(EAvaOutlinerVisibilityType InVisibilityType) const
{
	return true;
}

bool FAvaOutlinerRCTrackerComponent::GetVisibility(EAvaOutlinerVisibilityType InVisibilityType) const
{
	return TrackerComponentWeak.IsValid();
}

void FAvaOutlinerRCTrackerComponent::OnVisibilityChanged(EAvaOutlinerVisibilityType InVisibilityType, bool bInNewVisibility)
{
	FAvaOutlinerObject::OnVisibilityChanged(InVisibilityType, bInNewVisibility);
}

void FAvaOutlinerRCTrackerComponent::Select(FAvaOutlinerScopedSelection& InSelection) const
{
	FAvaOutlinerObject::Select(InSelection);

	if (const URemoteControlTrackerComponent* TrackerComponent = TrackerComponentWeak.Get())
	{
		if (InSelection.IsSelected(TrackerComponent))
		{
			OnRemoteControlTrackerSelected().Broadcast(TrackerComponent);
		}
	}
}

void FAvaOutlinerRCTrackerComponent::SetObject_Impl(UObject* InObject)
{
	FAvaOutlinerObject::SetObject_Impl(InObject);
	TrackerComponentWeak = Cast<URemoteControlTrackerComponent>(InObject);
}

#undef LOCTEXT_NAMESPACE

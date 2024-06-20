// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceChannel.h"

#include "Internationalization/Text.h"

// TraceTools
#include "Services/ISessionTraceFilterService.h"

namespace UE::TraceTools
{

FTraceChannel::FTraceChannel(FString InName,
							 FString InDescription,
							 FString InParentName, 
							 uint32 InId,
							 bool bInEnabled,
							 bool bInReadOnly,
							 TSharedPtr<ISessionTraceFilterService> InFilterService) 
	: Name(InName),
	  Description(InDescription),
	  ParentName(InParentName),
	  Id(InId),
	  bFiltered(!bInEnabled), 
	  bIsPending(false),
	  bReadOnly(bInReadOnly),
	  FilterService(InFilterService)
{
}

FText FTraceChannel::GetDisplayText() const
{
	return FText::FromString(Name);
}

FText FTraceChannel::GetTooltipText() const
{
	return FText::FromString(Description);
}

FString FTraceChannel::GetName() const
{
	return Name;
}

FString FTraceChannel::GetDescription() const
{
	return Description;
}

void FTraceChannel::SetPending()
{
	bIsPending = true;
}

bool FTraceChannel::IsReadOnly() const
{
	return bReadOnly;
}

void FTraceChannel::SetIsFiltered(bool bState)
{
	SetPending();
	FilterService->SetObjectFilterState(Name, !bState);
}

bool FTraceChannel::IsFiltered() const
{
	return bFiltered;
}

bool FTraceChannel::IsPending() const
{
	return bIsPending;
}

void FTraceChannel::GetSearchString(TArray<FString>& OutFilterStrings) const
{
	OutFilterStrings.Add(Name);
}

} // namespace UE::TraceTools
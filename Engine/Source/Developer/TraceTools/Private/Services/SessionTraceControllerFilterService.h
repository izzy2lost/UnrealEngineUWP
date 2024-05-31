// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISessionTraceFilterService.h"
#include "ITraceController.h"
#include "Misc/DateTime.h"

namespace TraceServices
{
	class IAnalysisSession;
	typedef uint64 FSessionHandle;
}

namespace UE::TraceTools
{

class IEventInfoProvider;

/** Implementation of ISessionTraceFilterService to query and set channels using the TraceController (MessageBus). */
class FSessionTraceControllerFilterService : public ISessionTraceFilterService
{
public:
	FSessionTraceControllerFilterService(TSharedPtr<ITraceController> InTraceController);
	virtual ~FSessionTraceControllerFilterService();

	/** Begin ISessionTraceFilterService overrides */
	virtual void GetRootObjects(TArray<FTraceObjectInfo>& OutObjects) const override;
	virtual void GetChildObjects(uint32 InObjectHash, TArray<FTraceObjectInfo>& OutChildObjects) const override;
	virtual const FDateTime& GetTimestamp() override;
	virtual void SetObjectFilterState(const FString& InObjectName, const bool bFilterState) override;
	virtual void UpdateFilterPreset(const TSharedPtr<ITraceFilterPreset> InPreset, bool IsEnabled) override;
	/** End ISessionTraceFilterService overrides */

protected:
	/** Callback at end of engine frame, used to dispatch all enabled/disabled channels */
	void OnApplyChannelChanges();

	/** Retrieves channels names from provider and marks them all as disabled */
	void DisableAllChannels();

	void RetrieveAndStoreStartupChannels();

	void OnTraceStatusUpdated(const FTraceStatus& InStatus, FTraceStatus::EUpdateType InUpdateType, ITraceControllerCommands& Commands);

protected:
	TSharedPtr<ITraceController> TraceController;

	TArray<FTraceObjectInfo> Objects;

	/** Names of channels that were either enabled or disabled during the duration of this frame */
	TArray<FString> FrameEnabledChannels;
	TArray<FString> FrameDisabledChannels;

	/** Timestamp at which contained data (including provider) was last updated */
	FDateTime TimeStamp;	
};

} // namespace UE::TraceTools
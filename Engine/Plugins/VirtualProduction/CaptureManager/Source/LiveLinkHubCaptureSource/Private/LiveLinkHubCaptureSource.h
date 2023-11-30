// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSource.h"

#include "Capabilities/MessagingCapability.h"

#include "MessageEndpoint.h"

#include "LiveLinkHubCaptureMessages.h"

#include "Widgets/SWidget.h"

class FLiveLinkHubCaptureSource 
	: public FCaptureSource
{
public:

	FLiveLinkHubCaptureSource(const FString& InName);

	virtual FCaptureVoidResult Start() override;
	virtual FCaptureVoidResult Stop() override;

private:

	void HandleGetCapabilities(const FName& InResponseName, const FBaseResponse& InResponse);

	TUniquePtr<FCaptureSourceCapability> CreateCapability(const FString& InName);

	template<typename T, typename ... Args>
	TUniquePtr<T> CreateCapability(Args&&... InArgs)
	{
		return MakeUnique<T>(Forward<Args>(InArgs)...);
	}

	FPropertyValue PropertyGetter(const FString& InProperty, FString InCapability);
	void CommandHandler(TSharedPtr<FCommandBase> InCommand, FString InCapability);
	void HandleCommandResponse(const FName& InName, const FBaseResponse& InResponse, FString InCapability);

	void GetPropertyHandler(const FName& InName, const FBaseResponse& InResponse, FString InProperty);
	void EventHandler(TSharedPtr<const FCaptureEvent> InEvent);
	void OnDisconnect();

	TSharedPtr<SWidget> CreateMessagingWidget(int32, FCaptureSourceCapability*);

	TMap<FString, TUniquePtr<FCaptureSourceCapability>> OwnedCapabilities;
	FMessagingCapability* MessagingCapability;

	FCriticalSection Mutex;
	TMap<FString, FPropertyValue> PropertyValues;
};
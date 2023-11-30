// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubCaptureSource.h"

#include "LiveLinkHubCaptureSourceFactory.h"
#include "LiveLinkHubCaptureSourceLog.h"

#include "MessageEndpointBuilder.h"

#include "CaptureSourceFrameworkModule.h"
#include "CaptureSourceDetailsCustomization.h"

#include "Capabilities/DiskCapacityCapability.h"
#include "Capabilities/RecordingCapability.h"

#include "Widgets/SNullWidget.h"

DEFINE_LOG_CATEGORY(LogLiveLinkHubCaptureSource);

FLiveLinkHubCaptureSource::FLiveLinkHubCaptureSource(const FString& InName)
	: FCaptureSource(FLiveLinkHubCaptureSourceFactory::Id, InName)
{
	using FCustomization = FCaptureSourceDetailsCustomization;
	using FCreator = FCustomization::FCapabilityWidgetCreator;

	FCaptureSourceDetailsCustomization& Customization =
		FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework").GetCustomization();

	Customization.
		RegisterCapabilityCustomization(FLiveLinkHubCaptureSourceFactory::Id, 
										FMessagingCapability::Name, FCreator::CreateRaw(this, &FLiveLinkHubCaptureSource::CreateMessagingWidget));

	TUniquePtr<FMessagingCapability> Messaging = MakeUnique<FMessagingCapability>();

	Messaging->SubscribeToEvent(FMessagingEvent::Name, FCaptureEventHandler(FCaptureEventHandler::Type::CreateRaw(this, &FLiveLinkHubCaptureSource::EventHandler), EDelegateExecutionThread::InternalThread));

	MessagingCapability = Messaging.Get();
	OwnedCapabilities.Add(FMessagingCapability::Name, MoveTemp(Messaging));

	AddCapability(MessagingCapability);

	MessagingCapability->Initialize();

	MessagingCapability->SetDisconnectHandler(FMessagingCapability::FDisconnectHandler::CreateRaw(this, &FLiveLinkHubCaptureSource::OnDisconnect));
}

FCaptureVoidResult FLiveLinkHubCaptureSource::Start()
{
	MessagingCapability->Connect(FMessagingCapability::FMessageCallback::CreateLambda([this](const FName& InResponseName, const FBaseResponse& InResponse)
	{
		check(InResponseName == FConnectResponse::StaticStruct()->GetFName());

		if (InResponse.Status == EStatus::Ok)
		{
			MessagingCapability->SendGetCapabilities(FMessagingCapability::FMessageCallback::CreateRaw(this, &FLiveLinkHubCaptureSource::HandleGetCapabilities));

			PublishEvent<FReachableEvent>(true);
		}
		else
		{
			PublishEvent<FReachableEvent>(false);
		}
	}));

	return MakeValue();
}

FCaptureVoidResult FLiveLinkHubCaptureSource::Stop()
{
	return MakeValue();
}

void FLiveLinkHubCaptureSource::HandleGetCapabilities(const FName& InResponseName, const FBaseResponse& InResponse)
{
	check(InResponseName == FGetCapabilitiesResponse::StaticStruct()->GetFName());

	if (InResponse.Status == EStatus::Ok)
	{
		const FGetCapabilitiesResponse& Response = static_cast<const FGetCapabilitiesResponse&>(InResponse);

		for (const FString& Capability : Response.Capabilities)
		{
			if (TUniquePtr<FCaptureSourceCapability> CapabilityPtr = CreateCapability(Capability); CapabilityPtr.IsValid())
			{
				OwnedCapabilities.Add(Capability, MoveTemp(CapabilityPtr));
			}
		}
	}
	else
	{
		UE_LOG(LogLiveLinkHubCaptureSource, Error, TEXT("Failed to get the capabilities from LiveLink Hub: %s"), *InResponse.Message);
	}
}

TUniquePtr<FCaptureSourceCapability> FLiveLinkHubCaptureSource::CreateCapability(const FString& InName)
{
	if (InName == FDiskCapacityCapability::Name)
	{
		TUniquePtr<FDiskCapacityCapability> DiskCapacity = CreateCapability<FDiskCapacityCapability>();

		DiskCapacity->SetRemainingSpacePropertyHandler(FCaptureSourceCapability::FPropertyGetter::CreateRaw(this, &FLiveLinkHubCaptureSource::PropertyGetter, FDiskCapacityCapability::Name));
		DiskCapacity->SetTotalSpacePropertyHandler(FCaptureSourceCapability::FPropertyGetter::CreateRaw(this, &FLiveLinkHubCaptureSource::PropertyGetter, FDiskCapacityCapability::Name));

		DiskCapacity->SetUpdateCmdHandler(FCaptureSourceCapability::FCommandHandler::CreateRaw(this, &FLiveLinkHubCaptureSource::CommandHandler, FDiskCapacityCapability::Name));

		AddCapability(DiskCapacity.Get());

		// Get state
		MessagingCapability->SendGetPropertyValue(FDiskCapacityCapability::Name, FDiskCapacityCapability::TotalSpace, 
												  FMessagingCapability::FMessageCallback::CreateRaw(this, &FLiveLinkHubCaptureSource::GetPropertyHandler, FDiskCapacityCapability::TotalSpace));
		MessagingCapability->SendGetPropertyValue(FDiskCapacityCapability::Name, FDiskCapacityCapability::RemainingSpace, 
												  FMessagingCapability::FMessageCallback::CreateRaw(this, &FLiveLinkHubCaptureSource::GetPropertyHandler, FDiskCapacityCapability::RemainingSpace));
		MessagingCapability->SendGetPropertyValue(FDiskCapacityCapability::Name, FDiskCapacityCapability::UsedSpace, 
												  FMessagingCapability::FMessageCallback::CreateRaw(this, &FLiveLinkHubCaptureSource::GetPropertyHandler, FDiskCapacityCapability::UsedSpace));

		return DiskCapacity;
	}
	else if (InName == FRecordingCapability::Name)
	{
		TUniquePtr<FRecordingCapability> Recording = CreateCapability<FRecordingCapability>();

		Recording->SetStartRecordingHandler(FCaptureSourceCapability::FCommandHandler::CreateRaw(this, &FLiveLinkHubCaptureSource::CommandHandler, FRecordingCapability::Name));
		Recording->SetStopRecordingHandler(FCaptureSourceCapability::FCommandHandler::CreateRaw(this, &FLiveLinkHubCaptureSource::CommandHandler, FRecordingCapability::Name));

		AddCapability(Recording.Get());

		return Recording;
	}
	else
	{
		UE_LOG(LogLiveLinkHubCaptureSource, Warning, TEXT("Unsupported capability: %s"), *InName);
		return nullptr;
	}
}

FPropertyValue FLiveLinkHubCaptureSource::PropertyGetter(const FString& InProperty, FString InCapability)
{
	FScopeLock Lock(&Mutex);

	if (PropertyValues.Contains(InProperty))
	{
		return PropertyValues[InProperty];
	}

	return FPropertyValue();
}

void FLiveLinkHubCaptureSource::CommandHandler(TSharedPtr<FCommandBase> InCommand, FString InCapability)
{
	MessagingCapability->SendExecuteCommand(InCapability, 
											MoveTemp(InCommand),
											FMessagingCapability::FMessageCallback::CreateRaw(this, &FLiveLinkHubCaptureSource::HandleCommandResponse, InCapability));
}

void FLiveLinkHubCaptureSource::HandleCommandResponse(const FName& InName, const FBaseResponse& InResponse, FString InCapability)
{
	check(InName == FExecuteCommandResponse::StaticStruct()->GetFName());

	if (InResponse.Status != EStatus::Ok)
	{
		UE_LOG(LogLiveLinkHubCaptureSource, Error, TEXT("Failed to execute a command for [%s]: %s"), *InCapability, *InResponse.Message);
	}
}

void FLiveLinkHubCaptureSource::GetPropertyHandler(const FName& InName, const FBaseResponse& InResponse, FString InProperty)
{
	check(InName == FGetPropertyValueResponse::StaticStruct()->GetFName());

	const FGetPropertyValueResponse& Response = static_cast<const FGetPropertyValueResponse&>(InResponse);

	FPropertyValue Value = FMessagingCapability::ParseValue(Response.Value);

	FScopeLock Lock(&Mutex);

	if (!PropertyValues.Contains(InProperty))
	{
		PropertyValues.Add(MoveTemp(InProperty), MoveTemp(Value));
	}
	else
	{
		PropertyValues[MoveTemp(InProperty)] = MoveTemp(Value);
	}
}

void FLiveLinkHubCaptureSource::EventHandler(TSharedPtr<const FCaptureEvent> InEvent)
{
	check(InEvent->GetName() == FMessagingEvent::Name);

	TSharedPtr<const FMessagingEvent> Event = StaticCastSharedPtr<const FMessagingEvent>(InEvent);

	if (Event->EventName == FCapturePropertyChangedEvent::EventName)
	{
		checkf(OwnedCapabilities.Contains(Event->Capability), TEXT("Source doesn't contain given capability"));

		OwnedCapabilities[Event->Capability]->PublishPropertyChangedEvent(
			Event->Params[FCapturePropertyChangedEvent::Property].Get<FString>(), Event->Params[FCapturePropertyChangedEvent::PropertyValue]);
	}
	else
	{
		UE_LOG(LogLiveLinkHubCaptureSource, Warning, TEXT("Unsupported event arrived %s"), *(Event->EventName));
	}
}

void FLiveLinkHubCaptureSource::OnDisconnect()
{
	UE_LOG(LogTemp, Display, TEXT("Connection to the LiveLinkHub lost"));

	PublishEvent<FReachableEvent>(false);
}

TSharedPtr<SWidget> FLiveLinkHubCaptureSource::CreateMessagingWidget(int32, FCaptureSourceCapability*)
{
	return SNullWidget::NullWidget;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/EventSourceUtils.h"

#include "CaptureSourceCapabilityProperty.h"
#include "CaptureSourceCapabilityCommand.h"
#include "CaptureSourceCapabilityEvent.h"

struct FCaptureSourceCapabilityDesc
{
	FString Name;
	TArray<FPropertyDesc> Properties;
	TArray<FCommandDesc> Commands;

	FCaptureSourceCapabilityDesc(FString InName, TArray<FPropertyDesc> InProperties, TArray<FCommandDesc> InCommands);
};

struct CAPTURESOURCEFRAMEWORK_API FCapturePropertyChangedEvent : public FCaptureEvent
{
public:
	static const FString EventName;
	static const FString Property;
	static const FString PropertyValue;

	FCapturePropertyChangedEvent(const FString& InName, FPropertyValue InValue);

	const FString Name;
	const FPropertyValue Value;
};

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceCapability : public FCaptureEventSource
{
public:

	DECLARE_DELEGATE_TwoParams(FPropertySetter, const FString&, FPropertyValue);
	DECLARE_DELEGATE_RetVal_OneParam(FPropertyValue, FPropertyGetter, const FString&);
	DECLARE_DELEGATE_OneParam(FCommandHandler, TSharedPtr<FCommandBase>);

	virtual ~FCaptureSourceCapability() override = default;

	void PublishPropertyChangedEvent(const FString& InName, FPropertyValue InValue);

	void SetPropertyValue(const FString& InName, FPropertyValue InValue);
	FPropertyValue GetPropertyValue(const FString& InName);

	void ExecuteCommand(TSharedPtr<FCommandBase> InCommand);

	TArray<FPropertyDesc> GetProperties() const;
	TArray<FCommandDesc> GetCommands() const;
	TArray<FEventDesc> GetEvents() const;

	FString GetName() const;

	FCaptureSourceCapabilityDesc CreateDescriptor();

protected:

	FCaptureSourceCapability(FString InName);

	void SetPropertySetter(const FString& InName, FPropertySetter InSetter);
	void SetPropertyGetter(const FString& InName, FPropertyGetter InGetter);
	void SetCommandHandler(const FString& InName, FCommandHandler InCommandHandler);

	void AddProperty(FPropertyDesc InPropertyDesc);
	void AddCommand(FCommandDesc InCommand);
	void AddEvent(FEventDesc InEvent);

private:

	FPropertyList Properties;
	FCommandList Commands;
	FEventList Events;

	FString Name;

	TMap<FString, FPropertySetter> Setters;
	TMap<FString, FPropertyGetter> Getters;
	TMap<FString, FCommandHandler> CommandHandlers;
};

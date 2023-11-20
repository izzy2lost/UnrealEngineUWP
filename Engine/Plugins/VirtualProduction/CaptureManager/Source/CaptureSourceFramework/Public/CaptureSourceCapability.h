// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/EventSourceUtils.h"

#include "CaptureSourceCapabilityProperty.h"
#include "CaptureSourceCapabilityCommand.h"

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
	static const FString& EventName;

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

	virtual ~FCaptureSourceCapability() = default;

	void PublishPropertyChangedEvent(const FString& InName, FPropertyValue InValue);

	void SetPropertyValue(const FString& InName, FPropertyValue InValue);
	FPropertyValue GetPropertyValue(const FString& InName);

	void ExecuteCommand(TSharedPtr<FCommandBase> InCommand);

	TArray<FPropertyDesc> GetProperties() const;
	TArray<FCommandDesc> GetCommands() const;

	FString GetName() const;

	FCaptureSourceCapabilityDesc CreateDescriptor();

protected:

	FCaptureSourceCapability(FString InName);

	void SetPropertySetter(const FString& InName, FPropertySetter InSetter);
	void SetPropertyGetter(const FString& InName, FPropertyGetter InGetter);
	void SetCommandHandler(const FString& InName, FCommandHandler InCommandHandler);

	void AddProperty(FPropertyDesc InPropertyDesc);
	void AddCommand(FCommandDesc InCommand);

private:

	FPropertyList Properties;
	FCommandList Commands;

	FString Name;

	TMap<FString, FPropertySetter> Setters;
	TMap<FString, FPropertyGetter> Getters;
	TMap<FString, FCommandHandler> CommandHandlers;
};

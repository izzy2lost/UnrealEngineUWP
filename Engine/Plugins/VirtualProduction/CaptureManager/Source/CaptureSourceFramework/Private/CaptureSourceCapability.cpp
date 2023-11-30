// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceCapability.h"

FCaptureSourceCapabilityDesc::FCaptureSourceCapabilityDesc(FString InName, TArray<FPropertyDesc> InProperties, TArray<FCommandDesc> InCommands)
	: Name(MoveTemp(InName))
	, Properties(MoveTemp(InProperties))
	, Commands(MoveTemp(InCommands))
{
}

const FString FCapturePropertyChangedEvent::EventName = TEXT("PropertyChanged");
const FString FCapturePropertyChangedEvent::Property = TEXT("Property");
const FString FCapturePropertyChangedEvent::PropertyValue = TEXT("Value");

FCapturePropertyChangedEvent::FCapturePropertyChangedEvent(const FString& InName, FPropertyValue InValue)
	: FCaptureEvent(EventName)
	, Name(InName)
	, Value(MoveTemp(InValue))
{
}

FCaptureSourceCapability::FCaptureSourceCapability(FString InName)
	: Name(MoveTemp(InName))
{
	AddEvent(FEventDesc(FCapturePropertyChangedEvent::EventName, 
						{
							FPropertyDesc(FCapturePropertyChangedEvent::Property, FPropertyDesc::EType::String),
							FPropertyDesc(FCapturePropertyChangedEvent::PropertyValue, FPropertyDesc::EType::Any)
						}));
}

void FCaptureSourceCapability::PublishPropertyChangedEvent(const FString& InName, FPropertyValue InValue)
{
	Properties.CheckPropertyValueTypeMatch(InName, InValue);

	PublishEvent<FCapturePropertyChangedEvent>(InName, MoveTemp(InValue));
}

void FCaptureSourceCapability::SetPropertyValue(const FString& InName, FPropertyValue InValue)
{
	Properties.CheckPropertyAllowsWrite(InName);
	Properties.CheckPropertyValueTypeMatch(InName, InValue);

	if (Setters.Contains(InName))
	{
		FPropertySetter& Setter = Setters[InName];
		if (Setter.IsBound())
		{
			Setter.Execute(InName, MoveTemp(InValue));
		}
	}
}

FPropertyValue FCaptureSourceCapability::GetPropertyValue(const FString& InName)
{
	Properties.CheckPropertyExists(InName);

	if (Getters.Contains(InName))
	{
		FPropertyGetter& Getter = Getters[InName];
		if (Getter.IsBound())
		{
			return Getter.Execute(InName);
		}
	}
	
	return FPropertyValue();
}

TArray<FPropertyDesc> FCaptureSourceCapability::GetProperties() const
{
	return Properties.GetProperties();
}

TArray<FCommandDesc> FCaptureSourceCapability::GetCommands() const
{
	return Commands.GetCommands();
}

TArray<FEventDesc> FCaptureSourceCapability::GetEvents() const
{
	return Events.GetEvents();
}

FString FCaptureSourceCapability::GetName() const
{
	return Name;
}

FCaptureSourceCapabilityDesc FCaptureSourceCapability::CreateDescriptor()
{
	return FCaptureSourceCapabilityDesc(Name, Properties.GetProperties(), Commands.GetCommands());
}

void FCaptureSourceCapability::SetPropertySetter(const FString& InName, FPropertySetter InSetter)
{
	Properties.CheckPropertyAllowsWrite(InName);

	Setters.Emplace(InName, MoveTemp(InSetter));
}

void FCaptureSourceCapability::SetPropertyGetter(const FString& InName, FPropertyGetter InGetter)
{
	Properties.CheckPropertyExists(InName);

	Getters.Emplace(InName, MoveTemp(InGetter));
}

void FCaptureSourceCapability::SetCommandHandler(const FString& InName, FCommandHandler InCommandHandler)
{
	Commands.CheckCommandExists(InName);

	CommandHandlers.Emplace(InName, MoveTemp(InCommandHandler));
}

void FCaptureSourceCapability::AddProperty(FPropertyDesc InPropertyDesc)
{
	Properties.AddProperty(MoveTemp(InPropertyDesc));
}

void FCaptureSourceCapability::AddCommand(FCommandDesc InCommand)
{
	Commands.AddCommand(MoveTemp(InCommand));
}

void FCaptureSourceCapability::AddEvent(FEventDesc InEvent)
{
	RegisterEvent(InEvent.Name);

	Events.AddEvent(MoveTemp(InEvent));
}

void FCaptureSourceCapability::ExecuteCommand(TSharedPtr<FCommandBase> InCommand)
{
	const FString& CommandName = InCommand->GetName();

	Commands.CheckCommandParamValue(CommandName, InCommand);

	if (CommandHandlers.Contains(CommandName))
	{
		FCommandHandler& Handler = CommandHandlers[CommandName];
		if (Handler.IsBound())
		{
			Handler.Execute(MoveTemp(InCommand));
		}
	}
}
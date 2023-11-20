// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceCapability.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCaptureSourceCapabilityTest, "Plugin.CaptureSourceFramework.CaptureSourceCapability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::MediumPriority)

class FTestCommand final : public FCommandBase
{
public:
	static const FString Command;
	static const FString CommandParam;

	FTestCommand()
		: FCommandBase(Command)
		, bReturnValue(false)
	{
	}

	bool bReturnValue;
};

class FTestCapability final : public FCaptureSourceCapability
{
public:
	static const FString Name;

	static const FString ReadWriteProperty;
	static const FString ReadOnlyProperty;

	static const FString TestEvent;

	static const FString DefaultROPropertyValue;

	FTestCapability()
		: FCaptureSourceCapability(Name)
	{
		AddProperty(
			FPropertyDesc(ReadWriteProperty, 
						  FPropertyDesc::EType::String));
		AddProperty(
			FPropertyDesc(ReadOnlyProperty, 
						  FPropertyDesc::EType::String, 
						  FPropertyDesc::EAccess::ReadOnly));
		AddCommand(
			FCommandDesc(FTestCommand::Command,
						 {
							 FPropertyDesc(FTestCommand::CommandParam, FPropertyDesc::EType::String)
						 }));

		RegisterEvent(TestEvent);

		SetPropertyGetter(ReadWriteProperty, FPropertyGetter::CreateRaw(this, &FTestCapability::GetPropertyValues));
		SetPropertySetter(ReadWriteProperty, FPropertySetter::CreateRaw(this, &FTestCapability::SetRWPropertyValue));

		SetPropertyGetter(ReadOnlyProperty, FPropertyGetter::CreateRaw(this, &FTestCapability::GetPropertyValues));

		SetCommandHandler(FTestCommand::Command, FCommandHandler::CreateRaw(this, &FTestCapability::HandleCommand));

		ROPropertyValue = DefaultROPropertyValue;
	}

private:

	FPropertyValue GetPropertyValues(const FString& InName) const
	{
		if (ReadWriteProperty == InName)
		{
			return MakePropertyValue(RWPropertyValue);
		}
		else
		{
			return MakePropertyValue(ROPropertyValue);
		}
	}

	void SetRWPropertyValue(const FString& InName, FPropertyValue InValue)
	{
		RWPropertyValue = InValue.Get<FString>();

		PublishPropertyChangedEvent(InName, MoveTemp(InValue));
	}

	void HandleCommand(TSharedPtr<FCommandBase> InCommand)
	{
		StaticCastSharedPtr<FTestCommand>(InCommand)->bReturnValue = true;
	}

	FString RWPropertyValue;
	FString ROPropertyValue;
};

TUniquePtr<FCaptureSourceCapability> TestCapability;

END_DEFINE_SPEC(FCaptureSourceCapabilityTest)

bool operator==(const FPropertyDesc& InLeft, const FPropertyDesc& InRight)
{
	return InLeft.Name == InRight.Name &&
		InLeft.Type == InRight.Type &&
		InLeft.Access == InRight.Access;
}

bool operator==(const FCommandDesc& InLeft, const FCommandDesc& InRight)
{
	return InLeft.Name == InRight.Name && 
		InLeft.Params == InRight.Params;
}

const FString FCaptureSourceCapabilityTest::FTestCommand::Command = TEXT("Command");
const FString FCaptureSourceCapabilityTest::FTestCommand::CommandParam = TEXT("CommandParam");

const FString FCaptureSourceCapabilityTest::FTestCapability::Name = TEXT("TestCapability");
const FString FCaptureSourceCapabilityTest::FTestCapability::ReadWriteProperty = TEXT("ReadWriteProperty");
const FString FCaptureSourceCapabilityTest::FTestCapability::ReadOnlyProperty = TEXT("ReadOnlyProperty");
const FString FCaptureSourceCapabilityTest::FTestCapability::TestEvent = TEXT("TestEvent");

const FString FCaptureSourceCapabilityTest::FTestCapability::DefaultROPropertyValue = TEXT("Default");

void FCaptureSourceCapabilityTest::Define()
{
	TestCapability = MakeUnique<FTestCapability>();

	Describe("GetProperties()", [this]()
	{
		It("should return all registered properties", [this]()
		{
			const TArray<FPropertyDesc> Properties = TestCapability->GetProperties();

			UTEST_EQUAL("Two properties are registered", Properties.Num(), 2);

			UTEST_EQUAL("First property name must match", Properties[0].Name, FTestCapability::ReadWriteProperty);
			UTEST_EQUAL("First property type must match", Properties[0].Type, FPropertyDesc::EType::String);
			UTEST_EQUAL("First property access must match", Properties[0].Access, FPropertyDesc::EAccess::ReadWrite);

			UTEST_EQUAL("Second property name must match", Properties[1].Name, FTestCapability::ReadOnlyProperty);
			UTEST_EQUAL("Second property type must match", Properties[1].Type, FPropertyDesc::EType::String);
			UTEST_EQUAL("Second property access must match", Properties[1].Access, FPropertyDesc::EAccess::ReadOnly);

			return true;
		});
	});

	Describe("GetCommands()", [this]()
	{
		It("should return all registered commands", [this]()
		{
			const TArray<FCommandDesc> Commands = TestCapability->GetCommands();

			UTEST_EQUAL("One command is registered", Commands.Num(), 1);
			UTEST_EQUAL("First command name must match", Commands[0].Name, FTestCommand::Command);

			const TArray<FPropertyDesc>& Params = Commands[0].Params;
			UTEST_EQUAL("First param name must match", Params[0].Name, FTestCommand::CommandParam);
			UTEST_EQUAL("First param type must match", Params[0].Type, FPropertyDesc::EType::String);
			UTEST_EQUAL("First param access must match", Params[0].Access, FPropertyDesc::EAccess::ReadWrite);

			return true;
		});
	});

	Describe("PublishPropertyChangedEvent()", [this]()
	{
		FString Value = TEXT("Value");
		TSharedPtr<bool> EventReturn = MakeShared<bool>(false);

		FCaptureEventHandler Handler = FCaptureEventHandler(
			[this, Value, EventReturn](TSharedPtr<const FCaptureEvent> InEvent)
		{
			TSharedPtr<const FCapturePropertyChangedEvent> Event = StaticCastSharedPtr<const FCapturePropertyChangedEvent>(InEvent);

			if (!TestEqual("Event should carry property name that is set", Event->Name, FTestCapability::ReadOnlyProperty))
			{
				*EventReturn = false;
			}

			if (!TestEqual("Event should carry value that is set", Event->Value.Get<FString>(), Value))
			{
				*EventReturn = false;
			}

			*EventReturn = true;

		}, EDelegateExecutionThread::InternalThread);

		TestCapability->SubscribeToEvent(FCapturePropertyChangedEvent::EventName, MoveTemp(Handler));

		It("should correctly publish the event", [this, Value, EventReturn]()
		{
			TestCapability->PublishPropertyChangedEvent(FTestCapability::ReadOnlyProperty, MakePropertyValue(Value));

			UTEST_TRUE("Event values are correct", *EventReturn);

			TestCapability->UnsubscribeAll();

			return true;
		});
	});

	Describe("GetPropertyValue()", [this]()
	{
		It("should get the correct value of the property", [this]()
		{
			FPropertyValue PropertyValue = TestCapability->GetPropertyValue(FTestCapability::ReadOnlyProperty);

			UTEST_EQUAL("Values should match", FTestCapability::DefaultROPropertyValue, PropertyValue.Get<FString>());

			return true;
		});
	});

	Describe("SetPropertyValue()", [this]()
	{
		It("should set the correct value to the property", [this]()
		{
			FString Value = TEXT("Value");

			TestCapability->SetPropertyValue(FTestCapability::ReadWriteProperty, MakePropertyValue(Value));
			
			FPropertyValue PropertyValue = TestCapability->GetPropertyValue(FTestCapability::ReadWriteProperty);

			UTEST_EQUAL("Set value should match fetched value", Value, PropertyValue.Get<FString>());

			return true;
		});
	});

	Describe("ExecuteCommand()", [this]()
	{
		It("should correctly execute a command", [this]()
		{
			TSharedPtr<FTestCommand> Command = MakeShared<FTestCommand>();
			Command->AddParamValue(FTestCommand::CommandParam, MakePropertyValue<FString>(TEXT("TestValue")));

			TestCapability->ExecuteCommand(Command);

			UTEST_TRUE("Return value must be true", Command->bReturnValue);

			return true;
		});
	});

	Describe("GetName()", [this]()
	{
		It("should get the correct capability name", [this]()
		{
			FString Name = TestCapability->GetName();

			UTEST_EQUAL("Name values must match", Name, FTestCapability::Name);

			return true;
		});
	});

	Describe("CreateDescriptor()", [this]()
	{
		It("should get the valid capability descriptor", [this]()
		{
			FCaptureSourceCapabilityDesc Descriptor = TestCapability->CreateDescriptor();

			TArray<FPropertyDesc> Properties = TestCapability->GetProperties();

			UTEST_EQUAL("Name values must match", Descriptor.Name, TestCapability->GetName());
			UTEST_EQUAL("Properties must match", Descriptor.Properties, TestCapability->GetProperties());
			UTEST_EQUAL("Commands must match", Descriptor.Commands, TestCapability->GetCommands());

			return true;
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
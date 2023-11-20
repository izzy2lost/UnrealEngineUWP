// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceDetailsCustomization.h"

#include "CaptureSourceFrameworkModule.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SNumericEntryBox.h"

static FString DefaultBase = TEXT("Default");

FCaptureSourceDetailsCustomization::FCaptureSourceDetailsCustomization()
{
}

void FCaptureSourceDetailsCustomization::RegisterPropertyCustomization(const FString& InCaptureSourceClassId,
																	   const FString& InCapabilityName, 
																	   const FString& InPropertyName, 
																	   FPropertyWidgetCreator InWidgetCreator)
{
	PropertyCustomizations.Emplace(GenerateKey(InCaptureSourceClassId, InCapabilityName, InPropertyName), MoveTemp(InWidgetCreator));
}

void FCaptureSourceDetailsCustomization::RegisterCommandCustomization(const FString& InCaptureSourceClassId,
																	  const FString& InCapabilityName,
																	  const FString& InCommandName,
																	  FCommandWidgetCreator InWidgetCreator)
{
	CommandCustomizations.Emplace(GenerateKey(InCaptureSourceClassId, InCapabilityName, InCommandName), MoveTemp(InWidgetCreator));
}

void FCaptureSourceDetailsCustomization::RegisterCapabilityCustomization(const FString& InCaptureSourceClassId,
																		 const FString& InCapabilityName,
																		 FCapabilityWidgetCreator InWidgetCreator)
{
	CapabilityCustomizations.Emplace(GenerateKey(InCaptureSourceClassId, InCapabilityName), MoveTemp(InWidgetCreator));
}

void FCaptureSourceDetailsCustomization::RegisterCaptureSourceCustomization(const FString& InCaptureSourceClassId,
																			FCaptureSourceWidgetCreator InWidgetCreator)
{
	CreationParamsCustomizations.Emplace(GenerateKey(InCaptureSourceClassId), MoveTemp(InWidgetCreator));
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreatePropertyWidget(FCaptureSourceId InCaptureSourceId,
																			 FCaptureSourceCapability* InCapability, 
																			 const FPropertyDesc& InProperty)
{
	FCaptureSourceFrameworkModule& Module = FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework");
	const FString& InCaptureSourceFactory = Module.GetCaptureSourceManager().GetCaptureSourceFactoryById(InCaptureSourceId);

	FPropertyWidgetCreator* FoundDelegate = PropertyCustomizations.Find(GenerateKey(InCaptureSourceFactory, InCapability->GetName(), InProperty.Name));
	if (FoundDelegate && FoundDelegate->IsBound())
	{
		TSharedPtr<SWidget> Widget = FoundDelegate->Execute(InCaptureSourceId, InCapability, InProperty);
		return Widget;
	}

	return CreateDefaultPropertyWidget(InCapability, InProperty);
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateCommandWidget(FCaptureSourceId InCaptureSourceId,
																			FCaptureSourceCapability* InCapability, 
																			const FCommandDesc& InCommand)
{
	FCaptureSourceFrameworkModule& Module = FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework");
	const FString& InCaptureSourceFactory = Module.GetCaptureSourceManager().GetCaptureSourceFactoryById(InCaptureSourceId);

	FCommandWidgetCreator* FoundDelegate = CommandCustomizations.Find(GenerateKey(InCaptureSourceFactory, InCapability->GetName(), InCommand.Name));
	if (FoundDelegate && FoundDelegate->IsBound())
	{
		TSharedPtr<SWidget> Widget = FoundDelegate->Execute(InCaptureSourceId, InCapability, InCommand);
		return Widget;
	}

	return CreateDefaultCommandWidget(InCapability, InCommand);
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateCapabilityWidget(FCaptureSourceId InCaptureSourceId,
																			   FCaptureSourceCapability* InCapability)
{
	FCaptureSourceFrameworkModule& Module = FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework");
	const FString& InCaptureSourceFactory = Module.GetCaptureSourceManager().GetCaptureSourceFactoryById(InCaptureSourceId);

	FCapabilityWidgetCreator* FoundDelegate = CapabilityCustomizations.Find(GenerateKey(InCaptureSourceFactory, InCapability->GetName()));
	if (FoundDelegate && FoundDelegate->IsBound())
	{
		TSharedPtr<SWidget> Widget = FoundDelegate->Execute(InCaptureSourceId, InCapability);
		return Widget;
	}

	TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	TMap<FString, TSharedPtr<FPropertyValue>> PropertyValueHolder;

	for (const FPropertyDesc& Property : InCapability->GetProperties())
	{
		if (ContainsPropertyWidgetCustomization(InCaptureSourceFactory, InCapability->GetName(), Property.Name))
		{
			VerticalBox->AddSlot()
				.AutoHeight()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					CreatePropertyWidget(InCaptureSourceId, InCapability, Property).ToSharedRef()
				];
		}
		else
		{
			FPropertyValue Value = InCapability->GetPropertyValue(Property.Name);
			if (Value.IsType<FEmptyVariantState>())
			{
				Value = GetDefaultValueForProperty(Property);
			}

			PropertyValueHolder.Emplace(Property.Name, MakeShared<FPropertyValue>(MoveTemp(Value)));

			VerticalBox->AddSlot()
				.AutoHeight()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					CreateDefaultCapabilityPropertyWidget(InCapability, Property, PropertyValueHolder[Property.Name]).ToSharedRef()
				];
		}
	}

	InCapability->SubscribeToEvent(
		FCapturePropertyChangedEvent::EventName,
		FCaptureEventHandler::Type::CreateRaw(this, &FCaptureSourceDetailsCustomization::HandlePropertyChangedEvent, MoveTemp(PropertyValueHolder)));

	for (const FCommandDesc& Command : InCapability->GetCommands())
	{
		TSharedPtr<SVerticalBox> CommandVerticalBox = SNew(SVerticalBox);

		CommandVerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(Command.Name))
			];

		CommandVerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				CreateCommandWidget(InCaptureSourceId, InCapability, Command).ToSharedRef()
			];

		VerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				CommandVerticalBox.ToSharedRef()
			];
	}

	return VerticalBox;
}

TSharedPtr<SWindow> FCaptureSourceDetailsCustomization::CreateCaptureSourceWidget(const FCaptureSourceDescriptor& InCreationParams,
																				  FCaptureSourceCreated InCaptureSourceCreated)
{
	FCaptureSourceWidgetCreator* FoundDelegate = CreationParamsCustomizations.Find(InCreationParams.GetId());
	if (FoundDelegate && FoundDelegate->IsBound())
	{
		TSharedPtr<SWindow> Widget = FoundDelegate->Execute(InCreationParams, MoveTemp(InCaptureSourceCreated));
		return Widget;
	}

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(FString::Format(TEXT("Creating {0}"), { InCreationParams.GetId() })))
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	TSharedPtr<TMap<FString, FPropertyValue>> ParamValues = MakeShared<TMap<FString, FPropertyValue>>();

	// Name
	TSharedPtr<FPropertyValue> NameValue = MakeShared<FPropertyValue>(MakePropertyValue<FString>(TEXT("Unnamed")));

	TSharedRef<SWidget> NameWidget = 
		CreateParamWidget(FPropertyDesc(TEXT("Name"), FPropertyDesc::EType::String), *NameValue).ToSharedRef();

	VerticalBox->AddSlot()
		.AutoHeight()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.AttachWidget(MoveTemp(NameWidget));

	// Other creation params
	for (const FPropertyDesc& Property : InCreationParams.GetCreationParams())
	{
		ParamValues->Emplace(Property.Name, GetDefaultValueForProperty(Property));

		TSharedRef<SWidget> Widget = CreateParamWidget(Property, (*ParamValues)[Property.Name]).ToSharedRef();

		VerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			.AttachWidget(MoveTemp(Widget));
	}

	VerticalBox->AddSlot()
		.AutoHeight()
		.VAlign(EVerticalAlignment::VAlign_Bottom)
		.HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SButton)
				.Text(FText::FromString(TEXT("Create")))
				.OnClicked_Lambda([Window, NameValue, ParamValues, InCreationParams, Callback = MoveTemp(InCaptureSourceCreated)]()
			{
				FCaptureSourceFrameworkModule& Module = FModuleManager::LoadModuleChecked<FCaptureSourceFrameworkModule>("CaptureSourceFramework");
				FCaptureSourceManager::FCaptureSourceResult Result = Module.GetCaptureSourceManager().CreateCaptureSource(InCreationParams.GetId(), NameValue->Get<FString>(), *ParamValues);

				if (Result.IsValid())
				{
					Callback.ExecuteIfBound(Result.StealValue());
				}

				Window->RequestDestroyWindow();

				return FReply::Handled();
			})
		];

	Window->SetContent(VerticalBox.ToSharedRef());

	return Window;
}

FString FCaptureSourceDetailsCustomization::GenerateDefaultMapKey(const FString& InName)
{
	return GenerateKey(DefaultBase, InName);
}

bool FCaptureSourceDetailsCustomization::ContainsPropertyWidgetCustomization(const FString& InFactoryId,
																			 const FString& InCapabilityName,
																			 const FString& InPropertyName)
{
	return PropertyCustomizations.Contains(GenerateKey(InFactoryId, InCapabilityName, InPropertyName));
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateDefaultPropertyWidget(FCaptureSourceCapability* InCapability,
																					const FPropertyDesc& InProperty)
{
	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	FPropertyValue Value = InCapability->GetPropertyValue(InProperty.Name);
	if (Value.IsType<FEmptyVariantState>())
	{
		Value = GetDefaultValueForProperty(InProperty);
	}

	TSharedPtr<FPropertyValue> PropertyValue = MakeShared<FPropertyValue>(MoveTemp(Value));

	HorizontalBox->AddSlot()
		.AttachWidget(MakeKeyWidget(InProperty).ToSharedRef());

	HorizontalBox->AddSlot()
		.AttachWidget(MakeValueWidget(InCapability, InProperty, PropertyValue).ToSharedRef());

	return HorizontalBox;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateDefaultCapabilityPropertyWidget(FCaptureSourceCapability* InCapability,
																							  const FPropertyDesc& InProperty,
																							  TSharedPtr<FPropertyValue> InPropertyValue)
{
	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	HorizontalBox->AddSlot()
		.AttachWidget(MakeKeyWidget(InProperty).ToSharedRef());

	HorizontalBox->AddSlot()
		.AttachWidget(MakeValueWidget(InCapability, InProperty, MoveTemp(InPropertyValue)).ToSharedRef());

	return HorizontalBox;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateDefaultCommandWidget(FCaptureSourceCapability* InCapability,
																				   const FCommandDesc& InCommand)
{
	TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	TSharedPtr<FCommandBase::FValues> CommandValues = MakeShared<FCommandBase::FValues>();

	for (const FPropertyDesc& Property : InCommand.Params)
	{
		CommandValues->Emplace(Property.Name, GetDefaultValueForProperty(Property));

		TSharedRef<SWidget> Widget = CreateParamWidget(Property, (*CommandValues)[Property.Name]).ToSharedRef();

		VerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AttachWidget(MoveTemp(Widget));
	}

	VerticalBox->AddSlot()
		.AutoHeight()
		.VAlign(EVerticalAlignment::VAlign_Bottom)
		.HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SButton)
				.Text(FText::FromString(TEXT("Execute")))
				.OnClicked_Lambda([CommandName = InCommand.Name, CommandValues, InCapability]() mutable
			{
				TSharedPtr<FCommandBase> CommandPtr = MakeShared<FCommandBase>(CommandName);
				CommandPtr->SetParamValues(*CommandValues);

				InCapability->ExecuteCommand(MoveTemp(CommandPtr));

				return FReply::Handled();
			})
		];

	return VerticalBox;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::CreateParamWidget(const FPropertyDesc& InProperty,
																		  FPropertyValue& InPropertyValue)
{
	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(5.0f)
		.AttachWidget(MakeKeyWidget(InProperty).ToSharedRef());

	HorizontalBox->AddSlot()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.AttachWidget(MakeValueWidget(InProperty, InPropertyValue).ToSharedRef());

	return HorizontalBox;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::MakeKeyWidget(const FPropertyDesc& InProperty)
{
	TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
		.Text(FText::FromString(InProperty.Name))
		.TextStyle(FAppStyle::Get(), "NormalText");

	return TextBlock;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::MakeValueWidget(FCaptureSourceCapability* InCapability, 
																		const FPropertyDesc& InProperty, 
																		TSharedPtr<FPropertyValue> InPropertyValue)
{
	TSharedPtr<SWidget> Widget;

	switch (InProperty.Type)
	{
		case FPropertyDesc::EType::Bool:
			Widget = SNew(SCheckBox)
				.OnCheckStateChanged_Lambda([InPropertyValue, InCapability, Name = InProperty.Name](ECheckBoxState InCheckState)
			{
				InPropertyValue->Set<bool>(InCheckState == ECheckBoxState::Checked);
				InCapability->SetPropertyValue(Name, *InPropertyValue);
				
			})
				.IsChecked_Lambda([InPropertyValue]()
			{
				return InPropertyValue->Get<bool>() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
			break;
		case FPropertyDesc::EType::Number:
			Widget = SNew(SNumericEntryBox<int64>)
				.OnValueCommitted_Lambda([InPropertyValue, InCapability, Name = InProperty.Name](int64 InValue, ETextCommit::Type)
			{
				InPropertyValue->Set<int64>(InValue);
				InCapability->SetPropertyValue(Name, *InPropertyValue);
			})
				.Value_Lambda([InPropertyValue]()
			{
				return InPropertyValue->Get<int64>();
			});
			break;
		case FPropertyDesc::EType::FloatingPoint:
			Widget = SNew(SNumericEntryBox<double>)
				.OnValueCommitted_Lambda([InPropertyValue, InCapability, Name = InProperty.Name](double InValue, ETextCommit::Type)
			{
				InPropertyValue->Set<double>(InValue);
				InCapability->SetPropertyValue(Name, *InPropertyValue);
			})
				.Value_Lambda([InPropertyValue]()
			{
				return InPropertyValue->Get<double>();
			});
			break;
		case FPropertyDesc::EType::String:
			Widget = SNew(SEditableTextBox)
				.OnTextCommitted_Lambda([InPropertyValue, InCapability, Name = InProperty.Name](FText InValue, ETextCommit::Type)
			{
				InPropertyValue->Set<FString>(InValue.ToString());
				InCapability->SetPropertyValue(Name, *InPropertyValue);
			})
				.Text_Lambda([InPropertyValue]()
			{
				return FText::FromString(InPropertyValue->Get<FString>());
			});
			break;
		case FPropertyDesc::EType::ArrayBool:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayNumber:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayString:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayFloatingPoint:
		default:
			Widget = SNullWidget::NullWidget;
	}

	Widget->SetEnabled(InProperty.Access == FPropertyDesc::EAccess::ReadWrite);

	return Widget;
}

TSharedPtr<SWidget> FCaptureSourceDetailsCustomization::MakeValueWidget(const FPropertyDesc& InProperty, FPropertyValue& InPropertyValue)
{
	TSharedPtr<SWidget> Widget;
	switch (InProperty.Type)
	{
		case FPropertyDesc::EType::Bool:
			Widget = SNew(SCheckBox)
				.OnCheckStateChanged_Lambda([&InPropertyValue](ECheckBoxState InCheckState)
			{
				InPropertyValue.Set<bool>(InCheckState == ECheckBoxState::Checked);
			})
				.IsChecked_Lambda([&InPropertyValue]()
			{
				return InPropertyValue.Get<bool>() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
			break;
		case FPropertyDesc::EType::Number:
			Widget = SNew(SNumericEntryBox<int64>)
				.AllowSpin(false)
				.OnValueCommitted_Lambda([&InPropertyValue](int64 InValue, ETextCommit::Type)
			{
				InPropertyValue.Set<int64>(InValue);
			})
				.Value_Lambda([&InPropertyValue]()
			{
				return InPropertyValue.Get<int64>();
			});
			break;
		case FPropertyDesc::EType::FloatingPoint:
			Widget = SNew(SNumericEntryBox<double>)
				.AllowSpin(false)
				.OnValueCommitted_Lambda([&InPropertyValue](double InValue, ETextCommit::Type)
			{
				InPropertyValue.Set<double>(InValue);
			})
				.Value_Lambda([&InPropertyValue]()
			{
				return InPropertyValue.Get<double>();
			});
			break;
		case FPropertyDesc::EType::String:
			Widget = SNew(SEditableTextBox)
				.OnTextCommitted_Lambda([&InPropertyValue](FText InValue, ETextCommit::Type)
			{
				InPropertyValue.Set<FString>(InValue.ToString());
			})
				.Text_Lambda([&InPropertyValue]()
			{
				return FText::FromString(InPropertyValue.Get<FString>());
			});
			break;
		case FPropertyDesc::EType::ArrayBool:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayNumber:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayString:
			Widget = SNullWidget::NullWidget;
			break;
		case FPropertyDesc::EType::ArrayFloatingPoint:
		default:
			Widget = SNullWidget::NullWidget;
	}

	Widget->SetEnabled(InProperty.Access == FPropertyDesc::EAccess::ReadWrite);

	return Widget;
}

FPropertyValue FCaptureSourceDetailsCustomization::GetDefaultValueForProperty(const FPropertyDesc& InProperty)
{
	switch (InProperty.Type)
	{
		case FPropertyDesc::EType::Bool:
			return MakePropertyValue(false);
		case FPropertyDesc::EType::Number:
			return MakePropertyValue<int64>(0);
		case FPropertyDesc::EType::FloatingPoint:
			return MakePropertyValue<double>(0.0);
		case FPropertyDesc::EType::String:
			return MakePropertyValue<FString>(TEXT(""));
		case FPropertyDesc::EType::ArrayBool:
			return MakePropertyValue(FEmptyVariantState());
		case FPropertyDesc::EType::ArrayNumber:
			return MakePropertyValue(FEmptyVariantState());
		case FPropertyDesc::EType::ArrayString:
			return MakePropertyValue(FEmptyVariantState());
		case FPropertyDesc::EType::ArrayFloatingPoint:
		default:
			return MakePropertyValue(FEmptyVariantState());
	}
}

void FCaptureSourceDetailsCustomization::HandlePropertyChangedEvent(TSharedPtr<const FCaptureEvent> InEvent, 
																	TMap<FString, TSharedPtr<FPropertyValue>> InPropertyValueHolder)
{
	TSharedPtr<const FCapturePropertyChangedEvent> PropertyEvent = StaticCastSharedPtr<const FCapturePropertyChangedEvent>(InEvent);

	if (!InPropertyValueHolder.Contains(PropertyEvent->Name))
	{
		return;
	}

	TSharedPtr<FPropertyValue> BoundValue = InPropertyValueHolder[PropertyEvent->Name];
	*BoundValue = PropertyEvent->Value;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowFunctionsPropertyCustomization.h"
#include "Dataflow/DataflowFunctionsProperty.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "FunctionPropertyCustomization"

namespace Dataflow
{
	namespace Private
	{
		static const FDataflowFunctionsProperty* GetFunctionProperty(const TSharedPtr<IPropertyHandle>& PropertyHandle)
		{
			const FStructProperty* const StructProperty = CastField<FStructProperty>(PropertyHandle->GetProperty());
			check(StructProperty && StructProperty->Struct);
			checkf(StructProperty->Struct->IsA(FDataflowFunctionsProperty::StaticStruct()->StaticClass()),
				TEXT("This customisation can only apply to FDataflowFunctionsProperty and its derived structures."));

			const FDataflowFunctionsProperty* FunctionProperty = nullptr;
			void* Data;
			if (PropertyHandle && PropertyHandle->GetValueData(Data) == FPropertyAccess::Success)
			{
				FunctionProperty = reinterpret_cast<const FDataflowFunctionsProperty*>(Data);
			}
			return FunctionProperty;
		}
	}

	TSharedRef<IPropertyTypeCustomization> FFunctionsPropertyCustomization::MakeInstance()
	{
		return MakeShareable(new FFunctionsPropertyCustomization);
	}

	void FFunctionsPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		StructProperty = StructPropertyHandle;

		TSharedPtr<SWrapBox> WrapBox;
		HeaderRow
		[
			SAssignNew(WrapBox, SWrapBox)
			.PreferredSize(2000)  // Copied from FObjectDetails::AddCallInEditorMethods()
			.UseAllottedSize(true)
		];

		const FDataflowFunctionsProperty* const FunctionBlockProperty = Private::GetFunctionProperty(StructPropertyHandle);
		check(FunctionBlockProperty);

		for (const FDataflowFunctionsProperty::FFunction& Function : FunctionBlockProperty->GetFunctions())
		{
			auto OnClicked = [this, &Function]() -> FReply
				{
					// Execute function
					Function.Delegate.ExecuteIfBound();
					StructProperty->NotifyFinishedChangingProperties();  // Triggers node invalidation
					return FReply::Handled();
				};

			if (!Function.ImageStyle.IsNone())
			{
				const FString& ButtonImage = StructProperty->GetMetaData("ButtonImage");  // e.g. FAppStyle::GetBrush("Persona.ReimportAsset") will be Meta = (ButtonImage = "Persona.ReimportAsset")
				if (Function.Name.IsEmptyOrWhitespace())
				{
					// Create button image
					WrapBox->AddSlot()
						.Padding(0.f, 0.f, 5.f, 3.f)
						[
							SNew(SButton)
							.ToolTipText(Function.ToolTip)
							.OnClicked_Lambda(MoveTemp(OnClicked))
							.ContentPadding(FMargin(0.f, 4.f))  // Too much horizontal padding otherwise (default is 4, 2)
							.IsEnabled_Lambda([this]() -> bool { return StructProperty->IsEditable(); })
							[
								SNew(SImage)
								.DesiredSizeOverride(FVector2D(16, 16))
								.Image(FAppStyle::GetBrush(Function.ImageStyle))
							]
						];
				}
				else
				{
					// Create button image + text
					WrapBox->AddSlot()
						.Padding(0.f, 0.f, 5.f, 3.f)
						[
							SNew(SButton)
							.Text(Function.Name)
							.ToolTipText(Function.ToolTip)
							.OnClicked_Lambda(MoveTemp(OnClicked))
							.ContentPadding(FMargin(0.f, 2.f))  // Too much horizontal padding otherwise (default is 4, 2)
							.IsEnabled_Lambda([this]() -> bool { return StructProperty->IsEditable(); })
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0, 2)
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(16, 16))
									.Image(FAppStyle::GetBrush(Function.ImageStyle))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(5.f, 0.f)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(Function.Name)
								]
							]
						];
				}
			}
			else
			{
				// Create button text
				WrapBox->AddSlot()
					.Padding(0.f, 0.f, 5.f, 3.f)
					[
						SNew(SButton)
						.Text(Function.Name)
						.ToolTipText(Function.ToolTip)
						.OnClicked_Lambda(MoveTemp(OnClicked))
						.ContentPadding(FMargin(0.f, 2.f))  // Too much horizontal padding otherwise (default is 4, 2)
						.VAlign(VAlign_Center)  // No Slot, so need vertical align
						.IsEnabled_Lambda([this]() -> bool { return StructProperty->IsEditable(); })
					];
			}
		}
	}
}  // End namespace Dataflow

#undef LOCTEXT_NAMESPACE

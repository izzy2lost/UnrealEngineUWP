// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterCameraComponentDetailsCustomization.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "DisplayClusterRootActor.h"
#include "Camera/CameraComponent.h"

#include "DisplayClusterProjectionStrings.h"
#include "DisplayClusterConfiguratorLog.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"
#include "Views/Details/Widgets/SDisplayClusterConfiguratorComponentPicker.h"
#include "Views/Details/Widgets/SDisplayClusterConfigurationSearchableComboBox.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "DisplayClusterCameraComponentDetailsCustomization"

void FDisplayClusterCameraComponentDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& InLayoutBuilder)
{
	Super::CustomizeDetails(InLayoutBuilder);

	DetailLayout = &InLayoutBuilder;

	if (!EditedObject.IsValid())
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		InLayoutBuilder.GetObjectsBeingCustomized(Objects);

		for (TWeakObjectPtr<UObject> Object : Objects)
		{
			if (Object->IsA<UDisplayClusterCameraComponent>())
			{
				EditedObject = Cast<UDisplayClusterCameraComponent>(Object.Get());
			}
		}
	}

	if (EditedObject.IsValid())
	{
		NoneOption = MakeShared<FString>("Active Engine Camera");

		CameraHandle = InLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDisplayClusterCameraComponent, OuterViewportCameraName));
		check(CameraHandle->IsValidHandle());

		RebuildCameraOptions();

		if (IDetailPropertyRow* CameraPropertyRow = InLayoutBuilder.EditDefaultProperty(CameraHandle))
		{
			CameraPropertyRow->CustomWidget()
				.NameContent()
				[
					CameraHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					CreateCustomCameraWidget()
				];
		}
	}
}

void FDisplayClusterCameraComponentDetailsCustomization::RebuildCameraOptions()
{
	CameraOptions.Reset();
	UDisplayClusterCameraComponent* DestCameraComponent = EditedObject.Get();
	check(DestCameraComponent != nullptr);

	AActor* RootActor = GetRootActor();
	check(RootActor);

	TArray<UActorComponent*> ActorComponents;
	RootActor->GetComponents(UCameraComponent::StaticClass(), ActorComponents);
	for (UActorComponent* ActorComponent : ActorComponents)
	{
		const FString ComponentName = ActorComponent->GetName();
		CameraOptions.Add(MakeShared<FString>(ComponentName));
	}

	// Component order not guaranteed, sort for consistency.
	CameraOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
	{
		// Default sort isn't compatible with TSharedPtr<FString>.
		return *A < *B;
	});

	// Add None option
	if (!DestCameraComponent->OuterViewportCameraName.IsEmpty())
	{
		CameraOptions.Add(NoneOption);
	}
}

TSharedRef<SWidget> FDisplayClusterCameraComponentDetailsCustomization::CreateCustomCameraWidget()
{
	if (CameraComboBox.IsValid())
	{
		return CameraComboBox.ToSharedRef();
	}

	return SAssignNew(CameraComboBox, SDisplayClusterConfigurationSearchableComboBox)
		.OptionsSource(&CameraOptions)
		.OnGenerateWidget(this, &FDisplayClusterCameraComponentDetailsCustomization::MakeCameraOptionComboWidget)
		.OnSelectionChanged(this, &FDisplayClusterCameraComponentDetailsCustomization::OnCameraSelected)
		.ContentPadding(2)
		.MaxListHeight(200.0f)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FDisplayClusterCameraComponentDetailsCustomization::GetSelectedCameraText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

TSharedRef<SWidget> FDisplayClusterCameraComponentDetailsCustomization::MakeCameraOptionComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

void FDisplayClusterCameraComponentDetailsCustomization::OnCameraSelected(TSharedPtr<FString> InCamera, ESelectInfo::Type SelectInfo)
{
	if (InCamera.IsValid())
	{
		// Handle empty case
		if (InCamera->Equals(*NoneOption.Get()))
		{
			CameraHandle->SetValue(TEXT(""));
		}
		else
		{
			CameraHandle->SetValue(*InCamera.Get());
		}

		// Reset available options
		RebuildCameraOptions();
		CameraComboBox->ResetOptionsSource(&CameraOptions);
		CameraComboBox->SetIsOpen(false);
	}
}

FText FDisplayClusterCameraComponentDetailsCustomization::GetSelectedCameraText() const
{
	FString SelectedOption = EditedObject.Get()->OuterViewportCameraName;
	if (SelectedOption.IsEmpty())
	{
		SelectedOption = *NoneOption.Get();
	}
	return FText::FromString(SelectedOption);
}

#undef LOCTEXT_NAMESPACE

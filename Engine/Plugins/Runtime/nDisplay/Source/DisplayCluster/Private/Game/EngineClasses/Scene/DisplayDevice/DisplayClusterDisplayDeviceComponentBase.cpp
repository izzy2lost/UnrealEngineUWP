// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayDevice/DisplayClusterDisplayDeviceBaseComponent.h"

#include "DisplayClusterRootActor.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UDisplayClusterDisplayDeviceBaseComponent::UDisplayClusterDisplayDeviceBaseComponent()
{
#if WITH_EDITOR
	static ConstructorHelpers::FObjectFinder<UMaterial> PreviewMaterialObj(TEXT("/nDisplay/Materials/Preview/M_DisplayDevicePreview"));
	check(PreviewMaterialObj.Object);
	PreviewMaterial = PreviewMaterialObj.Object;

	static ConstructorHelpers::FObjectFinder<UMaterial> MeshMaterialObj(TEXT("/nDisplay/Materials/Preview/M_DisplayDeviceMesh"));
	check(MeshMaterialObj.Object);
	MeshMaterial = MeshMaterialObj.Object;
#endif
}

#if WITH_EDITOR
void UDisplayClusterDisplayDeviceBaseComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDisplayClusterDisplayDeviceBaseComponent, PreviewMaterial)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDisplayClusterDisplayDeviceBaseComponent, MeshMaterial))
	{
		if (ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
		{
			RootActor->ResetPreviewComponents_Editor(true);
		}
	}
}
#endif

UDisplayClusterDisplayDeviceComponent::UDisplayClusterDisplayDeviceComponent()
{
}

void UDisplayClusterDisplayDeviceComponent::OnUpdatePreviewMaterialInstance(UMaterialInstanceDynamic* InMaterialInstance)
{
	Super::OnUpdatePreviewMaterialInstance(InMaterialInstance);

	if (InMaterialInstance)
	{
		InMaterialInstance->SetScalarParameterValue(TEXT("Exposure"), Exposure);
	}
}

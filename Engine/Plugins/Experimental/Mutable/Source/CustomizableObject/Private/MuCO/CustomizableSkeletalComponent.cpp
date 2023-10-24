// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"

#include "UObject/UObjectGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObject.h"


void UCustomizableSkeletalComponent::CreateCustomizableObjectInstanceUsage()
{
	if (!CustomizableObjectInstanceUsage && !HasAnyFlags(RF_ClassDefaultObject))
	{
		CustomizableObjectInstanceUsage = NewObject<UCustomizableObjectInstanceUsage>(this, NAME_None, RF_Transient);
		CustomizableObjectInstanceUsage->CustomizableSkeletalComponent = this;
	}
}


void UCustomizableSkeletalComponent::Callbacks() const
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->Callbacks();
	}
}


USkeletalMesh* UCustomizableSkeletalComponent::GetSkeletalMesh() const
{
	return CustomizableObjectInstance ? CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex) : nullptr;
}


void UCustomizableSkeletalComponent::SetSkeletalMesh(USkeletalMesh* SkeletalMesh, bool bReinitPose, bool bForceClothReset)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetSkeletalMesh(SkeletalMesh, bReinitPose, bForceClothReset);
	}
}


void UCustomizableSkeletalComponent::SetPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetPhysicsAsset(PhysicsAsset);
	}
}


USkeletalMesh* UCustomizableSkeletalComponent::GetAttachedSkeletalMesh() const
{
	if (CustomizableObjectInstanceUsage)
	{
		return CustomizableObjectInstanceUsage->GetAttachedSkeletalMesh();
	}

	return nullptr;
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsync(false, false);
	}
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsyncResult(Callback, false, false);
	}
}


#if WITH_EDITOR

void UCustomizableSkeletalComponent::EditorUpdateComponent()
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->EditorUpdateComponent();
	}
}
#endif


void UCustomizableSkeletalComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent && CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetPendingSetSkeletalMesh(true);
	}
	else if(!GetAttachParent())
	{
		DestroyComponent();
	}
}


void UCustomizableSkeletalComponent::PostInitProperties()
{
	Super::PostInitProperties();

	CreateCustomizableObjectInstanceUsage();
}

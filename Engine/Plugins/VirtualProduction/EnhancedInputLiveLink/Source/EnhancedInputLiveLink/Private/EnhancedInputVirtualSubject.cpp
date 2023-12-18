// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputVirtualSubject.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#	include "Editor.h"
#	include "EnhancedInputEditorSubsystem.h"
#endif

void UEnhancedInputVirtualSubject::Initialize(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, ILiveLinkClient* InLiveLinkClient)
{
	Super::Initialize(InSubjectKey, Role, LiveLinkClient);

	{
		FEditorScriptExecutionGuard ScriptGuard;
		
		CreateEditorInput();
	}
}



void UEnhancedInputVirtualSubject::CreateEditorInput()
{
	InputComponent = NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), TEXT("LivelinkEnhancedInputComponent0"), RF_Transient);
	UInputDelegateBinding::BindInputDelegates(GetClass(), InputComponent, this);
	
	RegisterInputComponent();
}

void UEnhancedInputVirtualSubject::RemoveEditorInput()
{
	if (InputComponent)
		UnregisterInputComponent();
	{
		InputComponent->DestroyComponent();
	}
	InputComponent = nullptr;

}


void UEnhancedInputVirtualSubject::RegisterInputComponent()
{
	// Ensure we start from a clean slate
	UnregisterInputComponent();
	
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->PushInputComponent(InputComponent);
			bIsInputRegistered = true;
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		if (UEnhancedInputEditorSubsystem* EditorInputSubsystem = GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>())
		{
			EditorInputSubsystem->StartConsumingInput();
			EditorInputSubsystem->PushInputComponent(InputComponent);
			const int32 InPriority = 1;
			UInputMappingContext* NewMappingContext = Cast<UInputMappingContext>(MappingContext.TryLoad());
			if(NewMappingContext == nullptr)
			{
				// early out if there is no Mapping Context reference available
				return;
			}
			
			if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
			{
				if(IsValid(NewMappingContext))
				{
					EnhancedInputSubsystemInterface->AddMappingContext(NewMappingContext, InPriority);
					
				}
			}
			bIsInputRegistered = true;
			
		}
	}
#endif
}


void UEnhancedInputVirtualSubject::UnregisterInputComponent()
{
	// Removes the component from both editor and runtime input systems if possible
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->PopInputComponent(InputComponent);
			
		}
	}
#if WITH_EDITOR
	if (GEditor)
	{
		if (UEnhancedInputEditorSubsystem* EditorInputSubsystem = GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>())
		{
			EditorInputSubsystem->PopInputComponent(InputComponent);
			EditorInputSubsystem->StopConsumingInput();
		}
			
	}
#endif

	bIsInputRegistered = false;
}

IEnhancedInputSubsystemInterface* UEnhancedInputVirtualSubject::GetEnhancedInputSubsystemInterface() const
{
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		if (const ULocalPlayer* FirstLocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(FirstLocalPlayer);
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>();
	}
#endif
	return nullptr;
}



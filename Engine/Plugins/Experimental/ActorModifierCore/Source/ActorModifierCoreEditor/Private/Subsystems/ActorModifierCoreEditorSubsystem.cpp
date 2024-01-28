// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/ActorModifierCoreEditorSubsystem.h"

#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Internationalization/Text.h"
#include "Modifiers/ActorModifierCoreEditorMenu.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Modifiers/Widgets/SActorModifierCoreEditorProfiler.h"
#include "ScopedTransaction.h"
#include "ActorModifierCoreEditorStyle.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"

#define LOCTEXT_NAMESPACE "ActorModifierCoreEditorExtensionSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogActorModifierCoreEditorSubsystem, Log, All);

UActorModifierCoreEditorSubsystem::UActorModifierCoreEditorSubsystem()
	: UEditorSubsystem()
{
}

UActorModifierCoreEditorSubsystem* UActorModifierCoreEditorSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UActorModifierCoreEditorSubsystem>();
	}
	return nullptr;
}

void UActorModifierCoreEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EngineSubsystem = UActorModifierCoreSubsystem::Get();

	check(EngineSubsystem.IsValid());
	
	// Initialize here if not called anywhere to setup ClassIcons
	FActorModifierCoreEditorStyle::Get();
}

void UActorModifierCoreEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UActorModifierCoreEditorSubsystem::EnableModifiers(bool bInEnabled, TSet<UActorModifierCoreBase*>& InModifiers, bool bInShouldTransact) const
{
	if (InModifiers.IsEmpty())
	{
		return false;
	}

	// create transaction
	FText TransactionText;
	if (bInEnabled)
	{
		TransactionText = FText::Format(
			LOCTEXT("EnableModifiers.Enable", "Enabling {0} modifier(s)"),
			FText::FromString(FString::FromInt(InModifiers.Num())));
	}
	else
	{
		TransactionText = FText::Format(
			LOCTEXT("EnableModifiers.Disable", "Disabling {0} modifier(s)"),
			FText::FromString(FString::FromInt(InModifiers.Num())));
	}
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);
	
	// lets group modifiers to batch the operation, better to update the stack only once instead of many times
	while (!InModifiers.IsEmpty())
	{
		TSet<UActorModifierCoreBase*> StackModifiers;

		const UActorModifierCoreStack* CurrentStack = nullptr;
	
		for (UActorModifierCoreBase* Modifier : InModifiers)
		{
			if (!IsValid(Modifier))
			{
				continue;
			}

			UActorModifierCoreStack* ModifierStack = Modifier->GetModifierStack();
			ModifierStack = IsValid(ModifierStack) ? ModifierStack : Modifier->GetRootModifierStack();
			
			if (!CurrentStack)
			{
				CurrentStack = ModifierStack;
			}

			if (ModifierStack == CurrentStack)
			{
				StackModifiers.Add(Modifier);
			}
		}

		{
			// disable stack modifiers update
			FActorModifierCoreScopedLock Lock(StackModifiers);
			for (UActorModifierCoreBase* Modifier : StackModifiers)
			{
				Modifier->SetModifierEnabled(bInEnabled);
			}
		}

		InModifiers = InModifiers.Difference(StackModifiers);
	}

	return true;
}

bool UActorModifierCoreEditorSubsystem::RemoveModifiers(TSet<UActorModifierCoreBase*>& InModifiers, FText* OutFailReason, bool bInShouldTransact) const
{
	if (InModifiers.IsEmpty())
	{
		return false;
	}

	// create transaction
	FText TransactionText;
	const UActorModifierCoreBase* SingleModifier = InModifiers.Array()[0];
	if (InModifiers.Num() == 1 && IsValid(SingleModifier))
	{
		TransactionText = FText::Format(
			LOCTEXT("RemoveSingleModifiers", "Removing {0} modifier"),
			FText::FromName(SingleModifier->GetModifierName()));
	}
	else
	{
		TransactionText = FText::Format(
			LOCTEXT("RemoveMultipleModifiers", "Removing {0} modifier(s)"),
			FText::FromString(FString::FromInt(InModifiers.Num())));
	}
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);

	// lets group modifiers to batch the operation, better to update the stack only once instead of many times
	while (!InModifiers.IsEmpty())
	{
		TSet<UActorModifierCoreBase*> StackModifiers;

		UActorModifierCoreStack* CurrentStack = nullptr;
	
		for (UActorModifierCoreBase* Modifier : InModifiers)
		{
			if (!IsValid(Modifier))
			{
				continue;
			}

			UActorModifierCoreStack* ModifierStack = Modifier->GetModifierStack();
			ModifierStack = IsValid(ModifierStack) ? ModifierStack : Modifier->GetRootModifierStack();
			
			if (!CurrentStack)
			{
				CurrentStack = ModifierStack;
			}

			if (ModifierStack == CurrentStack)
			{
				StackModifiers.Add(Modifier);
			}
		}

		// Sort by their order in stack first to avoid dependencies errors
		bool bSuccess = true;
		StackModifiers.Sort([CurrentStack](UActorModifierCoreBase& A, UActorModifierCoreBase& B)
		{
			bool bSortResult = true; 
			CurrentStack->ProcessFunction([&A, &B, &bSortResult](const UActorModifierCoreBase* InModifier)->bool
			{
				if (InModifier == &A || InModifier == &B)
				{
					bSortResult = InModifier == &A ? false : true;
					return false;
				}
				return true;
			});
			return bSortResult;
		});

		CurrentStack->ProcessLockFunction([this, &StackModifiers, OutFailReason, CurrentStack, &bSuccess]()
		{
			for (UActorModifierCoreBase* RemoveModifier : StackModifiers)
			{
				FActorModifierCoreStackRemoveOp RemoveOp;
				RemoveOp.RemoveModifier = RemoveModifier;
				RemoveOp.FailReason = OutFailReason;
				if (!CurrentStack->RemoveModifier(RemoveOp))
				{
					bSuccess = false;
					break;
				}
			}
		});
		
		if (!bSuccess)
		{
			return false;
		}

		InModifiers = InModifiers.Difference(StackModifiers);
	}

	return true;
}

bool UActorModifierCoreEditorSubsystem::RemoveActorsModifiers(TSet<AActor*>& InActors, bool bInShouldTransact) const
{
	if (InActors.IsEmpty())
	{
		return false;
	}

	// Get actors stack
	TArray<UActorModifierCoreStack*> ActorStacks;
	for (const AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		if (UActorModifierCoreStack* ActorStack = EngineSubsystem->GetActorModifierStack(Actor))
		{
			ActorStacks.Add(ActorStack);
		}
	}

	// create transaction
	const FText TransactionText = FText::Format(
		LOCTEXT("AddModifier", "Remove all modifiers from {0} actor(s)"),
		FText::FromString(FString::FromInt(InActors.Num())));
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);

	// remove modifiers from actors
	for (UActorModifierCoreStack* ActorStack : ActorStacks)
	{
		if (!ActorStack->RemoveAllModifiers())
		{
			return false;
		}
		// remove to indicate it was done
		InActors.Remove(ActorStack->GetModifiedActor());
	}
	
	return true;
}

bool UActorModifierCoreEditorSubsystem::AddActorsModifiers(const FName& InModifierName, TSet<AActor*>& InActors, FText* OutFailReason, bool bInShouldTransact) const
{
	if (!EngineSubsystem.IsValid())
	{
		return false;
	}

	if (!EngineSubsystem->IsRegisteredModifierClass(InModifierName))
	{
		return false;
	}
	
	// Get actors stack, create if none
	TArray<UActorModifierCoreStack*> ActorStacks;
	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		UActorModifierCoreStack* ActorStack = EngineSubsystem->GetActorModifierStack(Actor);
		
		if (!ActorStack)
		{
			ActorStack = EngineSubsystem->AddActorModifierStack(Actor);
		}

		if (IsValid(ActorStack))
		{
			ActorStacks.Add(ActorStack);
		}
	}

	// create transaction
	const FText TransactionText = FText::Format(
		LOCTEXT("AddModifier", "Add {0} modifier on {1} actor(s)"),
		FText::FromName(InModifierName),
		FText::FromString(FString::FromInt(InActors.Num())));
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);

	// add modifier to actors
	FActorModifierCoreStackInsertOp InsertOp;
	InsertOp.NewModifierName = InModifierName;
	InsertOp.FailReason = OutFailReason;
	
	for (UActorModifierCoreStack* ActorStack : ActorStacks)
	{
		if (!ActorStack->InsertModifier(InsertOp))
		{
			return false;
		}
		
		// remove to indicate it was done
		InActors.Remove(ActorStack->GetModifiedActor());
	}
	
	return true;
}

bool UActorModifierCoreEditorSubsystem::InsertModifier(const FName& InModifierName, UActorModifierCoreStack* InStack, UActorModifierCoreBase* InPositionModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason, bool bInShouldTransact) const
{
	if (!EngineSubsystem.IsValid())
	{
		return false;
	}

	if (!EngineSubsystem->IsRegisteredModifierClass(InModifierName))
	{
		return false;
	}

	if (!IsValid(InStack))
	{
		return false;
	}

	// create transaction
	FText TransactionText;
	if (InPosition == EActorModifierCoreStackPosition::Before)
	{
		if (IsValid(InPositionModifier))
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Insert {0} modifier before {1}"),
				FText::FromName(InModifierName),
				FText::FromName(InPositionModifier->GetModifierName()));
		}
		else
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Insert {0} modifier at the end of stack"),
				FText::FromName(InModifierName));
		}
	}
	else if (InPosition == EActorModifierCoreStackPosition::After)
	{
		if (IsValid(InPositionModifier))
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Insert {0} modifier after {1}"),
				FText::FromName(InModifierName),
				FText::FromName(InPositionModifier->GetModifierName()));
		}
		else
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Insert {0} modifier at the start of stack"),
				FText::FromName(InModifierName));
		}
	}
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);

	// insert modifier in stack
	FActorModifierCoreStackInsertOp InsertOp;
	InsertOp.NewModifierName = InModifierName;
	InsertOp.InsertPosition = InPosition;
	InsertOp.InsertPositionContext = InPositionModifier;
	InsertOp.FailReason = OutFailReason;
	return InStack->InsertModifier(InsertOp) != nullptr;
}

bool UActorModifierCoreEditorSubsystem::MoveModifier(UActorModifierCoreBase* InMoveModifier, UActorModifierCoreBase* InPositionModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason, bool bInShouldTransact) const
{
	if (!EngineSubsystem.IsValid())
	{
		return false;
	}

	if (!IsValid(InMoveModifier))
	{
		return false;
	}

	UActorModifierCoreStack* ModifierStack = InMoveModifier->GetModifierStack();
	if (!IsValid(ModifierStack))
	{
		return false;
	}
	
	const FName& MoveModifierName = InMoveModifier->GetModifierName();
	
	// create transaction
	FText TransactionText;
	if (InPosition == EActorModifierCoreStackPosition::Before)
	{
		if (IsValid(InPositionModifier))
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Move {0} modifier before {1}"),
				FText::FromName(MoveModifierName),
				FText::FromName(InPositionModifier->GetModifierName()));
		}
		else
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Move {0} modifier at the end of stack"),
				FText::FromName(MoveModifierName));
		}
	}
	else if (InPosition == EActorModifierCoreStackPosition::After)
	{
		if (IsValid(InPositionModifier))
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Move {0} modifier after {1}"),
				FText::FromName(MoveModifierName),
				FText::FromName(InPositionModifier->GetModifierName()));
		}
		else
		{
			TransactionText = FText::Format(
				LOCTEXT("InsertModifier", "Move {0} modifier at the start of stack"),
				FText::FromName(MoveModifierName));
		}
	}
	FScopedTransaction Transaction(TransactionText, bInShouldTransact);

	// move modifier in stack
	FActorModifierCoreStackMoveOp MoveOp;
	MoveOp.MoveModifier = InMoveModifier;
	MoveOp.MovePosition = InPosition;
	MoveOp.MovePositionContext = InPositionModifier;
	MoveOp.FailReason = OutFailReason;
	return ModifierStack->MoveModifier(MoveOp);
}

bool UActorModifierCoreEditorSubsystem::FillModifierMenu(UToolMenu* InMenu, const FActorModifierCoreEditorMenuContext& InContext, const FActorModifierCoreEditorMenuOptions& InMenuOptions) const
{
	if (!InMenu || InContext.IsEmpty())
	{
		return false;
	}
	
	const FActorModifierCoreEditorMenuData MenuData(InContext, InMenuOptions);
	FToolMenuSection* ContextModifiersSection = nullptr;
	if (InMenuOptions.ShouldCreateSubMenu())
	{
		static const FName ModifierSection("ContextModifierActions");
	
		ContextModifiersSection = InMenu->FindSection(ModifierSection);
		if (!ContextModifiersSection)
		{
			ContextModifiersSection = &InMenu->AddSection(ModifierSection
				, LOCTEXT("ContextModifierActions", "Modifiers Actions")
				, FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
		}
	}
	
	if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::Add)
	{
		if (InContext.ContainsAnyActor())
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextAddModifiersActions"),
					LOCTEXT("AddModifiers.Label", "Add Modifiers"),
					LOCTEXT("AddModifiers.Tooltip", "Add modifiers to this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendAddModifierMenu, MenuData));
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendAddModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::Delete)
	{
		if (InContext.ContainsNonEmptyStack() || InContext.ContainsAnyModifier())
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextRemoveModifierActions"),
					LOCTEXT("RemoveModifier.Label", "Remove Modifiers"),
					LOCTEXT("EnableModifier.Tooltip", "Remove modifiers from this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendRemoveModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendRemoveModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::Move)
	{
		if (InContext.ContainsOnlyModifier() && InContext.ContextModifiers.Num() == 1)
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextMoveModifiersActions"),
					LOCTEXT("MoveModifiers.Label", "Move Modifiers"),
					LOCTEXT("MovModifiers.Tooltip", "Move modifiers in the stack"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendMoveModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendMoveModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::Enable)
	{
		if (InContext.ContainsDisabledModifier() || InContext.ContainsDisabledStack())
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextEnableModifiersAction"),
					LOCTEXT("EnableModifiers.Label", "Enable Modifier"),
					LOCTEXT("EnableModifiers.Tooltip", "Enable modifier from this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendEnableModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendEnableModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::Disable)
	{
		if (InContext.ContainsEnabledModifier() || InContext.ContainsEnabledStack())
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextDisableModifiersAction"),
					LOCTEXT("DisableModifiers.Label", "Disable Modifier"),
					LOCTEXT("DisableModifiers.Tooltip", "Disable modifier from this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendEnableModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendEnableModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::InsertBefore)
	{
		if (InContext.ContainsOnlyModifier() && InContext.ContextModifiers.Num() == 1)
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextInsertBeforeModifiersActions"),
					LOCTEXT("InsertBeforeModifiers.Label", "Insert Modifiers Before"),
					LOCTEXT("InsertBeforeModifiers.Tooltip", "Insert modifiers before this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendInsertModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendInsertModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}
	else if (InMenuOptions.GetMenuType() == EActorModifierCoreEditorMenuType::InsertAfter)
	{
		if (InContext.ContainsOnlyModifier() && InContext.ContextModifiers.Num() == 1)
		{
			if (InMenuOptions.ShouldCreateSubMenu())
			{
				ContextModifiersSection->AddSubMenu(
					TEXT("ContextInsertAfterModifiersActions"),
					LOCTEXT("InsertAfterModifiers.Label", "Insert Modifiers After"),
					LOCTEXT("InsertAfterModifiers.Tooltip", "Insert modifiers after this selection"),
					FNewToolMenuDelegate::CreateLambda(&UE::ActorModifierCoreEditor::OnExtendInsertModifierMenu, MenuData)
				);
			}
			else
			{
				UE::ActorModifierCoreEditor::OnExtendInsertModifierMenu(InMenu, MenuData);
			}
			return true;
		}
	}

	return false;
}

void UActorModifierCoreEditorSubsystem::GetSortedModifiers(const TSet<UActorModifierCoreBase*>& InModifiers, AActor* InTargetActor, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, TArray<UActorModifierCoreBase*>& OutMoveModifiers, TArray<UActorModifierCoreBase*>& OutCloneModifiers) const
{
	const UActorModifierCoreSubsystem* const ModifierSubsystem = UActorModifierCoreSubsystem::Get();
	
	if (!IsValid(ModifierSubsystem) || InModifiers.IsEmpty() || !IsValid(InTargetActor))
	{
		return;
	}

	for (UActorModifierCoreBase* Modifier : InModifiers)
	{
		if (!IsValid(Modifier))
		{
			continue;	
		}

		// it's a clone operation if target actor is different as modifier actor
		if (Modifier->GetModifiedActor() != InTargetActor)
		{
			OutCloneModifiers.Add(Modifier);
		}
		// it's a move operation if target actor is same as modifier actor
		else
		{
			OutMoveModifiers.Add(Modifier);
		}
	}
	
	// Remove unsupported modifiers when inserting where target modifier is nullptr
	if (!InTargetModifier)
	{
		const TSet<FName> AllowedModifiers = ModifierSubsystem->GetAllowedModifiers(InTargetActor, InTargetModifier, InPosition);
		
		OutMoveModifiers.RemoveAll([&AllowedModifiers](UActorModifierCoreBase* InModifier)
		{
			return !IsValid(InModifier) || !AllowedModifiers.Contains(InModifier->GetModifierName());
		});

		OutCloneModifiers.RemoveAll([&AllowedModifiers](UActorModifierCoreBase* InModifier)
		{
			return !IsValid(InModifier) || !AllowedModifiers.Contains(InModifier->GetModifierName());
		});
	}

	// Sort them by dependency and current order
	OutMoveModifiers.Sort([InTargetModifier](const UActorModifierCoreBase& A, const UActorModifierCoreBase& B)
	{
		const FActorModifierCoreMetadata& MetadataA = A.GetModifierMetadata();
		const FActorModifierCoreMetadata& MetadataB = B.GetModifierMetadata();
		const bool bADependsOnB = MetadataA.DependsOn(MetadataB.GetName());

		const UActorModifierCoreStack* StackA = A.GetModifierStack();
		const UActorModifierCoreStack* StackB = B.GetModifierStack();
		const bool bAIsBeforeB = StackA == StackB && StackA->ContainsModifierBefore(&A, &B);

		bool bSortOrder = bAIsBeforeB && !bADependsOnB;
		
		const UActorModifierCoreBase* DependencyModifier = bADependsOnB ? &A : &B;
		if (StackA->ContainsModifierAfter(InTargetModifier, DependencyModifier))
		{
			bSortOrder = !bSortOrder;
		}
			
		return bSortOrder;
	});

	OutCloneModifiers.Sort([](const UActorModifierCoreBase& A, const UActorModifierCoreBase& B)
	{
		const FActorModifierCoreMetadata& MetadataA = A.GetModifierMetadata();
		const FActorModifierCoreMetadata& MetadataB = B.GetModifierMetadata();
		const bool bADependsOnB = MetadataA.DependsOn(MetadataB.GetName());

		const UActorModifierCoreStack* StackA = A.GetModifierStack();
		const UActorModifierCoreStack* StackB = B.GetModifierStack();
		const bool bAIsBeforeB = StackA == StackB && StackA->ContainsModifierBefore(&A, &B);

		const bool bSortOrder = bAIsBeforeB && !bADependsOnB;

		return bSortOrder;
	});
}

bool UActorModifierCoreEditorSubsystem::MoveModifiers(const TArray<UActorModifierCoreBase*>& InModifiers, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason, bool bInShouldTransact) const
{
	if (!InTargetModifier)
	{
		return false;
	}
	
	const UActorModifierCoreSubsystem* const ModifierSubsystem = UActorModifierCoreSubsystem::Get();
	UActorModifierCoreStack* const TargetStack = InTargetModifier->GetModifierStack();

	if (!IsValid(ModifierSubsystem) || !IsValid(TargetStack) || InModifiers.IsEmpty())
	{
		return false;
	}

	static const FText TransactionText = LOCTEXT("MoveModifiers", "Moving {0} modifier(s) {1} modifier {2}");
	FScopedTransaction Transaction(
		FText::Format(TransactionText
			, FText::FromString(FString::FromInt(InModifiers.Num()))
			, FText::FromString(InPosition == EActorModifierCoreStackPosition::After ? TEXT("after") : TEXT("before"))
			, FText::FromName(InTargetModifier->GetModifierName()))
		, bInShouldTransact);
	
	uint32 EditModifierCount = 0;

	TargetStack->ProcessLockFunction([TargetStack, &InModifiers, OutFailReason, InPosition, InTargetModifier, &EditModifierCount]()
	{
		UActorModifierCoreBase* OperationContext = nullptr;
		
		for (UActorModifierCoreBase* Modifier : InModifiers)
		{
			if (IsValid(Modifier))
			{
				FActorModifierCoreStackMoveOp MoveOp;
				MoveOp.FailReason = OutFailReason;
				MoveOp.MovePosition = InPosition;
				MoveOp.MovePositionContext = InTargetModifier;
				MoveOp.MoveModifier = Modifier;

				// Eg 1: when moving [bend, subdivide] before target modifier X : X is after them in the stack, we want bend added before X and subdivide added before Bend
				// Eg 2: when moving [subdivide, bend] after target modifier X : X is before them in the stack, we want subdivide after X and bend after subdivide
				if (OperationContext &&
					((MoveOp.MovePosition == EActorModifierCoreStackPosition::After && TargetStack->ContainsModifierAfter(Modifier, InTargetModifier)) ||
					(MoveOp.MovePosition == EActorModifierCoreStackPosition::Before && TargetStack->ContainsModifierBefore(Modifier, InTargetModifier))))
				{
					MoveOp.MovePositionContext = OperationContext;
				}
				
				if (TargetStack->MoveModifier(MoveOp))
				{
					++EditModifierCount;
					OperationContext = Modifier;
				}
				else
				{
					const AActor* TargetActor = InTargetModifier->GetModifiedActor();
					const FText& ErrorText = MoveOp.FailReason ? *MoveOp.FailReason : FText::GetEmpty();
					
					// Move modifiers on actor failed
					UE_LOG(LogActorModifierCoreEditorSubsystem, Warning, TEXT("Move modifier %s on actor %s failed : %s"),
							*MoveOp.MoveModifier->GetModifierName().ToString(),
							*TargetActor->GetActorNameOrLabel(),
							*ErrorText.ToString());
					break;
				}
			}
		}
	});

	return EditModifierCount > 0;
}

bool UActorModifierCoreEditorSubsystem::CloneModifiers(const TArray<UActorModifierCoreBase*>& InModifiers, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason, bool bInShouldTransact) const
{
	if (!InTargetModifier)
	{
		return false;
	}
	
	const UActorModifierCoreSubsystem* const ModifierSubsystem = UActorModifierCoreSubsystem::Get();
	UActorModifierCoreStack* const TargetStack = InTargetModifier->GetModifierStack();

	if (!IsValid(ModifierSubsystem) || !IsValid(TargetStack) || InModifiers.IsEmpty())
	{
		return false;
	}

	static const FText TransactionText = LOCTEXT("CloneModifiers", "Cloning {0} modifier(s) {1} modifier {2}");
	FScopedTransaction Transaction(
		FText::Format(TransactionText
			, FText::FromString(FString::FromInt(InModifiers.Num()))
			, FText::FromString(InPosition == EActorModifierCoreStackPosition::After ? TEXT("after") : TEXT("before"))
			, FText::FromName(InTargetModifier->GetModifierName()))
		, bInShouldTransact);
	
	uint32 EditModifierCount = 0;

	TargetStack->ProcessLockFunction([TargetStack, &InModifiers, OutFailReason, InPosition, InTargetModifier, &EditModifierCount]()
	{
		UActorModifierCoreBase* OperationContext = nullptr;
		for (UActorModifierCoreBase* Modifier : InModifiers)
		{
			if (IsValid(Modifier))
			{
				FActorModifierCoreStackCloneOp CloneOp;
				CloneOp.FailReason = OutFailReason;
				CloneOp.ClonePosition = InPosition;
				CloneOp.ClonePositionContext = InTargetModifier;
				CloneOp.CloneModifier = Modifier;

				// When inserting A, B where B depends on A after C, clone A after C then clone B after A
				if (OperationContext && CloneOp.ClonePosition == EActorModifierCoreStackPosition::After)
				{
					CloneOp.ClonePositionContext = OperationContext;
				}
				
				if (UActorModifierCoreBase* NewClonedModifier = TargetStack->CloneModifier(CloneOp))
				{
					++EditModifierCount;
					OperationContext = NewClonedModifier;
				}
				else
				{
					const AActor* TargetActor = InTargetModifier->GetModifiedActor();
					const FText& ErrorText = CloneOp.FailReason ? *CloneOp.FailReason : FText::GetEmpty();
					
					// Clone modifiers on actor failed
					UE_LOG(LogActorModifierCoreEditorSubsystem, Warning, TEXT("Clone modifier %s on actor %s failed : %s"),
							*CloneOp.CloneModifier->GetModifierName().ToString(),
							*TargetActor->GetActorNameOrLabel(),
							*ErrorText.ToString());
					break;
				}
			}
		}
	});

	return EditModifierCount > 0;
}

TSharedPtr<SActorModifierCoreEditorProfiler> UActorModifierCoreEditorSubsystem::CreateProfilerWidget(TSharedPtr<FActorModifierCoreProfiler> InProfiler)
{
	if (!InProfiler.IsValid())
	{
		return nullptr;
	}

	if (const TFunction<TSharedRef<SActorModifierCoreEditorProfiler>(TSharedPtr<FActorModifierCoreProfiler>)>* WidgetFunction = ModifierProfilerWidgets.Find(InProfiler->GetProfilerType()))
	{
		return (*WidgetFunction)(InProfiler);
	}

	return SNew(SActorModifierCoreEditorProfiler, InProfiler);
}

#undef LOCTEXT_NAMESPACE

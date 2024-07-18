// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/ActorModifierCoreDefs.h"
#include "ActorModifierCoreLibraryDefs.generated.h"

class UActorModifierCoreBase;

UENUM(BlueprintType)
enum class EActorModifierCoreSearchMode : uint8
{
	/** True when one condition is met */
	Or,
	/** True when all condition are met */
	And
};

USTRUCT(BlueprintType)
struct FActorModifierCoreInsertOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TSubclassOf<UActorModifierCoreBase> ModifierClass;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	EActorModifierCoreStackPosition InsertPosition = EActorModifierCoreStackPosition::Before;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> InsertPositionContext = nullptr;
};

USTRUCT(BlueprintType)
struct FActorModifierCoreCloneOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> CloneModifier = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	EActorModifierCoreStackPosition ClonePosition = EActorModifierCoreStackPosition::Before;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> ClonePositionContext = nullptr;
};

USTRUCT(BlueprintType)
struct FActorModifierCoreMoveOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> MoveModifier = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	EActorModifierCoreStackPosition MovePosition = EActorModifierCoreStackPosition::Before;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> MovePositionContext = nullptr;
};

USTRUCT(BlueprintType)
struct FActorModifierCoreRemoveOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TObjectPtr<UActorModifierCoreBase> RemoveModifier = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	bool bRemoveDependencies = false;
};

USTRUCT(BlueprintType)
struct FActorModifierCoreSearchOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	EActorModifierCoreSearchMode SearchMode = EActorModifierCoreSearchMode::Or;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TSet<TSubclassOf<UActorModifierCoreBase>> ModifierClasses;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TSet<FName> ModifierNames;

	UPROPERTY(BlueprintReadWrite, Category="Motion Design|Modifiers|Utility")
	TSet<TObjectPtr<UActorModifierCoreBase>> Modifiers;
};
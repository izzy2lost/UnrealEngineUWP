// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Modifiers/ActorModifierCoreBase.h"
#include "Modifiers/ActorModifierCoreDefs.h"
#include "Modifiers/ActorModifierCoreEditorMenuDefs.h"
#include "Modifiers/Widgets/SActorModifierCoreEditorProfiler.h"
#include "ToolMenus.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "ActorModifierCoreEditorSubsystem.generated.h"

class AActor;
class FActorModifierCoreProfiler;
class UActorModifierCoreBase;
class UActorModifierCoreStack;
class UActorModifierCoreSubsystem;

/** Singleton class that handles editor operations for modifiers */
UCLASS()
class ACTORMODIFIERCOREEDITOR_API UActorModifierCoreEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	friend class SActorModifierCoreEditorProfiler;
	
public:
	UActorModifierCoreEditorSubsystem();
	
	static UActorModifierCoreEditorSubsystem* Get();
	
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Enable or disable modifiers, will update the original array and perform a transaction if wanted */
	bool EnableModifiers(bool bInEnabled, TSet<UActorModifierCoreBase*>& InModifiers, bool bInShouldTransact = true) const;
	
	/** Remove modifiers from different actors or stacks, will update the original array and perform a transaction if wanted */
	bool RemoveModifiers(TSet<UActorModifierCoreBase*>& InModifiers, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Remove all modifiers from actors, will update the original array and perform a transaction if wanted */
	bool RemoveActorsModifiers(TSet<AActor*>& InActors, bool bInShouldTransact = true) const;

	/** Add a modifier to actors, adds a stack automatically if none is found, will update the original array and perform a transaction if wanted */
	bool AddActorsModifiers(const FName& InModifierName, TSet<AActor*>& InActors, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Insert a modifier in a stack before or after another modifier, will perform a transaction if wanted */
	bool InsertModifier(const FName& InModifierName, UActorModifierCoreStack* InStack, UActorModifierCoreBase* InPositionModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Moves a modifier in a stack before or after another modifier, will perform a transaction if wanted */
	bool MoveModifier(UActorModifierCoreBase* InMoveModifier, UActorModifierCoreBase* InPositionModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Fills a menu based on context objects and menu options, will perform menu action transaction if wanted, returns true if the menu has been modified */
	bool FillModifierMenu(UToolMenu* InMenu, const FActorModifierCoreEditorMenuContext& InContext, const FActorModifierCoreEditorMenuOptions& InMenuOptions) const;

	/** Get order dependent modifiers in the correct order for stack operations */
	void GetSortedModifiers(const TSet<UActorModifierCoreBase*>& InModifiers, AActor* InTargetActor, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, TArray<UActorModifierCoreBase*>& OutMoveModifiers, TArray<UActorModifierCoreBase*>& OutCloneModifiers) const;

	/** Move modifiers to a specific target modifier position, will perform menu action transaction if wanted */
	bool MoveModifiers(const TArray<UActorModifierCoreBase*>& InModifiers, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Clone modifiers to a specific target modifier position, will perform menu action transaction if wanted */
	bool CloneModifiers(const TArray<UActorModifierCoreBase*>& InModifiers, UActorModifierCoreBase* InTargetModifier, EActorModifierCoreStackPosition InPosition, FText* OutFailReason = nullptr, bool bInShouldTransact = true) const;

	/** Register a profiler widget based on a modifier profiler class */
	template<typename InProfilerClass, typename = typename TEnableIf<TIsDerivedFrom<InProfilerClass, FActorModifierCoreProfiler>::Value>::Type
		, typename InWidgetClass, typename = typename TEnableIf<TIsDerivedFrom<InWidgetClass, SActorModifierCoreEditorProfiler>::Value>::Type>
	void RegisterProfilerWidget()
	{
		const FName ProfilerType = GetGeneratedTypeName<InProfilerClass>();
		ModifierProfilerWidgets.Add(ProfilerType, [](TSharedPtr<FActorModifierCoreProfiler> InProfiler)
		{
			return SNew(InWidgetClass, InProfiler);
		});
	}

	/** Unregister a modifier profiler widget */
	template<typename InProfilerClass, typename = typename TEnableIf<TIsDerivedFrom<InProfilerClass, FActorModifierCoreProfiler>::Value>::Type>
	void UnregisterProfilerWidget()
	{
		const FName ProfilerType = GetGeneratedTypeName<InProfilerClass>();
		ModifierProfilerWidgets.Remove(ProfilerType);
	}

	/** Creates a profiler widget for a modifier profiler, returns default widget if none was registered */
	TSharedPtr<SActorModifierCoreEditorProfiler> CreateProfilerWidget(TSharedPtr<FActorModifierCoreProfiler> InProfiler);
	
protected:
	/** Modifier profiler pinned stats based on profiler type */
	TMap<FName, TSet<FName>> ModifierProfilerStats;

	/** Modifier profiler widgets */
	TMap<FName, TFunction<TSharedRef<SActorModifierCoreEditorProfiler>(TSharedPtr<FActorModifierCoreProfiler>)>> ModifierProfilerWidgets;
	
	/** Runtime subsystem for modifier factories */
	TWeakObjectPtr<UActorModifierCoreSubsystem> EngineSubsystem = nullptr;
};

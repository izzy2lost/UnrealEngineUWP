// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSceneTree.h"
#include "AvaSequenceShared.h"
#include "Data/AvaWorldData.h"
#include "Engine/Blueprint.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "Types/SlateEnums.h"
#include "Viewport/AvaViewportQualitySettings.h"
#include "AvaBlueprint.generated.h"

class AAvaActor;
class IAvaSequencePlaybackObject;
class ISequencer;
class UAvaSequence;
class UAvaSequencePlayer;
class URemoteControlPreset;
class UTexture2D;

#if WITH_EDITOR
struct FAvaViewportGuideInfo_Deprecated;
#endif

UCLASS(BlueprintType, DisplayName = "Motion Design Blueprint")
class AVALANCHE_API UAvalancheBlueprint : public UBlueprint, public IAvaSequenceProvider, public IAvaSceneInterface
{
	GENERATED_BODY()

public:
	UAvalancheBlueprint();

	static FTopLevelAssetPath GetWorldContextPath(const FSoftObjectPath& InSourceAssetPath);

#if WITH_EDITORONLY_DATA
	/** Called on Pre Save to Notify that we have Updated the Avalanche World for Save. */
	FSimpleMulticastDelegate OnAvalanchePreSave;

	/** Called on Post Save to Notify that we have Saved the Avalanche World. */
	FSimpleMulticastDelegate OnAvalanchePostSave;

	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;

	const TArray<uint8>& GetEditorData() const { return EditorData; }
	TArray<uint8>& GetEditorDataMutable() { return EditorData; }

	void SetCreateDefaultScene(bool bInCreateDefaultScene) { bCreateDefaultScene = bInCreateDefaultScene; }
	bool ShouldCreateDefaultScene() const { return bCreateDefaultScene; }
#endif

	void SetAvalancheWorld(UWorld* InWorld);

	UWorld* GetAvalancheWorld() const;

	AAvaActor* GetPlaceholderActor() const;
	IAvaSequencePlaybackObject* GetScenePlayback() const;

	void DestroyPlaceholderActor();
	
	void UpdatePlaceholderActor(bool bInForceFullUpdate = false);

	/**
	 * Saves the Avalanche World State into the UAvalancheBlueprint::WorldData property, does NOT serialize to disk
	 * Should be used prior to actions like duplicating or copying the UObject to get the latest world state duplicated/copied over
	 */
	void SaveAvalancheWorld();

	using FInitActorFunctionRef = TFunctionRef<void(AActor&, const FAvaActorData&)>;
	void LoadAvalancheWorld(UWorld* InOverrideWorld = nullptr, TOptional<FInitActorFunctionRef> InInitActorFunction = {});

	bool IsSavingAvalancheWorld() const { return bSavingWorld; }
	bool IsLoadingAvalancheWorld() const { return bLoadingWorld; }

	void RegisterRemoteControlPreset();
	void UnregisterRemoteControlPreset();

	const FName& GetStartupCameraName() const { return StartupCameraName; }
	void SetStartupCameraName(FName InCameraName);

#if WITH_EDITOR
	//~ Begin UBlueprint
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual UClass* GetBlueprintClass() const override;
	virtual void GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses, TSet<const UClass*>& DisallowedChildrenOfClasses) const override;
	//~ End UBlueprint
#endif

	//~ Begin UObject
	virtual void BeginDestroy() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& InDuplicationParameters) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;
	//~ End UObject

	//~ Begin IAvaSceneInterface
	virtual ULevel* GetSceneLevel() const override;
	virtual UAvaSceneSettings* GetSceneSettings() const { return nullptr; }
	virtual UAvaSceneState* GetSceneState() const override { return nullptr; }
	virtual FAvaSceneTree& GetSceneTree() override { return SceneTree; }
	virtual const FAvaSceneTree& GetSceneTree() const override { return SceneTree; }
	virtual IAvaSequencePlaybackObject* GetPlaybackObject() const override;
	virtual IAvaSequenceProvider* GetSequenceProvider() override { return this; }
	virtual const IAvaSequenceProvider* GetSequenceProvider() const override { return this; }
	virtual URemoteControlPreset* GetRemoteControlPreset() const override { return RemoteControlPreset; }
	//~ End IAvaSceneInterface

	//~ Begin IAvaSequenceProvider
	virtual UObject* ToUObject() override;
	virtual UWorld* GetContextWorld() const override;
	virtual bool CreateDirectorInstance(UAvaSequence& InSequence, IMovieScenePlayer& InPlayer, const FMovieSceneSequenceID& InSequenceID, UObject*& OutDirectorInstance) override;
	virtual bool AddSequence(UAvaSequence* InSequence) override;
	virtual void RemoveSequence(UAvaSequence* InSequence) override;
	virtual void SetDefaultSequence(UAvaSequence* InSequence) override;
	virtual UAvaSequence* GetDefaultSequence() const override;
	virtual const TArray<TObjectPtr<UAvaSequence>>& GetSequences() const override { return Animations; }
	virtual const TArray<TWeakObjectPtr<UAvaSequence>>& GetRootSequences() const override { return RootAnimations; }
	virtual TArray<TWeakObjectPtr<UAvaSequence>>& GetRootSequencesMutable() override { return RootAnimations; }
	virtual FName GetSequenceProviderDebugName() const override;
#if WITH_EDITOR
	virtual void OnEditorSequencerCreated(const TSharedPtr<ISequencer>& InSequencer) override;
	virtual TSharedPtr<ISequencer> GetEditorSequencer() const override;
	virtual bool GetDirectorBlueprint(UAvaSequence& InSequence, UBlueprint*& OutBlueprint) override;
#endif
	virtual void RebuildSequenceTree() override;
	virtual void ScheduleRebuildSequenceTree() override;
	virtual FSimpleMulticastDelegate& GetOnSequenceTreeRebuilt() override { return OnAnimationsUpdated; }
	//~ End IAvaAnimationManager

	virtual UWorld* GetWorld() const override;

	UTexture2D* GetThumbnailImage() const { return ThumbnailImage; };
	void SetThumbnailImage(UTexture2D* InTexture) { ThumbnailImage = InTexture; }

	FAvaViewportQualitySettings GetViewportQualitySettings() const { return ViewportQualitySettings; }
	void SetViewportQualitySettings(const FAvaViewportQualitySettings InSettings) { ViewportQualitySettings = InSettings; }

#if WITH_EDITOR
	const TArray<FAvaViewportGuideInfo_Deprecated>& GetGuideInfos() const { return GuideInfos; }
	void SetGuideOffset(int32 InGuideIdx, float InGuideOffset);
	void SetGuides(const TArray<FAvaViewportGuideInfo_Deprecated>& InNewGuideInfos);
#endif

private:
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<UWorld> World;

	/**
	 * Placeholder Actor to use when the Blueprint is used for other means than to have it spawned on a different Level.
	 * You can think of this as the Preview Actor of BlueprintEditor, but available at runtime and for other use cases beyond BP Editor
	 * Examples include the Blueprint Editor itself, Playback and the Avalanche UMG Widget.
	 * Placeholder should be null when an BP Avalanche Actor is spawned into the World as the Actor to use should be the spawned one instead of this Placeholder
	 */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<AAvaActor> PlaceholderActor;

	/** The Base Playback Scene that is always present to Play Animations */
	UPROPERTY()
	TScriptInterface<IAvaSequencePlaybackObject> PlaybackObject;

	UPROPERTY()
	FAvaWorldData WorldData;

	UPROPERTY()
	FAvaSceneTree SceneTree;
	
	UPROPERTY()
	TObjectPtr<UTexture2D> ThumbnailImage;

	/** A List of All Animations, including those that are nested in other Animations */
	UPROPERTY()
	TArray<TObjectPtr<UAvaSequence>> Animations;

	/** The index to the animation to use as the default animation */
	UPROPERTY()
	int32 DefaultAnimationIndex = 0;

	/** A list of only the Root Animations (those without Parent Animations) */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<TWeakObjectPtr<UAvaSequence>> RootAnimations;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Motion Design")
	TObjectPtr<URemoteControlPreset> RemoteControlPreset;

	UPROPERTY()
	FName StartupCameraName = NAME_None;

#if WITH_EDITOR
	TWeakPtr<ISequencer> EditorSequencer;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<uint8> EditorData;

	UPROPERTY()
	bool bCreateDefaultScene = false;
#endif

	/** Only the flags specified in these settings will be applied to the viewport client ShowFlags. */
	UPROPERTY()
	FAvaViewportQualitySettings ViewportQualitySettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FAvaViewportGuideInfo_Deprecated> GuideInfos;
#endif

	FSimpleMulticastDelegate OnAnimationsUpdated;

	bool bLoadingWorld = false;
	bool bSavingWorld = false;
	bool bStoppingAllAnimations = false;
	bool bPendingAnimTreeUpdate = false;
};

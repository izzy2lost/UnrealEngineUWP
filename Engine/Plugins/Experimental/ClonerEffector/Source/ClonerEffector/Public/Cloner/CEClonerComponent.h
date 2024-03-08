// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerEffectorShared.h"
#include "Layouts/CEClonerLayoutBase.h"
#include "NiagaraComponent.h"
#include "CEClonerComponent.generated.h"

class UCEClonerLayoutBase;
class UMaterialInterface;
struct FNiagaraMeshMaterialOverride;

UCLASS(MinimalAPI, Within=CEClonerActor, HideCategories=(Niagara))
class UCEClonerComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:
	/** Called when meshes have been updated */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClonerMeshUpdated, UCEClonerComponent* /** ClonerComponent */)
	static FOnClonerMeshUpdated OnClonerMeshUpdated;

	/** Called when cloner layout system is loaded */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClonerSystemLoaded, UCEClonerComponent* /** ClonerComponent */, UCEClonerLayoutBase* /** InLayout */)
	static FOnClonerSystemLoaded OnClonerSystemLoaded;

	/** Only materials transient or part of the content folder can be dirtied, engine or plugins cannot */
	static bool IsMaterialDirtyable(const UMaterialInterface* InMaterial);

	/** Check if material has niagara usage flag set */
	static bool IsMaterialUsageFlagSet(const UMaterialInterface* InMaterial);

#if WITH_EDITOR
	/** Show material warning notification when missing niagara usage flag */
	static void ShowMaterialWarning(int32 InMaterialCount);
#endif

	UCEClonerComponent();

	/**
	 * Triggers an update of the attachment tree to detect updated items
	 * If reset is true, clears the attachment tree and rebuilds it otherwise diff update
	 */
	void UpdateClonerAttachmentTree(bool bInReset = false);

	/** Called to trigger an update of cloner rendering state if tree */
	void UpdateClonerRenderState();

	/** Sets the layout to use for this cloner simulation */
	bool SetClonerActiveLayout(UCEClonerLayoutBase* InLayout);

	UCEClonerLayoutBase* GetClonerActiveLayout() const
	{
		return ActiveLayout;
	}

	/** Forces a refresh of the active system parameters in niagara store */
	void RefreshUserParameters() const;

	void SetUseOverrideMeshesMaterial(bool bInOverride);

	void SetOverrideMeshesMaterial(UMaterialInterface* InMaterial);

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
	//~ End UObject

	void OnRenderStateDirty(UActorComponent& InComponent);

	void UpdateAttachmentTree();
	void UpdateActorAttachment(AActor* InActor, AActor* InParent);

	void BindActorDelegates(AActor* InActor);
	void UnbindActorDelegates(AActor* InActor) const;

	void OnTransformUpdated(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport);

	UFUNCTION()
	void OnActorDestroyed(AActor* InDestroyedActor);

#if WITH_EDITOR
	void OnActorPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
#endif

	/** Called when a material has changed in any cloned actors */
	void OnMaterialChanged(UObject* InObject);

	/** Called when a mesh has been updated and cloner needs to update instances */
	void OnMeshChanged(UStaticMeshComponent*, AActor* InActor);

	/** Get cloner direct children in the correct order */
	void GetOrderedRootActors(TArray<AActor*>& OutActors) const;

	/** Get the root cloner actor (direct child) for a specific actor */
	AActor* GetRootActor(AActor* InActor) const;

	/** Resets the root baked static mesh to be regenerated later */
	void InvalidateBakedStaticMesh(AActor* InActor);

	/** Runs async update to rebuild dirty meshes */
	void UpdateDirtyMeshesAsync();
	void OnDirtyMeshesUpdated(bool bInSuccess);

	/** Merges all primitive components from an actor to dynamic mesh, does not recurse */
	void UpdateActorBakedDynamicMesh(AActor* InActor);

	/** Merges all baked dynamic meshes from children and self into one static mesh to use as niagara mesh input */
	void UpdateRootActorBakedStaticMesh(AActor* InRootActor);

	/** Gets all attachment items based on an actor, will recurse */
	void GetActorAttachmentItems(AActor* InActor, TArray<FCEClonerAttachmentItem*>& OutAttachmentItems);

	/** Update niagara asset static meshes */
	void UpdateClonerMeshes();

	/** Checks that all root static meshes are valid */
	bool IsAllMergedMeshesValid() const;

	/** Called when material override properties are changed */
	void OnOverrideMeshesMaterialChanged();

	/** Get the niagara materials override if enabled */
	TArray<FNiagaraMeshMaterialOverride> GetOverrideMeshesMaterials() const;

	/** Attachment tree view */
	UPROPERTY(Transient, NonTransactional)
	FCEClonerAttachmentTree ClonerTree;

	/** Current active layout */
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UCEClonerLayoutBase> ActiveLayout;

	/** Attachment items that are dirty and need an update */
	TSet<FCEClonerAttachmentItem*> DirtyItemAttachments;

	/** Override meshes material */
	TWeakObjectPtr<UMaterialInterface> ClonerMeshesOverrideMaterial;

	/** Use override meshes material */
	bool bUseClonerMeshesOverrideMaterial = false;

	/** Asset meshes needs update */
	bool bClonerMeshesDirty = true;

	/** State of the baked dynamic and static mesh creation */
	bool bClonerMeshesUpdating = false;
};
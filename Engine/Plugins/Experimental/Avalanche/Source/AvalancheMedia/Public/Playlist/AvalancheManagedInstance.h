// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Playback/AvalancheRemoteControlValues.h"
#include "UObject/GCObject.h"
#include "UObject/SoftObjectPath.h"

class FAvalancheManagedInstanceCache;
class IAvaSceneInterface;

class AVALANCHEMEDIA_API FAvalancheManagedInstance : public IAvaTypeCastable, public FGCObject, public TSharedFromThis<FAvalancheManagedInstance>
{
public:
	UE_AVA_INHERITS(FAvalancheManagedInstance, IAvaTypeCastable);
	
	FAvalancheManagedInstance(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath);
	virtual ~FAvalancheManagedInstance() override;

	/** Returns true if the instanced asset is valid. */
	virtual bool IsValid() const { return false; }
	
	/** Get the Managed Avalanche Asset's (top level) Remote Control Preset. */
	virtual URemoteControlPreset* GetRemoteControlPreset() const { return nullptr; }
	
	/** Access a copy of the Remote Control values from the source (a.k.a. template). */
	const FAvalancheRemoteControlValues& GetDefaultRemoteControlValues() const { return DefaultRemoteControlValues; }

	/** Get the Ava scene interface from the asset. */
	virtual IAvaSceneInterface* GetSceneInterface() const { return nullptr; }

	const FSoftObjectPath& GetSourceAssetPath() const { return SourceAssetPath; }
	
protected:
	void InvalidateFromCache();

	void RegisterSourceRemoteControlPresetDelegates(URemoteControlPreset* InSourceRemoteControlPreset);
	void UnregisterSourceRemoteControlPresetDelegates(URemoteControlPreset* InSourceRemoteControlPreset) const;
	
	void OnSourceRemoteControlEntitiesExposed(URemoteControlPreset* InSourcePreset, const FGuid& InEntityId);
	void OnSourceRemoteControlEntitiesUnexposed(URemoteControlPreset* InSourcePreset, const FGuid& InEntityId);
	void OnSourceRemoteControlEntitiesUpdated(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedEntities);
	void OnSourceRemoteControlExposedPropertiesModified(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedProperties);
	void OnSourceRemoteControlControllerAdded(URemoteControlPreset* InSourcePreset, const FName NewControllerName, const FGuid& InControllerId);
	void OnSourceRemoteControlControllerRemoved(URemoteControlPreset* InSourcePreset, const FGuid& InControllerId);
	void OnSourceRemoteControlControllerRenamed(URemoteControlPreset* InSourcePreset, const FName InOldLabel, const FName InNewLabel);
	void OnSourceRemoteControlControllerModified(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedControllerIds);

protected:
	FAvalancheManagedInstanceCache* ParentCache;
	FSoftObjectPath SourceAssetPath;
	FAvalancheRemoteControlValues DefaultRemoteControlValues;
};
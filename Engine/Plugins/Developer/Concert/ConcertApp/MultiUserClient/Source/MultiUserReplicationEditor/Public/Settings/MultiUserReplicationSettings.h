// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MultiUserReplicationClientProfileAsset.h"
#if WITH_EDITORONLY_DATA
#include "Settings/MultiUserDefaultPropertySelection.h"
#include "Settings/MultiUserDefaultSubobjectSelection.h"
#include "Templates/Function.h"
#endif
#include "MultiUserReplicationSettings.generated.h"

// Architecturally speaking FDefaultReplicationSessionSettings should be in the MultiUserClient module.
// However, we want a single "Multi-User Replication" settings section... so all the data just lives here (for now?).

USTRUCT()
struct FDefaultReplicationSessionSettings
{
	GENERATED_BODY()

	/** Whether to automatically start replicating after successfully joining a concert session. */
	UPROPERTY(EditAnywhere, Config, Category = "Replication")
	bool bAutoJoin = false;
	
	/** The client profile to use when connecting to a session. */
	UPROPERTY(EditAnywhere, Config, Category = "Replication")
	TSoftObjectPtr<UMultiUserReplicationClientProfileAsset> DefaultProfile;
};

/** Asset for users to describe a client to the server. */
UCLASS(Config=MultiUserClient)
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSettings : public UObject
{
	GENERATED_BODY()
public:

	/** Default replication settings to use on this editor. */
	UPROPERTY(EditAnywhere, Config, Category = "Replication")
	FDefaultReplicationSessionSettings DefaultSessionSettings;

#if WITH_EDITORONLY_DATA
	/** Properties you want selected by default when you add a new replicated object in the editor */
	UPROPERTY(EditAnywhere, Config, Category = "Replication|Editor")
	TMap<FSoftClassPath, FMultiUserDefaultPropertySelection> DefaultPropertySelection;

	/**
	 * Subobjects you want selected by default when you add a new replicated object in the editor.
	 * For examples:
	 *  - Whenever you add a StaticMeshActor actor, you its static mesh component to be added automatically, too.
	 *  - Whenever you add a component, you want to add its child components, too.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Replication|Editor")
	TMap<FSoftClassPath, FMultiUserDefaultSubobjectSelection> DefaultSubobjectSelection;
#endif

	static UMultiUserReplicationSettings* Get() { return GetMutableDefault<UMultiUserReplicationSettings>(); }

#if WITH_EDITORONLY_DATA
	/** Reads DefaultPropertySelection and applies any default property selections to Info based on the Class just added. */
	void AddDefaultPropertiesFromSettings(FReplicatedObjectInfo& Info, UClass& Class);

	/** Reads DefaultComponentSelection and calls FurtherObjectsCallback on any further objects that should also be added. */
	void AddAdditionalObjectsFromSettings(UObject& AddedObject, TFunctionRef<void(UObject&)> FurtherObjectsCallback);
#endif
};
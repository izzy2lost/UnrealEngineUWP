// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"

class AActor;
class FExtender;
class FUICommandList;

/**
* The public interface to this module
*/
class INSTANCEDACTORSEDITOR_API FInstancedActorsEditorModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Convert selected actors to Mass Instance Actors (IAMs). */
	void ConvertActorsToIAMsUIAction(const TArray<AActor*> InActors) const;

	/** Convert selected Mass Instance Actors to Actors. */
	void ConvertIAMsToActorsUIAction(const TArray<AActor*> InActors) const;

protected:
	TSharedRef<FExtender> CreateLevelViewportContextMenuExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors);
	void AddLevelViewportMenuExtender();
	void RemoveLevelViewportMenuExtender();

	FDelegateHandle LevelViewportExtenderHandle;
};

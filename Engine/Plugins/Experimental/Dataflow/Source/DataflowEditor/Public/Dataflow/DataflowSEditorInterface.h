// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"

/**
* FDataflowSEditorInterface
* 
*/

class UDataflowBaseContent;
class UDataflowEditor;

class FDataflowSEditorInterface
{
public:
	FDataflowSEditorInterface() {}

	/** Dataflow editor content accessors */
	virtual const TSharedPtr<UE::Dataflow::FEngineContext> GetDataflowContext() const { return TSharedPtr<UE::Dataflow::FEngineContext>(nullptr); };

};


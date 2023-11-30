// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerMode.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Compatibility/TedsCompatibilityUtils.h"

struct FTypedElementOutlinerModeParams
{
	FTypedElementOutlinerModeParams(SSceneOutliner* InSceneOutliner)
		: SceneOutliner(InSceneOutliner)
	{}

	SSceneOutliner* SceneOutliner;

	// TODO: These queries once provided are owned and unregistered by the TEDSOutliner - they should maybe be query descriptions instead
	// to avoid sharing ownership
	TArray<TypedElementDataStorage::QueryHandle> RowHandleQueries;
};

/*
 * TEDS driven Outliner mode where the Outliner is populated using the results of the RowHandleQueries passed in.
 * See CreateGenericTEDSOutliner() for example usage
 * Inherits from ISceneOutlinerMode - which contains all actions that depend on the type of item you are viewing in the Outliner
 */
class TEDSOUTLINER_API FTypedElementOutlinerMode : public ISceneOutlinerMode, public FBaseTEDSOutlinerMode
{
public:
	explicit FTypedElementOutlinerMode(const FTypedElementOutlinerModeParams& InParams);
	virtual ~FTypedElementOutlinerMode();

	/** Rebuild all mode data */
	virtual void Rebuild() override;
	
protected:
	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;

protected:

	TArray<TypedElementDataStorage::QueryHandle> RowHandleQueries;

};
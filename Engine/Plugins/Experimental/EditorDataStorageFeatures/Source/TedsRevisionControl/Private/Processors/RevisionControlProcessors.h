// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"

#include "RevisionControlProcessors.generated.h"

class IEditorDataStorageProvider;

UCLASS()
class URevisionControlDataStorageFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~URevisionControlDataStorageFactory() override = default;

	void RegisterTables(IEditorDataStorageProvider& DataStorage) override;
	void RegisterQueries(IEditorDataStorageProvider& DataStorage) override;

private:
	void RegisterFetchUpdates(IEditorDataStorageProvider& DataStorage);
	void RegisterApplyOverlays(IEditorDataStorageProvider& DataStorage);
	void RegisterRemoveOverlays(IEditorDataStorageProvider& DataStorage);
	UE::Editor::DataStorage::QueryHandle FetchUpdates = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle ApplyNewOverlays = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle ChangeOverlay = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle ApplyOverlaysObjectToSCC = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle RemoveOverlays = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle FlushPackageUpdates = UE::Editor::DataStorage::InvalidQueryHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"

class ISettingsCategory;

class FTedsSettingsManager final : public TSharedFromThis<FTedsSettingsManager>
{
public:

	FTedsSettingsManager();

	const bool IsInitialized() const
	{
		return bIsInitialized;
	}

	void Initialize();
	void Shutdown();

private:

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage);
	void UnregisterQueries(ITypedElementDataStorageInterface& DataStorage);

	void RegisterSettings();
	void UnregisterSettings();

	void RegisterSettingsContainer(const FName& ContainerName);

	void UpdateSettingsCategory(TSharedPtr<ISettingsCategory> SettingsCategory, const FName& ContainerName, const bool bQueryExistingRows = true);

	bool bIsInitialized;
	TypedElementDataStorage::QueryHandle SelectAllSettingsQuery;

};

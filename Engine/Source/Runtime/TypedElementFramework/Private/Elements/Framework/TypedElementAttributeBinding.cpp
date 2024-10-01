// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementAttributeBinding.h"

#include "Elements/Common/EditorDataStorageFeatures.h"

namespace UE::Editor::DataStorage
{
	FAttributeBinder::FAttributeBinder(RowHandle InTargetRow)
		: TargetRow(InTargetRow)
	{
		if (ensureMsgf(AreEditorDataStorageFeaturesEnabled(),
				TEXT("The Editor Data Storage plugin needs to be enabled to use attribute bindings.")))
		{
			DataStorage = GetMutableDataStorageFeature<IEditorDataStorageProvider>(StorageFeatureName);
		}
	}

	FAttributeBinder::FAttributeBinder(
		RowHandle InTargetRow, IEditorDataStorageProvider* InDataStorage)
		: TargetRow(InTargetRow)
		, DataStorage(InDataStorage)
	{
		ensureMsgf(DataStorage, TEXT("The Editor Data Storage plugin needs to be enabled to use attribute bindings."));
	}
} // namespace UE::Editor::DataStorage

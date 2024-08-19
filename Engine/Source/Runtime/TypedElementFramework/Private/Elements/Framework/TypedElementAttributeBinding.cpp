// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementAttributeBinding.h"

#include "Elements/Framework/TypedElementRegistry.h"

namespace UE::EditorDataStorage
{
	FAttributeBinder::FAttributeBinder(TypedElementDataStorage::RowHandle InTargetRow)
		: TargetRow(InTargetRow)
	{
		if (ensureMsgf(UTypedElementRegistry::GetInstance()->AreDataStorageInterfacesSet(), 
				TEXT("The Editor Data Storage plugin needs to be enabled to use attribute bindings.")))
		{
			DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
		}
	}

	FAttributeBinder::FAttributeBinder(
		TypedElementDataStorage::RowHandle InTargetRow, ITypedElementDataStorageInterface* InDataStorage)
		: TargetRow(InTargetRow)
		, DataStorage(InDataStorage)
	{
		ensureMsgf(DataStorage, TEXT("The Editor Data Storage plugin needs to be enabled to use attribute bindings."));
	}
}

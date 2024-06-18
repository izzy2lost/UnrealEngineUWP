// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementAttributeBinding.h"

#include "Elements/Framework/TypedElementRegistry.h"

namespace UE::EditorDataStorage
{
	FAttributeBinder::FAttributeBinder(TypedElementDataStorage::RowHandle InTargetRow)
		: TargetRow(InTargetRow)
	{
		if (ensureMsgf(UTypedElementRegistry::GetInstance()->AreDataStorageInterfacesSet(), 
				TEXT("The TypedElementsDataStorage plugin needs to be enabled to use attribute bindings.")))
		{
			DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
		}
	}
}

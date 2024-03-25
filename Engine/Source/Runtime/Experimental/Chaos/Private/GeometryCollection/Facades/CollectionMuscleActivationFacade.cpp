// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FleshCollection.cpp: FFleshCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionMuscleActivationFacade.h"
#include "GeometryCollection/GeometryCollection.h"

namespace GeometryCollection::Facades
{
	// Attributes
	const FName FMuscleActivationFacade::GroupName("MuscleActivation");
	const FName FMuscleActivationFacade::MuscleActivationElement("MuscleActivationElement");
	const FName FMuscleActivationFacade::OriginInsertionPair("OriginInsertionPair");
	const FName FMuscleActivationFacade::OriginInsertionRestLength("OriginInsertionRestLength");
	const FName FMuscleActivationFacade::FiberDirectionMatrix("FiberDirectionMatrix");


	FMuscleActivationFacade::FMuscleActivationFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, MuscleActivationElementAttribute(InCollection, MuscleActivationElement, GroupName)
		, OriginInsertionPairAttribute(InCollection, OriginInsertionPair, GroupName)
		, OriginInsertionRestLengthAttribute(InCollection, OriginInsertionRestLength, GroupName)
		, FiberDirectionMatrixAttribute(InCollection, FiberDirectionMatrix, GroupName)
	{
		DefineSchema();
	}

	FMuscleActivationFacade::FMuscleActivationFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, MuscleActivationElementAttribute(InCollection, MuscleActivationElement, GroupName)
		, OriginInsertionPairAttribute(InCollection, OriginInsertionPair, GroupName)
		, OriginInsertionRestLengthAttribute(InCollection, OriginInsertionRestLength, GroupName)
		, FiberDirectionMatrixAttribute(InCollection, FiberDirectionMatrix, GroupName)
	{
		
	}

	bool FMuscleActivationFacade::IsValid() const
	{
		return MuscleActivationElementAttribute.IsValid() && OriginInsertionPairAttribute.IsValid() && OriginInsertionRestLengthAttribute.IsValid() && FiberDirectionMatrixAttribute.IsValid();
	}

	void FMuscleActivationFacade::DefineSchema()
	{
		check(!IsConst());
		MuscleActivationElementAttribute.Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent, "Tetrahedral");
		OriginInsertionPairAttribute.Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent, FGeometryCollection::VerticesGroup);
		OriginInsertionRestLengthAttribute.Add();
		FiberDirectionMatrixAttribute.Add();
	}

	int32 FMuscleActivationFacade::AddMuscleActivationData(const FMuscleActivationData& InputData)
	{
		check(!IsConst());
		if (IsValid())
		{
			int32 NewIndex = MuscleActivationElementAttribute.AddElements(1);
			MuscleActivationElementAttribute.Modify()[NewIndex] = InputData.MuscleActivationElement;
			OriginInsertionPairAttribute.Modify()[NewIndex] = InputData.OriginInsertionPair;
			OriginInsertionRestLengthAttribute.Modify()[NewIndex] = InputData.OriginInsertionRestLength;
			FiberDirectionMatrixAttribute.Modify()[NewIndex] = InputData.FiberDirectionMatrix;
			return NewIndex;
		}
		return INDEX_NONE;
	}

	const FMuscleActivationData FMuscleActivationFacade::GetMuscleActivationData(const int32 DataIndex) const
	{
		FMuscleActivationData ReturnData;
		if (IsValid())
		{
			if (NumMuscles() > DataIndex && DataIndex > -1)
			{
				ReturnData.MuscleActivationElement = MuscleActivationElementAttribute.Get()[DataIndex];
				ReturnData.OriginInsertionPair = OriginInsertionPairAttribute.Get()[DataIndex];
				ReturnData.OriginInsertionRestLength = OriginInsertionRestLengthAttribute.Get()[DataIndex];
				ReturnData.FiberDirectionMatrix = FiberDirectionMatrixAttribute.Get()[DataIndex];
			}
		}
		return ReturnData;
	}
}

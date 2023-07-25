// Copyright Epic Games, Inc. All Rights Reserved.
#include "RigidBodyNameRecords.h"

//======================================================================================================================
void AddUniqueNamesToSet(const TArray<FName>& Names, const FName SetName, TMap<FName, TArray<FName>>& SetCollection)
{
	TArray<FName>& SetElementNames = SetCollection.FindOrAdd(SetName);

	for (const FName Name : Names)
	{
		SetElementNames.AddUnique(Name);
	}
}

//======================================================================================================================
void FRigidBodyNameRecords::AddControl(FName Name, FName SetName)
{
	ControlSets.FindOrAdd(SetName).AddUnique(Name);
	ControlSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FRigidBodyNameRecords::AddControl(FName Name, const TArray<FName>& SetNames)
{
	for (const FName SetName : SetNames)
	{
		ControlSets.FindOrAdd(SetName).AddUnique(Name);
	}
	ControlSets.FindOrAdd("All").AddUnique(Name);
}


//======================================================================================================================
void FRigidBodyNameRecords::AddControls(const TArray<FName>& ControlNames, const FName SetName)
{
	AddUniqueNamesToSet(ControlNames, SetName, ControlSets);
	AddUniqueNamesToSet(ControlNames, "All", ControlSets);
}

//======================================================================================================================
void FRigidBodyNameRecords::RemoveControl(FName Name)
{
	for (TPair<FName, TArray<FName>>& Set : ControlSets)
	{
		Set.Value.Remove(Name);
	}
}

//======================================================================================================================
const TArray<FName>& FRigidBodyNameRecords::GetControlNamesInSet(FName SetName) const
{
	const TArray<FName>* Names = ControlSets.Find(SetName);
	if (Names)
	{
		return *Names;
	}
	static TArray<FName> FailureResult;
	return FailureResult;
}

//======================================================================================================================
void FRigidBodyNameRecords::AddBodyModifier(FName Name, FName SetName)
{
	BodyModifierSets.FindOrAdd(SetName).AddUnique(Name);
	BodyModifierSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FRigidBodyNameRecords::AddBodyModifier(FName Name, const TArray<FName>& SetNames)
{
	for (FName SetName : SetNames)
	{
		BodyModifierSets.FindOrAdd(SetName).AddUnique(Name);
	}
	BodyModifierSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FRigidBodyNameRecords::AddBodyModifiers(const TArray<FName>& BodyModifierNames, const FName SetName)
{
	AddUniqueNamesToSet(BodyModifierNames, SetName, BodyModifierSets);
	AddUniqueNamesToSet(BodyModifierNames, "All", BodyModifierSets);
}

//======================================================================================================================
void FRigidBodyNameRecords::RemoveBodyModifier(FName Name)
{
	for (TPair<FName, TArray<FName>>& Set : BodyModifierSets)
	{
		Set.Value.Remove(Name);
	}
}

//======================================================================================================================
const TArray<FName>& FRigidBodyNameRecords::GetBodyModifierNamesInSet(FName SetName) const
{
	const TArray<FName>* Names = BodyModifierSets.Find(SetName);
	if (Names)
	{
		return *Names;
	}
	static TArray<FName> FailureResult;
	return FailureResult;
}

//======================================================================================================================
void FRigidBodyNameRecords::Reset()
{
	ControlSets.Reset();
	BodyModifierSets.Reset();
}

//======================================================================================================================
TArray<FName> ExpandSetName(const FName InName, const TMap<FName, TArray<FName>>& SetNames)
{
	TArray<FName> OutputNames;
	if (const TArray<FName>* const FoundSet = SetNames.Find(InName))
	{
		OutputNames.Append(*FoundSet);
	}
	else
	{
		OutputNames.AddUnique(InName);
	}
	return OutputNames;
}

//======================================================================================================================
TArray<FName> ExpandSetNames(const TArray<FName>& InNames, const TMap<FName, TArray<FName>>& SetNames)
{
	TArray<FName> OutputNames;

	for (const FName Name : InNames)
	{
		OutputNames.Append(ExpandSetName(Name, SetNames));
	}

	return OutputNames;
}

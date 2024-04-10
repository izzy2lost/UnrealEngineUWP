// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef USE_OPENMODEL
class AlDagNode;
struct FColor;

typedef double AlMatrix4x4[4][4];

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

// Defined the reference in which the object has to be defined
enum class EAliasObjectReference
{
	LocalReference,  
	ParentReference, 
	WorldReference, 
};

class IAliasBRepConverter
{
public:
	virtual bool AddBRep(AlDagNode& DagNode, const FColor& Color, EAliasObjectReference ObjectReference) = 0;
};

}
#endif
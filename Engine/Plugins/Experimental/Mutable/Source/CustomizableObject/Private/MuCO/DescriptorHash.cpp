// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/DescriptorHash.h"

#include "MuCO/CustomizableObject.h"


FDescriptorHash::FDescriptorHash(const FCustomizableObjectInstanceDescriptor& Descriptor)
{
#if WITH_EDITORONLY_DATA
	if (Descriptor.CustomizableObject)
	{
		Hash = HashCombine(Hash, GetTypeHash(Descriptor.CustomizableObject->GetPathName()));
		Hash = HashCombine(Hash, GetTypeHash(Descriptor.CustomizableObject->GetCompilationGuid()));
	}
#endif

	for (const FCustomizableObjectBoolParameterValue& Value : Descriptor.BoolParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}	

	for (const FCustomizableObjectIntParameterValue& Value : Descriptor.IntParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	for (const FCustomizableObjectFloatParameterValue& Value : Descriptor.FloatParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectTextureParameterValue& Value : Descriptor.TextureParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectVectorParameterValue& Value : Descriptor.VectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectProjectorParameterValue& Value : Descriptor.ProjectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	Hash = HashCombine(Hash, GetTypeHash(Descriptor.State));
	Hash = HashCombine(Hash, GetTypeHash(Descriptor.bBuildParameterRelevancy));

	for (const TTuple<FName, FMultilayerProjector>& Pair : Descriptor.MultilayerProjectors)
	{
		// Hash = HashCombine(Hash, GetTypeHash(Pair.Key)); // Already hashed by the FMultilayerProjector.
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	MinLOD = Descriptor.MinLOD;
	MaxLOD = Descriptor.MaxLOD;	

	RequestedLODsPerComponent = Descriptor.RequestedLODLevels;
}


bool FDescriptorHash::operator==(const FDescriptorHash& Other) const
{
	return Hash == Other.Hash &&
		MinLOD == Other.MinLOD &&
		MaxLOD == Other.MaxLOD &&
		RequestedLODsPerComponent == Other.RequestedLODsPerComponent;
}


bool FDescriptorHash::operator!=(const FDescriptorHash& Other) const
{
	return !(*this == Other);
}


bool FDescriptorHash::IsSubset(const FDescriptorHash& Other) const
{
	if (Hash != Other.Hash || MinLOD < Other.MinLOD || MaxLOD != Other.MaxLOD)
	{
		return false;
	}

	if (RequestedLODsPerComponent == Other.RequestedLODsPerComponent)
	{
		return true;
	}

	if (RequestedLODsPerComponent.Num() != Other.RequestedLODsPerComponent.Num())
	{
		return false;
	}
	
	for (int32 ComponentIndex = 0; ComponentIndex < RequestedLODsPerComponent.Num(); ++ComponentIndex)
	{
		const int32 RequestedLODs = RequestedLODsPerComponent[ComponentIndex];
		const int32 OtherRequestedLODs = Other.RequestedLODsPerComponent[ComponentIndex];
		for (uint16 LODIndex = MinLOD; LODIndex <= MaxLOD; ++LODIndex)
		{
			// To be a subset all bits set in RequestedLODs must be set in OtherRequestedLODs. OtherRequestedLODs can have additional bits set
			if (RequestedLODs & (1 << LODIndex) && !(OtherRequestedLODs & (1 << LODIndex)))
			{
				return false;
			}
		}
	}

	return true;
}


void FDescriptorHash::UpdateMinMaxLOD(const int32 InMinLOD, const int32 InMaxLOD)
{
	MinLOD = InMinLOD;
	MaxLOD = InMaxLOD;	
}


int32 FDescriptorHash::GetMinLOD() const
{
	return MinLOD;
}


int32 FDescriptorHash::GetMaxLOD() const
{
	return MaxLOD;
}


void FDescriptorHash::UpdateRequestedLODs(const TArray<uint16>& InRequestedLODs)
{
	RequestedLODsPerComponent = InRequestedLODs;
}


const TArray<uint16>& FDescriptorHash::GetRequestedLODs() const
{
	return RequestedLODsPerComponent;
}


FString FDescriptorHash::ToString() const
{
	TStringBuilder<150> Builder;

	Builder.Appendf(TEXT("(Hash=%u,"), Hash);
	Builder.Appendf(TEXT("MinLOD=%i,"), MinLOD);
	Builder.Appendf(TEXT("MaxLOD=%i,"), MaxLOD);
	Builder.Appendf(TEXT("RequestredLODLevels=["));

	for (const uint16 RequestedLODs : RequestedLODsPerComponent)
	{
		Builder.Appendf(TEXT("%i,"), RequestedLODs);
	}
	
	Builder.Appendf(TEXT("])"));

	return Builder.ToString();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MultilayerProjector.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstanceDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MultilayerProjector)


const FString FMultilayerProjector::MULTILAYER_PROJECTOR_PARAMETERS_INVALID = TEXT("Invalid Multilayer Projector Parameters.");


void FMultilayerProjectorLayer::Read(const FCustomizableObjectInstanceDescriptor& Descriptor, const FString& ParamName, const int32 LayerIndex)
{
	if (!Descriptor.IsMultilayerProjector(ParamName))
	{
		ensureAlwaysMsgf(false, TEXT("%s"), *FMultilayerProjector::MULTILAYER_PROJECTOR_PARAMETERS_INVALID);
		return;
	}

	check(LayerIndex >= 0 && LayerIndex < Descriptor.NumProjectorLayers(*ParamName)); // Layer out of range.

	{
		ECustomizableObjectProjectorType DummyType;
		Descriptor.GetProjectorValue(ParamName, Position, Direction, Up, Scale, Angle, DummyType, LayerIndex);
	}

	{
		const int32 ImageParamIndex = Descriptor.FindTypedParameterIndex(ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX, EMutableParameterType::Int);
		Image = Descriptor.GetIntParameters()[ImageParamIndex].ParameterRangeValueNames[LayerIndex];
	}

	{
		const int32 OpacityParamIndex = Descriptor.FindTypedParameterIndex(ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX, EMutableParameterType::Float);
		Opacity = Descriptor.GetFloatParameters()[OpacityParamIndex].ParameterRangeValues[LayerIndex];
	}
}


void FMultilayerProjectorLayer::Write(FCustomizableObjectInstanceDescriptor& Descriptor, const FString& ParamName, int32 LayerIndex) const
{
	if (!Descriptor.IsMultilayerProjector(ParamName))
	{
		ensureAlwaysMsgf(false, TEXT("%s"), *FMultilayerProjector::MULTILAYER_PROJECTOR_PARAMETERS_INVALID);
		return;
	}

	check(LayerIndex >= 0 && LayerIndex < Descriptor.NumProjectorLayers(*ParamName)); // Layer out of range.


	Descriptor.SetProjectorValue(ParamName, Position, Direction, Up, Scale, Angle, LayerIndex);
	Descriptor.SetIntParameterSelectedOption(ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX, Image, LayerIndex);
	Descriptor.SetFloatParameterSelectedOption(ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX, Opacity, LayerIndex);

}


uint32 GetTypeHash(const FMultilayerProjectorLayer& Key)
{
	uint32 Hash = GetTypeHash(Key.Position);

	Hash = HashCombine(Hash, GetTypeHash(Key.Direction));
	Hash = HashCombine(Hash, GetTypeHash(Key.Up));
	Hash = HashCombine(Hash, GetTypeHash(Key.Scale));
	Hash = HashCombine(Hash, GetTypeHash(Key.Angle));
	Hash = HashCombine(Hash, GetTypeHash(Key.Image));
	Hash = HashCombine(Hash, GetTypeHash(Key.Opacity));

	return Hash;
}


FMultilayerProjectorVirtualLayer::FMultilayerProjectorVirtualLayer(const FMultilayerProjectorLayer& Layer, const bool bEnabled, const int32 Order):
	FMultilayerProjectorLayer(Layer),
	bEnabled(bEnabled),
	Order(Order)
{
}


const FString FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX = FString("_NumLayers");

const FString FMultilayerProjector::OPACITY_PARAMETER_POSTFIX = FString("_Opacity");

const FString FMultilayerProjector::IMAGE_PARAMETER_POSTFIX = FString("_SelectedImages");

const FString FMultilayerProjector::POSE_PARAMETER_POSTFIX = FString("_SelectedPoses");

TArray<FName> FMultilayerProjector::GetVirtualLayers() const
{
	TArray<FName> VirtualLayers;
	VirtualLayersMapping.GetKeys(VirtualLayers);
	return VirtualLayers;
}


void FMultilayerProjector::CreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	if (!VirtualLayersMapping.Contains(Id))
	{
		const int32 Index = Descriptor.NumProjectorLayers(ParamName);
		
		Descriptor.CreateLayer(ParamName, Index);
		VirtualLayersMapping.Add(Id, Index);
		VirtualLayersOrder.Add(Id, NEW_VIRTUAL_LAYER_ORDER);
	}
}


FMultilayerProjectorVirtualLayer FMultilayerProjector::FindOrCreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	FMultilayerProjectorLayer Layer;
	bool bEnabled;
	int32 Order;

	if (const int32* Index = VirtualLayersMapping.Find(Id))
	{
		if (*Index == VIRTUAL_LAYER_DISABLED)
		{
			Layer = DisableVirtualLayers[Id];
			bEnabled = false;
		}
		else
		{
			Layer = Descriptor.GetLayer(ParamName, *Index);
			bEnabled = true;
		}

		Order = VirtualLayersOrder[Id];
	}
	else
	{
		const int32 NewIndex = Descriptor.NumProjectorLayers(ParamName);
		constexpr int32 NewOrder = NEW_VIRTUAL_LAYER_ORDER;
		
		Descriptor.CreateLayer(ParamName, NewIndex);
		VirtualLayersMapping.Add(Id, NewIndex);
		VirtualLayersOrder.Add(Id, NewOrder);

		Layer = Descriptor.GetLayer(ParamName, NewIndex);
		bEnabled = true;
		Order = NewOrder;
	}

	return FMultilayerProjectorVirtualLayer(Layer, bEnabled, Order);
}


void FMultilayerProjector::RemoveVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.
	
	if (*Index == VIRTUAL_LAYER_DISABLED)
	{
		DisableVirtualLayers.Remove(Id);
	}
	else
	{
		Descriptor.RemoveLayerAt(ParamName, *Index);
		
		for (TMap<FName, int32>::TIterator It =  VirtualLayersMapping.CreateIterator(); It; ++It)
		{
			if (It.Key() == Id)
			{
				It.RemoveCurrent();
			}
			else if (It.Value() > *Index) // Update following Layers.
			{
				--It.Value();
			}
		}
	}

	VirtualLayersOrder.Remove(Id);
}


FMultilayerProjectorVirtualLayer FMultilayerProjector::GetVirtualLayer(const FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id) const
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.

	const FMultilayerProjectorLayer Layer = Descriptor.GetLayer(ParamName, *Index);
	const bool bEnabled = *Index != VIRTUAL_LAYER_DISABLED;
	const int32 Order = VirtualLayersOrder[Id];
	
	return FMultilayerProjectorVirtualLayer(Layer, bEnabled, Order);
}


void FMultilayerProjector::UpdateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer)
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.

	const bool bEnabled = *Index != VIRTUAL_LAYER_DISABLED;
	
	if (!bEnabled)
	{
		DisableVirtualLayers[Id] = static_cast<FMultilayerProjectorLayer>(Layer); // Update disabled layer.
		VirtualLayersOrder[Id] = Layer.Order;
	}
	else
	{
		int32* Order = VirtualLayersOrder.Find(Id);
		if (*Order != Layer.Order) // Order changed, check if it needs to be moved.
		{
			const int32 OldIndex = *Index;
			int32 NewIndex = CalculateVirtualLayerIndex(Id, Layer.Order);
			if (OldIndex != NewIndex) // Move required. Could be optimized by moving only the in-between values.
			{
				Descriptor.RemoveLayerAt(ParamName, OldIndex);
				UpdateMappingVirtualLayerDisabled(Id, OldIndex);

				if (OldIndex < NewIndex)
				{
					NewIndex -= 1;
				}
				
				Descriptor.CreateLayer(ParamName, NewIndex);
				UpdateMappingVirtualLayerEnabled(Id, NewIndex);
			}
			
			*Order = Layer.Order;
		}

		Descriptor.UpdateLayer(ParamName, *Index, static_cast<FMultilayerProjectorLayer>(Layer)); // Update enabled layer.
	}
	
	// Enable or disable virtual layer.
	if (Layer.bEnabled && !bEnabled)
	{
		const int32 NewIndex = CalculateVirtualLayerIndex(Id, VirtualLayersOrder[Id]);

		Descriptor.CreateLayer(ParamName, NewIndex);
		UpdateMappingVirtualLayerEnabled(Id, NewIndex);

		Descriptor.UpdateLayer(ParamName, NewIndex, static_cast<FMultilayerProjectorLayer>(Layer));
		
		DisableVirtualLayers.Remove(Id);
	}
	else if (!Layer.bEnabled && bEnabled)
	{
		Descriptor.RemoveLayerAt(ParamName, *Index);
		UpdateMappingVirtualLayerDisabled(Id, *Index);
		
		DisableVirtualLayers.Add(Id, Layer);
	}
}


FMultilayerProjector::FMultilayerProjector(const FName& InParamName) :
	ParamName(InParamName)
{
}


int32 FMultilayerProjector::CalculateVirtualLayerIndex(const FName& Id, const int32 InsertOrder) const
{
	int32 LayerBeforeIndex = -1;
	int32 LayerBeforeOrder = -1;
	
	for (const TTuple<FName, int>& MappingTuple : VirtualLayersMapping) // Find closest smallest layer.
	{
		if (MappingTuple.Value != VIRTUAL_LAYER_DISABLED && MappingTuple.Key != Id)
		{
			const int32 LayerOrder = VirtualLayersOrder[MappingTuple.Key];
			if (LayerOrder <= InsertOrder)
			{
				if ((LayerOrder > LayerBeforeOrder) ||
					(LayerOrder == LayerBeforeOrder && MappingTuple.Value > LayerBeforeIndex))
				{
					LayerBeforeIndex = MappingTuple.Value;
					LayerBeforeOrder = LayerOrder;
				}
			}
		}
	}
	
	return LayerBeforeIndex + 1;
}


void FMultilayerProjector::UpdateMappingVirtualLayerEnabled(const FName& Id, const int32 Index)
{
	for (TTuple<FName, int>& Tuple : VirtualLayersMapping)
	{
		if (Tuple.Key == Id)
		{
			Tuple.Value = Index;
		}
		else if (Tuple.Value >= Index) // Update following Layers.
		{
			++Tuple.Value;
		}
	}
}


void FMultilayerProjector::UpdateMappingVirtualLayerDisabled(const FName& Id, const int32 Index)
{
	for (TTuple<FName, int>& Tuple : VirtualLayersMapping)
	{
		if (Tuple.Key == Id)
    	{
			Tuple.Value = VIRTUAL_LAYER_DISABLED;
    	}
		else if (Tuple.Value > Index) // Update following Layers.
		{
			--Tuple.Value;
		}
	}
}


uint32 GetTypeHash(const FMultilayerProjector& Key)
{
	uint32 Hash = GetTypeHash(Key.ParamName);

	for (const TTuple<FName, int32>& Pair : Key.VirtualLayersMapping)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	for (const TTuple<FName, int32>& Pair : Key.VirtualLayersOrder)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}

	for (const TTuple<FName, FMultilayerProjectorLayer>& Pair : Key.DisableVirtualLayers)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	return Hash;
}


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeMaterialBase.generated.h"

class UCustomizableObjectLayout;
class UCustomizableObjectNodeMaterial;
class UEdGraphPin;
class UObject;


DECLARE_MULTICAST_DELEGATE(FPostImagePinModeChangedDelegate)


/** Equivalent to mu::NodeSurface but with limitations. Currently only nodes that generate a mu::NodeSurfaceNew inherit this (NodeMaterial and NodeCopyMaterial).
 * Nodes that generate mu::NodeSurfaceEdit (NodeEditMaterial, NodeExtendMaterial), mu::NodeSurfaceSwitch (NodeSwitchMaterial)... are excluded.
 *
 * WARNING: All methods in this class should be PURE_VIRTUAL!
 */
UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()
	
	// Own interface
	virtual const UCustomizableObjectNodeMaterial* GetMaterialNode() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMaterialNode, return {}; );

	UCustomizableObjectNodeMaterial* GetMaterialNode()
	{
		const UCustomizableObjectNodeMaterialBase* ConstThis = this;
		return const_cast<UCustomizableObjectNodeMaterial*>(ConstThis->GetMaterialNode());
	}

	virtual TArray<UCustomizableObjectLayout*> GetLayouts() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetLayouts, return {}; );
	
	virtual UMaterialInterface* GetMaterial() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMaterial, return {}; );
	
	virtual bool IsReuseMaterialBetweenLODs() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::IsReuseMaterialBetweenLODs, return {}; );

	virtual int32 GetMeshComponentIndex() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMeshComponentIndex, return {}; );
	
	virtual TArray<FString> GetTags() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetTags, return {}; );
	
	virtual UEdGraphPin* GetMeshPin() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMeshPin, return {}; );

	virtual UEdGraphPin* GetMaterialAssetPin() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMaterialAssetPin, return {}; );
	
	/** Returns the number of Material Parameters. */
	virtual int32 GetNumParameters(EMaterialParameterType Type) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetNumParameters, return {}; );

	/** Returns the Material Parameter id.
	 *
	 * @param ParameterIndex Have to be valid. */
	virtual FGuid GetParameterId(EMaterialParameterType Type, int32 ParameterIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetParameterId, return {}; );
	
	/** Returns the Material Parameter name.
	 *
	 * @param ParameterIndex Have to be valid. */
	virtual FName GetParameterName(EMaterialParameterType Type, int32 ParameterIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetParameterName, return {}; );
	
	/** Get the Material Parameter layer index.
	 *
	 * @param ParameterIndex Have to be valid.
	 * @returns INDEX_NONE for global parameters. */
	virtual int32 GetParameterLayerIndex(EMaterialParameterType Type, int32 ParameterIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetParameterLayerIndex, return {}; );
	
	/** Get the Material Parameter layer name.
	 *
	 * @param ParameterIndex Have to be valid. */
	virtual FText GetParameterLayerName(EMaterialParameterType Type, int32 ParameterIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetParameterLayerName, return {}; );

	/** Returns true if the Material contains the given Material Parameter. */
	virtual bool HasParameter(const FGuid& ParameterId) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::HasParameter, return {}; );
	
	/** Get the Vector pin for the given Material Vector Parameter.
	 * Not all parameters have pin.
	 *
	 * @param ParameterIndex Has to be valid.
	 * @return Can return nullptr. */
	virtual const UEdGraphPin* GetParameterPin(EMaterialParameterType Type, int32 ParameterIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetMaterialNode, return {}; );
	
	// --------------------
	// IMAGES PARAMETERS
	// --------------------

	/** Returns true if the Material Texture Parameter goes through Mutable.
	 *
	 * @param ImageIndex Have to be valid. */
	virtual bool IsImageMutableMode(int32 ImageIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::IsImageMutableMode, return {}; );

	/** Given an Image pin, returns true if the Material Texture Parameter goes through Mutable. */
	virtual bool IsImageMutableMode(const UEdGraphPin& Pin) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::IsImageMutableMode, return {}; );

	/** Update a Material Texture Parameter Mode. */
	virtual void UpdateImagePinMode(const FGuid ParameterId) PURE_VIRTUAL(UCustomizableObjectNodeMaterial::UpdateImagePinMode, );
	
	/** Update a Material Texture Parameter Mode. */
	virtual void UpdateImagePinMode(const UEdGraphPin& Pin) PURE_VIRTUAL(UCustomizableObjectNodeMaterial::UpdateImagePinMode, );

	/** Update all Material Texture Parameters Mode. */
	virtual void UpdateAllImagesPinMode() PURE_VIRTUAL(UCustomizableObjectNodeMaterial::UpdateAllImagesPinMode, );
	
	/** Returns the reference texture assigned to a Material Texture Parameter.
	 *
	 * @param ImageIndex Have to be valid.
	 * @return nullptr if it does not have one assigned. */
	virtual UTexture2D* GetImageReferenceTexture(int32 ImageIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetImageReferenceTexture, return {}; );

	/** Returns the Texture set in the Material Texture Parameter.
	 *
	 * @param ImageIndex Have to be valid. */
	virtual UTexture2D* GetImageValue(int32 ImageIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetImageValue, return {}; );
	
	/** Get the Material Texture Parameter UV Index.
	 *
	 * @param ImageIndex Have to be valid.
	 * @return Return -1 if the UV Index is set to Ignore. Return >= 0 for a valid UV Index.  */ 
	virtual int32 GetImageUVLayout(int32 ImageIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::GetImageUVLayout, return {}; );

	// This method should be overidden in all derived classes
	virtual UEdGraphPin* OutputPin() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::OutputPin, return {}; );
	
	virtual FPostImagePinModeChangedDelegate* GetPostImagePinModeChangedDelegate() PURE_VIRTUAL(UCustomizableObjectNodeMaterial::OutputPin, return {}; );
	
	virtual bool RealMaterialDataHasChanged() const PURE_VIRTUAL(UCustomizableObjectNodeMaterial::OutputPin, return {}; );
};


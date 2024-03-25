// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeEditLayer.generated.h"

enum class ELandscapeToolTargetType : uint8;
struct FLandscapeLayer;
class ALandscape;

UCLASS(MinimalAPI, Abstract)
class ULandscapeEditLayerBase : public UObject
{
	GENERATED_BODY()

public:
	// Little wrapper class to let edit layers expose some actions generically : 
	class FEditLayerAction
	{
		FEditLayerAction() = delete;

	public:
		class FExecuteParams
		{
			FExecuteParams() = delete;

		public:
			LANDSCAPE_API FExecuteParams(const FLandscapeLayer* InLayer, ALandscape* InLandscape)
				: Layer(InLayer)
				, Landscape(InLandscape)
			{
				check((InLayer != nullptr) && (InLandscape != nullptr));
			}

			inline const FLandscapeLayer* GetLayer() const { return Layer; }
			inline ALandscape* GetLandscape() const { return Landscape; }

		private:
			const FLandscapeLayer* Layer = nullptr;
			ALandscape* Landscape = nullptr;
		};

		struct FExecuteResult
		{
			LANDSCAPE_API FExecuteResult() = default;
			LANDSCAPE_API FExecuteResult(bool bInSuccess, const FText& InReason = FText())
				: bSuccess(bInSuccess)
				, Reason(InReason)
			{}

			bool bSuccess = true;
			FText Reason;
		};

		DECLARE_DELEGATE_RetVal_OneParam(FExecuteResult, FExecuteDelegate, const FExecuteParams& /*InParams*/);
		DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanExecuteDelegate, const FExecuteParams& /*InParams*/, FText& /*OutReason*/);

		LANDSCAPE_API FEditLayerAction(const FText& InLabel, const FExecuteDelegate& InExecuteDelegate, const FCanExecuteDelegate& InCanExecuteDelegate)
			: Label(InLabel)
			, ExecuteDelegate(InExecuteDelegate)
			, CanExecuteDelegate(InCanExecuteDelegate)
		{}

		inline const FText& GetLabel() const { return Label; }
		inline const FExecuteDelegate& GetExecuteDelegate() const { return ExecuteDelegate; }
		inline const FCanExecuteDelegate& GetCanExecuteDelegate() const { return CanExecuteDelegate; }

	private:
		FText Label;
		FExecuteDelegate ExecuteDelegate;
		FCanExecuteDelegate CanExecuteDelegate;
	};

public:
	/**
	 * @return true if the this edit layer has support for the target type (heightmap, weightmap, visibility)
	 */
	virtual bool SupportsTargetType(ELandscapeToolTargetType InType) const
		PURE_VIRTUAL(ULandscapeEditLayerBase::SupportsTargetType, return true; );

	/**
	 * @return true if the edit layer can store heightmaps/weightmaps in the ALandscapeProxy (e.g. should return false for purely procedural layers, to avoid allocating textures)
	 */
	virtual bool NeedsPersistentTextures() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::NeedsPersistentTextures, return false; );

	/**
	* @return true if the edit layer can be manually edited via the landscape editing tools :
	*/
	virtual bool SupportsEditingTools() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::SupportsEditingTools, return true; );

	/**
	 * @return true if it's allowed to have more than one edit layer of this type at a time
	 */
	virtual bool SupportsMultiple() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::SupportsMultiple, return true; );

	/**
	 * @return true if the layer supports a layer above being collapsed onto it
	 */
	virtual bool SupportsBeingCollapsedAway() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::SupportsBeingCollapsedAway, return true; );

	/**
	 * @return true if the layer supports being collapsed onto a layer underneath
	 */
	virtual bool SupportsCollapsingTo() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::SupportsCollapsingTo, return true; );

	/**
	* @return the default name to use when creating a new layer of this type
	*/
	virtual FString GetDefaultName() const
		PURE_VIRTUAL(ULandscapeEditLayerBase::GetDefaultName, return FString(); );

	/**
	* @return a list of actions that can be triggered on this edit layer
	*/
	virtual TArray<FEditLayerAction> GetActions() const { return {}; }

	// TODO [jonathan.bard] : Remove : temporary method to give the edit layer an opportunity to change some settings on the old struct (FLandscapeLayer) upon creation. 
	//  The better way would be to move most of the settings to ULandscapeEditLayer(Base?) and expose a "property changed" event that both the UI and runtime code 
	//  could listen to in order to generically react to any change on the layer settings, including on the derived class (e.g. if there were some splines layer-specific 
	//  settings, on property change, the event would be triggered and the landscape layers would be updated as a result), instead of providing ad-hoc functions on 
	//  ALandscape, like we do currently (e.g. SetLayerAlpha, SetLayerVisibility, SetLayerName, etc.) on both the runtime code (ALandscape) and the UI code (FEdModeLandscape)
	virtual void OnLayerCreated(FLandscapeLayer& Layer) {}
};

/** 
	This is the standard type of edit layer. It can be manually authored (sculpted, painted, etc.) in the landscape editor 
*/
UCLASS(MinimalAPI)
class ULandscapeEditLayer : public ULandscapeEditLayerBase
{
	GENERATED_BODY()

public:
	// Begin ULandscapeEditLayerBase implementation
	virtual bool SupportsTargetType(ELandscapeToolTargetType InType) const override;
	virtual bool SupportsEditingTools() const override { return true; }
	virtual bool NeedsPersistentTextures() const override { return true; };
	virtual bool SupportsMultiple() const override { return true; }
	virtual bool SupportsBeingCollapsedAway() const override { return true; }
	virtual bool SupportsCollapsingTo() const override { return true; }
	virtual FString GetDefaultName() const { return TEXT("Layer"); }
	// End ULandscapeEditLayerBase implementation

protected:

};

/** 
	Base class for procedural layers. Procedural layers cannot be edited through standard editing tools
*/
UCLASS(MinimalAPI, Abstract)
class ULandscapeEditLayerProcedural : public ULandscapeEditLayerBase
{
	GENERATED_BODY()

public:
	// Begin ULandscapeEditLayerBase implementation
	virtual bool SupportsEditingTools() const override { return false; } // procedural layers cannot be edited through standard editing tools
	virtual bool SupportsBeingCollapsedAway() const override { return false; } // this is a procedural and therefore cannot be collapsed 
	virtual bool SupportsCollapsingTo() const override { return NeedsPersistentTextures(); } // if the layer has persistent textures, it supports being collapsed into a layer underneath (this may become unneeded if we make the collapse happen on the GPU)
	// End ULandscapeEditLayerBase implementation
};

/** 
	Procedural edit layer that lets the user manipulate its content using landscape splines (Splines tool in the Manage panel) 
*/
UCLASS(MinimalAPI)
class ULandscapeEditLayerSplines : public ULandscapeEditLayerProcedural
{
	GENERATED_BODY()

public:
	// Begin ULandscapeEditLayerBase implementation
	virtual bool SupportsTargetType(ELandscapeToolTargetType InType) const override;
	virtual bool NeedsPersistentTextures() const override { return true; }; // it's a layer computed on the CPU and outputting to persistent textures
	virtual bool SupportsMultiple() const override { return false; } // only one layer of this type is allowed
	virtual FString GetDefaultName() const override { return TEXT("Splines"); }
	virtual void OnLayerCreated(FLandscapeLayer& Layer) override;
	virtual TArray<FEditLayerAction> GetActions() const override;
	// End ULandscapeEditLayerBase implementation

protected:

};

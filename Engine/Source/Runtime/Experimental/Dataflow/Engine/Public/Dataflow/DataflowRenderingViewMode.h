// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "ChaosLog.h"

namespace Dataflow
{
	//
	// Dataflow Construction View Modes allow the user to choose different views on the same node output. The View Mode system serves two main purposes: 
	// 
	// 1) In the Dataflow Editor's Construction Viewport, the currently active IDataflowConstructionViewMode will determine which ELevelViewportType should be used for rendering.
	//    In other words, the the ViewMode will determine whether we have a 3D perspective camera or a 2D orthographic camera.
	// 2) In the Datflow Rendering Factory (Dataflow::FRenderingFactory), the rendering callbacks that are registered will also be able to return different 
	//    render data based on what the currently active IDataflowConstructionViewMode in the viewport is.
	//
	// For example, suppose you have created a new type FMyMeshType that contains both a 3D Mesh representation and a 2D Mesh representation (for, say, UVs.) If you want to
	// write custom ViewModes for it, here is the process:
	// 
	// 1) Define two new classes, e.g. FMyMeshType2DViewMode (inheriting from FDataflowConstruction2DViewModeBase) and FMyMeshType3DViewMode (inheriting from FDataflowConstruction3DViewModeBase)
	// 2) In your module initialization, register both view modes with the Dataflow::FRenderingViewModeFactory
	// 3) Create a render function callback class and register it with Dataflow::FRenderingFactory. 
	//    - The CanRender function should return true if the view mode is either of your two new custom view modes.
	//    - The Render function should return different RenderCollections depending on what the current view mode is (which can be determined from the FGraphRenderingState parameter.)
	//


	//
	// Equivalent to ELevelViewportType, but we don't want to add an Editor dependency here
	//
	enum EDataflowViewportType : int
	{
		DVT_OrthoXY = 0,
		DVT_OrthoXZ = 1,
		DVT_OrthoYZ = 2,
		DVT_Perspective = 3,
		DVT_OrthoFreelook = 4,
		DVT_OrthoNegativeXY = 5,
		DVT_OrthoNegativeXZ = 6,
		DVT_OrthoNegativeYZ = 7,
		DVT_MAX,
		DVT_None = 255,
	};


	class IDataflowConstructionViewMode
	{
	public:
		virtual ~IDataflowConstructionViewMode() = default;

		DATAFLOWENGINE_API virtual FName GetName() const = 0;
		DATAFLOWENGINE_API virtual FText GetButtonText() const = 0;
		DATAFLOWENGINE_API virtual FText GetTooltipText() const = 0;
		DATAFLOWENGINE_API virtual EDataflowViewportType GetViewportType() const = 0;
		
		DATAFLOWENGINE_API bool IsPerspective() const;
	};

	// Base 2D and 3D types

	class FDataflowConstruction2DViewModeBase : public IDataflowConstructionViewMode
	{
	public:
		DATAFLOWENGINE_API virtual EDataflowViewportType GetViewportType() const override;
	};

	class FDataflowConstruction3DViewModeBase : public IDataflowConstructionViewMode
	{
	public:
		DATAFLOWENGINE_API virtual EDataflowViewportType GetViewportType() const override;
	};

	// Concrete default 2D and 3D types

	class FDataflowConstruction2DViewMode : public FDataflowConstruction2DViewModeBase
	{
	public:
		DATAFLOWENGINE_API static FName Name;
		virtual ~FDataflowConstruction2DViewMode() = default;
	private:
		DATAFLOWENGINE_API virtual FName GetName() const override;
		DATAFLOWENGINE_API virtual FText GetButtonText() const override;
		DATAFLOWENGINE_API virtual FText GetTooltipText() const override;
	};

	class FDataflowConstruction3DViewMode : public FDataflowConstruction3DViewModeBase
	{
	public:
		DATAFLOWENGINE_API static FName Name;
		virtual ~FDataflowConstruction3DViewMode() = default;
	private:
		DATAFLOWENGINE_API virtual FName GetName() const override;
		DATAFLOWENGINE_API virtual FText GetButtonText() const override;
		DATAFLOWENGINE_API virtual FText GetTooltipText() const override;
	};

	//
	// ViewMode registry/factory
	//

	class FRenderingViewModeFactory
	{
	public:

		FRenderingViewModeFactory();

		// FLazySingleton
		static DATAFLOWENGINE_API FRenderingViewModeFactory& GetInstance();
		static DATAFLOWENGINE_API void TearDown();

		DATAFLOWENGINE_API void RegisterViewMode(TUniquePtr<IDataflowConstructionViewMode>&& ViewMode);
		DATAFLOWENGINE_API void DeregisterViewMode(const FName& ViewModeName);

		DATAFLOWENGINE_API const IDataflowConstructionViewMode* GetViewMode(const FName& ViewModeName) const;

		DATAFLOWENGINE_API const TMap<FName, TUniquePtr<IDataflowConstructionViewMode>>& GetViewModes() const;

	private:

		DATAFLOWENGINE_API static TUniquePtr<FRenderingViewModeFactory> Instance;

		TMap<FName, TUniquePtr<IDataflowConstructionViewMode>> ViewModeMap;
	};
}


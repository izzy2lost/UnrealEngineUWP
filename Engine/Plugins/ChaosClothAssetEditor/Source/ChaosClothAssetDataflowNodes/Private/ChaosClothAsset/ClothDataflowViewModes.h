// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowRenderingViewMode.h"

namespace UE::Chaos::ClothAsset
{
	class FCloth2DSimViewMode : public Dataflow::FDataflowConstruction2DViewModeBase
	{
	public:
		static const FName Name;
		virtual ~FCloth2DSimViewMode() = default;
	private:
		virtual FName GetName() const override;
		virtual FText GetButtonText() const override;
		virtual FText GetTooltipText() const override;
	};

	class FCloth3DSimViewMode : public Dataflow::FDataflowConstruction3DViewModeBase
	{
	public:
		static const FName Name;
		virtual ~FCloth3DSimViewMode() = default;
	private:
		virtual FName GetName() const override;
		virtual FText GetButtonText() const override;
		virtual FText GetTooltipText() const override;
	};

	class FClothRenderViewMode : public Dataflow::FDataflowConstruction3DViewMode
	{
	public:
		static const FName Name;
		virtual ~FClothRenderViewMode() = default;
	private:
		virtual FName GetName() const override;
		virtual FText GetButtonText() const override;
		virtual FText GetTooltipText() const override;
	};

}  // End namespace UE::Chaos::ClothAsset

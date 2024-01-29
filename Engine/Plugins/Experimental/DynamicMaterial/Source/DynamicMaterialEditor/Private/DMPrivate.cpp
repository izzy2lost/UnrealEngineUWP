// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMPrivate.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialStage.h"
#include "DMDefs.h"
#include "MaterialExpressionIO.h"

namespace UE::DynamicMaterialEditor::Private
{
	void SetMask(FExpressionInput& InInputConnector, const FExpressionOutput& InOutputConnector, int32 InChannelOverride)
	{
		const bool bUseOutputMask = (InChannelOverride != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);

		if (!bUseOutputMask)
		{
			InInputConnector.SetMask(InOutputConnector.Mask, InOutputConnector.MaskR, InOutputConnector.MaskG, InOutputConnector.MaskB, InOutputConnector.MaskA);
		}
		else
		{
			int32 MaskR = (!!(InChannelOverride & FDMMaterialStageConnectorChannel::FIRST_CHANNEL)) * (!!(InOutputConnector.MaskR));
			int32 MaskG = (!!(InChannelOverride & FDMMaterialStageConnectorChannel::SECOND_CHANNEL)) * (!!(InOutputConnector.MaskG));
			int32 MaskB = (!!(InChannelOverride & FDMMaterialStageConnectorChannel::THIRD_CHANNEL)) * (!!(InOutputConnector.MaskB));
			int32 MaskA = (!!(InChannelOverride & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL)) * (!!(InOutputConnector.MaskA));

			int32 MatchingMasks = MaskR + MaskG + MaskB + MaskA;

			if (MatchingMasks == 0)
			{
				InInputConnector.SetMask(InOutputConnector.Mask, InOutputConnector.MaskR, InOutputConnector.MaskG, InOutputConnector.MaskB, InOutputConnector.MaskA);
			}
			else
			{
				InInputConnector.SetMask(1, MaskR, MaskG, MaskB, MaskA);
			}
		}
	}
}

FDMMaterialLayerReference::FDMMaterialLayerReference()
	: FDMMaterialLayerReference(nullptr)
{
}

FDMMaterialLayerReference::FDMMaterialLayerReference(UDMMaterialLayerObject* InLayer)
{
	LayerWeak = InLayer;
}

UDMMaterialLayerObject* FDMMaterialLayerReference::GetLayer() const
{
	return LayerWeak.Get();
}

bool FDMMaterialLayerReference::IsBaseEnabled() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageEnabled(EDMMaterialLayerStage::Base);
	}

	return false;
}

bool FDMMaterialLayerReference::IsBaseBeingEdited() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageBeingEdited(EDMMaterialLayerStage::Base);
	}

	return false;
}

bool FDMMaterialLayerReference::IsMaskEnabled() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageEnabled(EDMMaterialLayerStage::Mask);
	}

	return false;
}

bool FDMMaterialLayerReference::IsMaskBeingEdited() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageBeingEdited(EDMMaterialLayerStage::Mask);
	}

	return false;
}

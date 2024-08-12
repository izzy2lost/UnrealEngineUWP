// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Misc/Guid.h"

#if IMAGE_WIDGETS_WITH_AB_COMPARISON
namespace UE::ImageWidgets
{
	/**
	 * Provides data and logic for AB comparisons of images.
	 */
	class FImageABComparison
	{
	public:
		DECLARE_DELEGATE_RetVal(FGuid, FGetCurrentImageGuid)
		DECLARE_DELEGATE_RetVal_OneParam(bool, FImageIsValid, const FGuid&)

		FImageABComparison(FImageIsValid&& ImageIsValid, FGetCurrentImageGuid&& GetCurrentImageGuid)
			: ImageIsValid(ImageIsValid)
			, GetCurrentImageGuid(GetCurrentImageGuid)
		{
			check(ImageIsValid.IsBound());
			check(GetCurrentImageGuid.IsBound());
		}

		enum EAorB
		{
			A,
			B
		};

		bool CanSetABComparison(EAorB AorB) const
		{
			const FGuid CurrentGuid = GetCurrentImageGuid.Execute();
			const bool bGuidIsValidAndCurrentGuid = Guids[static_cast<int32>(AorB)].IsValid() && Guids[static_cast<int32>(AorB)] == CurrentGuid;
			const bool bViewerHasValidImage = ImageIsValid.Execute(CurrentGuid);
			const bool bGuidIsDisabledAndOtherGuidIsNotCurrentGuid = !Guids[static_cast<int32>(AorB)].IsValid() && Guids[!static_cast<bool>(AorB)] != CurrentGuid;

			return bGuidIsValidAndCurrentGuid || (bViewerHasValidImage && bGuidIsDisabledAndOtherGuidIsNotCurrentGuid);
		}

		void SetABComparison(EAorB AorB, const FGuid& Guid)
		{
			Guids[static_cast<int32>(AorB)] = Guid;
		}

		bool ABComparisonIsSet(EAorB AorB) const
		{
			return Guids[static_cast<int32>(AorB)].IsValid();
		}

		bool IsActive() const
		{
			return Guids[0].IsValid() && Guids[1].IsValid();
		}

		const FGuid& GuidA() const
		{
			return Guids[0];
		}

		const FGuid& GuidB() const
		{
			return Guids[1];
		}

	private:
		FGuid Guids[2];
		FImageIsValid ImageIsValid;
		FGetCurrentImageGuid GetCurrentImageGuid;
	};
}
#else
#pragma message("Do not include this header!")
#endif

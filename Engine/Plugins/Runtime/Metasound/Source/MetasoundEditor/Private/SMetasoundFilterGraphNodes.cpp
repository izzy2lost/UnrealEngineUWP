// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetasoundFilterGraphNodes.h"

#include "Algo/Count.h"
#include "DSP/FloatArrayMath.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphNode.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SMetasoundFilterGraphNodes"

namespace Metasound::Editor
{
	namespace SMetasoundFilterGraphNodesPrivate
	{
		inline bool IsConnectedAudioOutputPin(const UEdGraphPin* Pin)
		{
			const bool bIsPinConnected = !Pin->LinkedTo.IsEmpty();
			const bool bIsAudioOutputPin = Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == FGraphBuilder::PinCategoryAudio;
			return (bIsAudioOutputPin && bIsPinConnected);
		}

		template<class T>
		inline TOptional<T> GetPinValue(UMetasoundEditorGraphNode& MetaSoundNode, FName PinName)
		{
			UEdGraphPin** FoundNamedInputPin = MetaSoundNode.Pins.FindByPredicate([&PinName](const UEdGraphPin* InPin)
				{
					return (InPin->Direction == EGPD_Input && InPin->PinType.PinCategory != FGraphBuilder::PinCategoryAudio && InPin->PinName == PinName);
				});

			if (!FoundNamedInputPin)
			{
				return NullOpt;
			}

			T PinValue;

			UEdGraphPin* Pin = *FoundNamedInputPin;
			if (Pin->LinkedTo.IsEmpty())
			{
				FMetasoundFrontendLiteral DefaultLiteral;
				if (FGraphBuilder::GetPinLiteral(*Pin, DefaultLiteral))
				{
					if (DefaultLiteral.TryGet(PinValue))
					{
						return PinValue;
					}
				}
			}
			else
			{
				// Find connected output for the input (only ever one):
				UEdGraphPin* SourcePin = Pin->LinkedTo.Last();
				ensure(SourcePin->Direction == EGPD_Output);

				const UEdGraphPin* ReroutedOutputPin = FGraphBuilder::FindReroutedOutputPin(SourcePin);

				if (const UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(ReroutedOutputPin->GetOwningNode()))
				{
					TSharedPtr<FEditor> Editor = FGraphBuilder::GetEditorForNode(MetaSoundNode);
					if (Editor.IsValid())
					{
						const FGuid NodeID = Node->GetNodeID();
						const FName OutputName = ReroutedOutputPin->GetFName();
						if (Editor->GetConnectionManager().GetValue(NodeID, OutputName, PinValue))
						{
							return PinValue;
						}
					}

					if (const UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(Node))
					{
						if (const UMetasoundEditorGraphMember* Member = MemberNode->GetMember())
						{
							if (Editor.IsValid())
							{
								// For an input member, we must use this NodeID/VertexName pair with the GraphConnectionManager:
								const FGuid MemberID = Member->GetMemberID();
								const FName MemberName = Member->GetMemberName();
								if (Editor->GetConnectionManager().GetValue(MemberID, MemberName, PinValue))
								{
									return PinValue;
								}
							}

							if (const UMetasoundEditorGraphMemberDefaultLiteral* MemberDefaultLiteral = Member->GetLiteral())
							{
								const FMetasoundFrontendLiteral DefaultLiteral = MemberDefaultLiteral->GetDefault();
								if (DefaultLiteral.TryGet(PinValue))
								{
									return PinValue;
								}
							}
						}
					}
				}
			}

			return NullOpt;
		}
	} // namespace SMetasoundFilterGraphNodesPrivate

	const float SMetaSoundFilterGraphNode::SampleRate = 48000.0f;

	SMetaSoundFilterGraphNode::~SMetaSoundFilterGraphNode()
	{
		if (ContextMenuExtension.IsValid())
		{
			FrequencyResponsePlot->RemoveContextMenuExtension(ContextMenuExtension.ToSharedRef());
		}
	}

	void SMetaSoundFilterGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		bHasFilterParams = UpdateFilterParams();

		SMetaSoundGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}

	void SMetaSoundFilterGraphNode::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
	{
		if (!FrequencyResponsePlot.IsValid())
		{
			FrequencyResponsePlot = SNew(SAudioSpectrumPlot)
				.Clipping(EWidgetClipping::ClipToBounds)
				.ViewMinSoundLevel(-24.0f)
				.TiltExponent_Lambda([] { return 0.0f; }) // Binding this property has the effect of hiding its context menu entry (Tilting the spectrum is not desired here).
				.FrequencyAxisPixelBucketMode_Lambda([]() { return EAudioSpectrumPlotFrequencyAxisPixelBucketMode::Sample; }) // Binding this property has the effect of hiding its context menu entry (PixelBucketMode is not much use here).
				.OnGetAudioSpectrumData(this, &SMetaSoundFilterGraphNode::GetAudioSpectrumData);

			ContextMenuExtension = FrequencyResponsePlot->AddContextMenuExtension(EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateSP(this, &SMetaSoundFilterGraphNode::ExtendSpectrumPlotContextMenu));
		}

		MainBox->AddSlot()
			.Padding(1.0f, 0.0f)
			[
				SNew(SBox)
					.MinDesiredHeight(125.0f)
					[
						FrequencyResponsePlot.ToSharedRef()
					]
			];
	}

	FAudioPowerSpectrumData SMetaSoundFilterGraphNode::GetAudioSpectrumData()
	{
		if (!bHasFilterParams || !FrequencyResponsePlot.IsValid())
		{
			return FAudioPowerSpectrumData();
		}

		// Fill array of frequencies (in Hz) to be plotted (these may be log spaced or linear, as defined by the FrequencyResponsePlot ScaleInfo transform):
		CenterFrequencies.Reset();
		float LocalX = 0.0f;
		const float LocalXEnd = FrequencyResponsePlot->GetPaintSpaceGeometry().GetLocalSize().X;
		const FAudioSpectrumPlotScaleInfo ScaleInfo = FrequencyResponsePlot->GetScaleInfo();
		while (LocalX <= LocalXEnd)
		{
			const float Frequency = ScaleInfo.LocalXToFrequency(LocalX);
			CenterFrequencies.Add(Frequency);
			LocalX += 1.0f;
		}

		// Create array of complex z values for all desired frequencies (interleaved real and imaginary):
		const int32 NumFrequencies = CenterFrequencies.Num();
		TArray<float> ComplexValues;
		ComplexValues.SetNumUninitialized(2 * NumFrequencies);
		const float HzToOmega = UE_TWO_PI / SampleRate;
		for (int32 Index = 0; Index < NumFrequencies; Index++)
		{
			const float Omega = HzToOmega * CenterFrequencies[Index];
			FMath::SinCos(&ComplexValues[2 * Index + 1], &ComplexValues[2 * Index + 0], Omega);
		}

		// Get the frequency response for all of the frequencies:
		ArrayCalculateFilterResponseInPlace(ComplexValues);

		// Store frequency response as squared magnitudes:
		SquaredMagnitudes.SetNumUninitialized(NumFrequencies);
		Audio::ArrayComplexToPower(ComplexValues, SquaredMagnitudes);

		return FAudioPowerSpectrumData
		{
			.CenterFrequencies = CenterFrequencies,
			.SquaredMagnitudes = SquaredMagnitudes,
		};
	}

#pragma region Biquad Filter

	bool SMetaSoundBiquadFilterGraphNode::UpdateFilterParams()
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// Try and get all required filter params:
		const TOptional<int32> Type = GetPinValue<int32>(MetaSoundNode, TEXT("Type"));
		const TOptional<float> Frequency = GetPinValue<float>(MetaSoundNode, TEXT("Cutoff Frequency"));
		const TOptional<float> Bandwidth = GetPinValue<float>(MetaSoundNode, TEXT("Bandwidth"));
		const TOptional<float> FilterGainDb = GetPinValue<float>(MetaSoundNode, TEXT("Gain"));

		if (!Type.IsSet() || !Frequency.IsSet() || !Bandwidth.IsSet() || !FilterGainDb.IsSet())
		{
			return false;
		}

		const float MaxCutoffFrequency = 0.5f * SampleRate;
		const float CurrentFrequency = FMath::Clamp(*Frequency, 0.f, MaxCutoffFrequency);
		const float CurrentBandwidth = FMath::Max(*Bandwidth, 0.f);
		const float CurrentFilterGainDb = FMath::Clamp(*FilterGainDb, -90.0f, 20.0f);

		// Update the filter with the filter params:
		if (Filter.GetNumChannels() == 0)
		{
			constexpr int32 NumChannels = 1;
			Filter.Init(SampleRate, NumChannels, static_cast<Audio::EBiquadFilter::Type>(Type.GetValue()), CurrentFrequency, CurrentBandwidth, CurrentFilterGainDb);
		}
		else
		{
			Filter.SetParams(static_cast<Audio::EBiquadFilter::Type>(Type.GetValue()), CurrentFrequency, CurrentBandwidth, CurrentFilterGainDb);
		}

		return true;
	}

	void SMetaSoundBiquadFilterGraphNode::ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const
	{
		Filter.ArrayCalculateResponseInPlace(InOutComplexValues);
	}

#pragma endregion

#pragma region Ladder Filter

	SMetaSoundLadderFilterGraphNode::SMetaSoundLadderFilterGraphNode()
	{
		constexpr int32 NumChannels = 1;
		Filter.Init(SampleRate, NumChannels);
	}

	bool SMetaSoundLadderFilterGraphNode::UpdateFilterParams()
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// Try and get all required filter params:
		const TOptional<float> Frequency = GetPinValue<float>(MetaSoundNode, TEXT("Cutoff Frequency"));
		const TOptional<float> Resonance = GetPinValue<float>(MetaSoundNode, TEXT("Resonance"));

		if (!Frequency.IsSet() || !Resonance.IsSet())
		{
			return false;
		}

		const float MaxCutoffFrequency = 0.5f * SampleRate;
		const float CurrentFrequency = FMath::Clamp(*Frequency, 0.f, MaxCutoffFrequency);
		const float CurrentResonance = FMath::Clamp(*Resonance, 1.0f, 10.0f);

		// Update the filter with the filter params:
		Filter.SetQ(CurrentResonance);
		Filter.SetFrequency(CurrentFrequency);

		Filter.Update();

		return true;
	}

	void SMetaSoundLadderFilterGraphNode::ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const
	{
		Filter.ArrayCalculateResponseInPlace(InOutComplexValues);
	}

#pragma endregion

#pragma region OnePoleHighPass Filter

	SMetaSoundOnePoleHighPassFilterGraphNode::SMetaSoundOnePoleHighPassFilterGraphNode()
	{
		constexpr int32 NumChannels = 1;
		Filter.Init(SampleRate, NumChannels);
	}

	bool SMetaSoundOnePoleHighPassFilterGraphNode::UpdateFilterParams()
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// Try and get all required filter params:
		const TOptional<float> Frequency = GetPinValue<float>(MetaSoundNode, TEXT("Cutoff Frequency"));

		if (!Frequency.IsSet())
		{
			return false;
		}

		const float ClampedFreq = FMath::Clamp(0.0f, *Frequency, SampleRate);

		// Update the filter with the filter params:
		Filter.StartFrequencyInterpolation(ClampedFreq);

		return true;
	}

	void SMetaSoundOnePoleHighPassFilterGraphNode::ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const
	{
		Filter.ArrayCalculateResponseInPlace(InOutComplexValues);
	}

#pragma endregion

#pragma region OnePoleLowPass Filter

	SMetaSoundOnePoleLowPassFilterGraphNode::SMetaSoundOnePoleLowPassFilterGraphNode()
	{
		constexpr int32 NumChannels = 1;
		Filter.Init(SampleRate, NumChannels);
	}

	bool SMetaSoundOnePoleLowPassFilterGraphNode::UpdateFilterParams()
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// Try and get all required filter params:
		const TOptional<float> Frequency = GetPinValue<float>(MetaSoundNode, TEXT("Cutoff Frequency"));
		if (!Frequency.IsSet())
		{
			return false;
		}

		const float ClampedFreq = FMath::Clamp(0.0f, *Frequency, SampleRate);

		// Update the filter with the filter params:
		Filter.StartFrequencyInterpolation(ClampedFreq);

		return true;
	}

	void SMetaSoundOnePoleLowPassFilterGraphNode::ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const
	{
		Filter.ArrayCalculateResponseInPlace(InOutComplexValues);
	}

#pragma endregion

#pragma region State Variable Filter

	const FName SMetaSoundStateVariableFilterGraphNode::LowPassFilter(TEXT("Low Pass Filter"));
	const FName SMetaSoundStateVariableFilterGraphNode::HighPassFilter(TEXT("High Pass Filter"));
	const FName SMetaSoundStateVariableFilterGraphNode::BandPass(TEXT("Band Pass"));
	const FName SMetaSoundStateVariableFilterGraphNode::BandStop(TEXT("Band Stop"));

	SMetaSoundStateVariableFilterGraphNode::SMetaSoundStateVariableFilterGraphNode()
		: DisplayedFilterResponse(LowPassFilter)
	{
		constexpr int32 NumChannels = 1;
		Filter.Init(SampleRate, NumChannels);
	}

	bool SMetaSoundStateVariableFilterGraphNode::UpdateFilterParams()
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// If there are connected audio outputs but the current selection is not connected, auto-select a new filter type:
		UEdGraphPin** FirstConnectedAudioOutputPin = MetaSoundNode.Pins.FindByPredicate(IsConnectedAudioOutputPin);
		if (FirstConnectedAudioOutputPin != nullptr)
		{
			for (UEdGraphPin* Pin : MetaSoundNode.Pins)
			{
				const bool bIsPinConnected = !Pin->LinkedTo.IsEmpty();
				if (Pin->PinName == DisplayedFilterResponse && !bIsPinConnected)
				{
					DisplayedFilterResponse = (*FirstConnectedAudioOutputPin)->PinName;
				}
			}
		}

		// Set the filter type on the filter:
		if (DisplayedFilterResponse == LowPassFilter)
		{
			Filter.SetFilterType(Audio::EFilter::LowPass);
		}
		else if (DisplayedFilterResponse == HighPassFilter)
		{
			Filter.SetFilterType(Audio::EFilter::HighPass);
		}
		else if (DisplayedFilterResponse == BandPass)
		{
			Filter.SetFilterType(Audio::EFilter::BandPass);
		}
		else if (DisplayedFilterResponse == BandStop)
		{
			Filter.SetFilterType(Audio::EFilter::BandStop);
		}

		// Try and get all required filter params:
		const TOptional<float> Frequency = GetPinValue<float>(MetaSoundNode, TEXT("Cutoff Frequency"));
		const TOptional<float> Resonance = GetPinValue<float>(MetaSoundNode, TEXT("Resonance"));
		const TOptional<float> BandStopControl = GetPinValue<float>(MetaSoundNode, TEXT("Band Stop Control"));

		if (!Frequency.IsSet() || !Resonance.IsSet() || !BandStopControl.IsSet())
		{
			return false;
		}

		const float MaxCutoffFrequency = 0.5f * SampleRate;
		const float CurrentFrequency = FMath::Clamp(*Frequency, 0.f, MaxCutoffFrequency);
		const float CurrentResonance = FMath::Clamp(*Resonance, 0.f, 10.f);
		const float CurrentBandStopControl = FMath::Clamp(*BandStopControl, 0.f, 1.f);

		// Update the filter with the filter params:
		Filter.SetQ(CurrentResonance);
		Filter.SetFrequency(CurrentFrequency);
		Filter.SetBandStopControl(CurrentBandStopControl);

		Filter.Update();

		return true;
	}

	void SMetaSoundStateVariableFilterGraphNode::ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const
	{
		Filter.ArrayCalculateResponseInPlace(InOutComplexValues);
	}

	void SMetaSoundStateVariableFilterGraphNode::ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder)
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		// Display filter response selection submenu if no outputs are connected, or more than one output is connected:
		const int NumConnectedAudioOutputPins = Algo::CountIf(GetMetaSoundNode().Pins, IsConnectedAudioOutputPin);
		if (NumConnectedAudioOutputPins == 0 || NumConnectedAudioOutputPins > 1)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("DisplayedFilterResponse", "Displayed Filter Response"),
				FText(),
				FNewMenuDelegate::CreateSP(this, &SMetaSoundStateVariableFilterGraphNode::BuildFilterOutputSubMenu));
		}
	}

	void SMetaSoundStateVariableFilterGraphNode::BuildFilterOutputSubMenu(FMenuBuilder& SubMenu)
	{
		using namespace SMetasoundFilterGraphNodesPrivate;

		const UMetasoundEditorGraphNode& MetaSoundNode = GetMetaSoundNode();

		// Add menu entries for all connected audio output pins. If there are no connected audio output pins then add entries for all audio output pins:
		const bool bHasConnectedAudioOutputPins = MetaSoundNode.Pins.ContainsByPredicate(IsConnectedAudioOutputPin);
		for (UEdGraphPin* Pin : MetaSoundNode.Pins)
		{
			const bool bIsAudioOutputPin = Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == FGraphBuilder::PinCategoryAudio;
			if (bIsAudioOutputPin)
			{
				const bool bIsPinConnected = !Pin->LinkedTo.IsEmpty();
				if (bIsPinConnected || !bHasConnectedAudioOutputPins)
				{
					SubMenu.AddMenuEntry(
						FText::FromName(Pin->PinName),
						FText::FromString(Pin->PinToolTip),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSPLambda(this, [this, PinName = Pin->PinName]() { DisplayedFilterResponse = PinName; }),
							FCanExecuteAction(),
							FIsActionChecked::CreateSPLambda(this, [this, PinName = Pin->PinName]() { return DisplayedFilterResponse == PinName; })
						),
						NAME_None,
						EUserInterfaceActionType::ToggleButton);
				}
			}
		}
	}

#pragma endregion

}

#undef LOCTEXT_NAMESPACE

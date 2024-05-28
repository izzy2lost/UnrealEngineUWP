// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DSP/Filter.h"
#include "DSP/InterpolatedOnePole.h"
#include "SAudioSpectrumPlot.h"
#include "SMetasoundGraphNode.h"

namespace Metasound::Editor
{
	/**
	* Abstract class implementing shared functionality for an SGraphNode for any Metasound basic filter. Allows for displaying a frequency response plot of the filter.
	*/
	class SMetaSoundFilterGraphNode : public SMetaSoundGraphNode
	{
	public:
		virtual ~SMetaSoundFilterGraphNode();

	protected:
		// Update function for derived classes to update their state each frame. Implementations should return true if they have the required information to plot a frequency response, or false otherwise.
		virtual bool UpdateFilterParams() = 0;

		// Derived classes should apply the filter transfer function to each z-domain value in the given array (complex numbers given as interleaved floats).
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const = 0;

		// Derived classes can optionally add items to the spectrum plot menu by overriding this member function.
		virtual void ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder) {};

		// Begin SWidget overrides.
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		// End SWidget overrides.

		// Begin SGraphNode overrides.
		virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
		// End SGraphNode overrides.

		static const float SampleRate;
		
	private:
		FAudioPowerSpectrumData GetAudioSpectrumData();

		TSharedPtr<SAudioSpectrumPlot> FrequencyResponsePlot;
		TSharedPtr<const FExtensionBase> ContextMenuExtension;
		TArray<float> CenterFrequencies;
		TArray<float> SquaredMagnitudes;
		bool bHasFilterParams = false;
	};


	class SMetaSoundBiquadFilterGraphNode : public SMetaSoundFilterGraphNode
	{
	protected:
		virtual bool UpdateFilterParams() override;
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const override;

	private:
		Audio::FBiquadFilter Filter;
	};


	class SMetaSoundLadderFilterGraphNode : public SMetaSoundFilterGraphNode
	{
	public:
		SMetaSoundLadderFilterGraphNode();

	protected:
		virtual bool UpdateFilterParams() override;
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const override;

	private:
		Audio::FLadderFilter Filter;
	};


	class SMetaSoundOnePoleHighPassFilterGraphNode : public SMetaSoundFilterGraphNode
	{
	public:
		SMetaSoundOnePoleHighPassFilterGraphNode();

	protected:
		virtual bool UpdateFilterParams() override;
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const override;

	private:
		Audio::FInterpolatedHPF Filter;
	};


	class SMetaSoundOnePoleLowPassFilterGraphNode : public SMetaSoundFilterGraphNode
	{
	public:
		SMetaSoundOnePoleLowPassFilterGraphNode();

	protected:
		virtual bool UpdateFilterParams() override;
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const override;

	private:
		Audio::FInterpolatedLPF Filter;
	};


	class SMetaSoundStateVariableFilterGraphNode : public SMetaSoundFilterGraphNode
	{
	public:
		SMetaSoundStateVariableFilterGraphNode();

	protected:
		virtual bool UpdateFilterParams() override;
		virtual void ArrayCalculateFilterResponseInPlace(TArrayView<float> InOutComplexValues) const override;
		virtual void ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder) override;

	private:
		void BuildFilterOutputSubMenu(FMenuBuilder& SubMenu);

		static const FName LowPassFilter;
		static const FName HighPassFilter;
		static const FName BandPass;
		static const FName BandStop;

		Audio::FStateVariableFilter Filter;
		FName DisplayedFilterResponse;
	};
}

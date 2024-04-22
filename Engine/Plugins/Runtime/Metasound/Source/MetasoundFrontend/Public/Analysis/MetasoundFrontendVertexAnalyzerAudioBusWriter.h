// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "AudioDefines.h"
#include "Containers/Array.h"

namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVertexAnalyzerAudioBusWriter : public FVertexAnalyzerBase
		{
		public:
			static const FName& GetAnalyzerName();
			static const FName& GetDataType();
			static FName GetAnalyzerMemberName(const Audio::FDeviceId InDeviceID, const uint32 InAudioBusID);

			class METASOUNDFRONTEND_API FFactory : public TVertexAnalyzerFactory<FVertexAnalyzerAudioBusWriter>
			{
			public:
				virtual const TArray<FAnalyzerOutput>& GetAnalyzerOutputs() const override
				{
					static const TArray<FAnalyzerOutput> Outputs;
					return Outputs;
				}
			};

			FVertexAnalyzerAudioBusWriter(const FCreateAnalyzerParams& InParams);
			virtual ~FVertexAnalyzerAudioBusWriter() = default;

			virtual void Execute() override;

		private:
			struct FBusAddress
			{
				Audio::FDeviceId DeviceID = 0;
				uint32 AudioBusID = 0;

				FString ToString() const;
				static FBusAddress FromString(const FString& InAnalyzerMemberName);
			};

			Audio::FPatchInput AudioBusPatchInput;
		};
	} // namespace Frontend
} // namespace Metasound

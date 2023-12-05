// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlJsonUtilities.h"

class CAPTUREPROTOCOLSTACK_API FControlResponse
{
public:

    FControlResponse(FString InAddressPath);
    virtual ~FControlResponse() = default;

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody);

    const FString& GetAddressPath() const;

private:

    FString AddressPath;
};

class CAPTUREPROTOCOLSTACK_API FKeepAliveResponse final : public FControlResponse
{
public:

    FKeepAliveResponse();
};

class CAPTUREPROTOCOLSTACK_API FStartSessionResponse final : public FControlResponse
{
public:
    
    FStartSessionResponse();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    const FString& GetSessionId() const;

private:

    FString SessionId;
};

class CAPTUREPROTOCOLSTACK_API FStopSessionResponse final : public FControlResponse
{
public:

    FStopSessionResponse();
};

class CAPTUREPROTOCOLSTACK_API FGetServerInformationResponse final : public FControlResponse
{
public:
    
    FGetServerInformationResponse();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    const FString& GetId() const;
    const FString& GetName() const;
    const FString& GetModel() const;
    const FString& GetPlatformName() const;
    const FString& GetPlatformVersion() const;
    const FString& GetSoftwareName() const;
    const FString& GetSoftwareVersion() const;
	uint16 GetExportPort() const;

private:

    FString Id;
    FString Name;
    FString Model;
    FString PlatformName;
    FString PlatformVersion;
    FString SoftwareName;
    FString SoftwareVersion;
	uint16 ExportPort;
};

class CAPTUREPROTOCOLSTACK_API FSubscribeResponse final : public FControlResponse
{
public:

    FSubscribeResponse();
};

class CAPTUREPROTOCOLSTACK_API FUnsubscribeResponse final : public FControlResponse
{
public:

    FUnsubscribeResponse();
};


class CAPTUREPROTOCOLSTACK_API FGetStateResponse final : public FControlResponse
{
public:

    FGetStateResponse();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    bool IsRecording() const;
    const TSharedPtr<FJsonObject>& GetPlatformState() const;

private:

    bool bIsRecording;
    TSharedPtr<FJsonObject> PlatformState;
};

class CAPTUREPROTOCOLSTACK_API FStartRecordingTakeResponse final : public FControlResponse
{
public:

    FStartRecordingTakeResponse();
};

class CAPTUREPROTOCOLSTACK_API FStopRecordingTakeResponse final : public FControlResponse
{
public:

    FStopRecordingTakeResponse();

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	const FString& GetTakeName() const;
private:

	FString TakeName;
};

class CAPTUREPROTOCOLSTACK_API FAbortRecordingTakeResponse final : public FControlResponse
{
public:

	FAbortRecordingTakeResponse();
};

class CAPTUREPROTOCOLSTACK_API FGetTakeListResponse final : public FControlResponse
{
public:

    FGetTakeListResponse();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    const TArray<FString>& GetNames() const;

private:

    TArray<FString> Names;
};

class CAPTUREPROTOCOLSTACK_API FGetTakeMetadataResponse final : public FControlResponse
{
public:

    struct FFileObject
    {
        FString Name;
        uint64 Length;
    };

    struct FVideoObject
    {
		uint64 Frames;
        uint16 FrameRate;
        uint32 Height;
        uint32 Width;
    };

    struct FAudioObject
    {
        uint8 Channels;
        uint32 SampleRate;
        uint8 BitsPerChannel;
    };

    struct FTakeObject
    {
        FString Name;
        FString Slate;
        uint16 TakeNumber;
        FString DateTime;
        FString AppVersion;
        FString Model;
        FString Subject;
        FString Scenario;
        TArray<FString> Tags;

        TArray<FFileObject> Files;

        FVideoObject Video;
        FAudioObject Audio;
    };

    FGetTakeMetadataResponse();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    const TArray<FTakeObject>& GetTakes() const;

private:

    TProtocolResult<void> CreateTakeObject(const TSharedPtr<FJsonObject>& InTakeObject, FTakeObject& OutTake) const;
    TProtocolResult<void> CreateFileObject(const TSharedPtr<FJsonObject>& InFileObject, FFileObject& OutFile) const;

    TArray<FTakeObject> Takes;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_UWP

#include "HttpIXMLSupport.h"

// Default user agent string
static const WCHAR USER_AGENT[] = L"UE4HTTPIXML\r\n";


/**
 * IXML implementation of an Http request
 */
class FHttpRequestIXML : public IHttpRequest
{
public:

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpRequest

	virtual FString GetVerb() override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() override;

	/**
	 * Constructor
	 */
	FHttpRequestIXML();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FHttpRequestIXML();

private:

	uint32		CreateRequest();
	uint32		ApplyHeaders();
	uint32		SendRequest();
	void		FinishedRequest();
	void		CleanupRequest();

private:
	TMap<FString, FString>				Headers;
	TArray<uint8>						Payload;
	FString								URL;
	FString								Verb;
	FHttpRequestCompleteDelegate		CompleteDelegate;
	FHttpRequestProgressDelegate		RequestProgressDelegate;

	EHttpRequestStatus::Type			RequestStatus;
	float								ElapsedTime;

	ComPtr<IXMLHTTPRequest2>			XHR;
	ComPtr<IXMLHTTPRequest2Callback>	XHRCallback;
	ComPtr<HttpCallback>				HttpCB;
	ComPtr<RequestStream>				SendStream;

	TSharedPtr<class FHttpResponseIXML, ESPMode::ThreadSafe> Response;

	friend class FHttpResponseIXML;
};

/**
 * IXML implementation of an Http response
 */
class FHttpResponseIXML : public IHttpResponse
{

public:

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpResponse

	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;

	// FHttpResponseIXML

	FHttpResponseIXML(FHttpRequestIXML& InRequest, ComPtr<HttpCallback> InHttpCB);
	virtual ~FHttpResponseIXML();

	bool	Succeeded();

private:

	FHttpRequestIXML&				Request;
	FString							RequestURL;
	ComPtr<HttpCallback>			HttpCB;

};

#endif

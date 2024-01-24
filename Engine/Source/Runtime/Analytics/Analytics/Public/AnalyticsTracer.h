// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IAnalyticsTracer.h"

using FThreadId = uint32;

/**
* Implementation of an IAnalyticsSpan interface
*/
class FAnalyticsSpan : public IAnalyticsSpan
{
public:

	FAnalyticsSpan(){};
	~FAnalyticsSpan(){};

	// Public IAnalyticsSpan implementation
	virtual void SetProvider(TSharedPtr<IAnalyticsProvider> AnalyticsProvider) override;
	virtual void Start(const FName Name, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});
	virtual void End(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {}) override;
	virtual void AddAttributes(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes) override;
	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {}) override;
	virtual const FName& GetName() const override;
	virtual const TArray<FAnalyticsEventAttribute>& GetAttributes() const override;
	virtual uint32 GetScopeDepth() const override;
	virtual TSharedPtr<IAnalyticsSpan> GetParentSpan() const override;
	virtual double GetDuration() const override;
	virtual void AddChildSpan(TSharedPtr<IAnalyticsSpan> ChildSpan) override;

private:

	FName								Name = TEXT("None");
	FGuid								Guid = FGuid();
	FDateTime							StartTime = 0;
	FDateTime							EndTime = 0;
	FThreadId							ThreadId = 0;
	uint32								ScopeDepth = 0;
	double								Duration = 0;
	bool								IsActive = false;
	TSharedPtr<IAnalyticsProvider>		AnalyticsProvider;
	TArray<FAnalyticsEventAttribute>	Attributes;
	TWeakPtr<IAnalyticsSpan>			ParentSpan;
	TArray<TSharedPtr<IAnalyticsSpan>>	ChildSpans;
};

/**
* Implementation of an IAnalyticsTracer interface
*/
class FAnalyticsTracer : public IAnalyticsTracer
{
public:

	FAnalyticsTracer(){};
	~FAnalyticsTracer(){};
	
	// Public IAnalyticsTracer implementation
	virtual void StartSession() override;
	virtual void EndSession() override;
	virtual void SetProvider(TSharedPtr<IAnalyticsProvider> AnalyticsProvider) override;
	virtual TSharedPtr<IAnalyticsSpan> StartSpan(const FName Name, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {}) override;
	virtual bool EndSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {}) override;
	virtual TSharedPtr<IAnalyticsSpan> GetCurrentSpan() const override;
	virtual TSharedPtr<IAnalyticsSpan> GetSessionSpan() const override;
	virtual TSharedPtr<IAnalyticsSpan> GetSpan(const FName Name) override;

protected:

	// Protected IAnalyticsTracer implementation
	virtual void SetCurrentSpan(TSharedPtr<IAnalyticsSpan> Span) override;

private:

	TSharedPtr<IAnalyticsSpan> StartSpanInternal(const FName Name, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes);
	bool EndSpanInternal(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes);
	TSharedPtr<IAnalyticsSpan> GetSpanInternal(const FName Name);

	TSharedPtr<IAnalyticsProvider>			AnalyticsProvider;
	TSharedPtr<IAnalyticsSpan>				SessionSpan;
	TSharedPtr<IAnalyticsSpan>				CurrentSpan;
	TMap<FName,TWeakPtr<IAnalyticsSpan>>	SpanRegistry;
	FCriticalSection						CriticalSection;
};



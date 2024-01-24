// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsTracer.h"
#include "ProfilingDebugging/MiscTrace.h"

UE_DISABLE_OPTIMIZATION_SHIP
 
static void AggregateAttributes(TArray<FAnalyticsEventAttribute>& AggregatedAttibutes, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// Aggregates all attributes
	for (const FAnalyticsEventAttribute& Attribute : Attributes)
	{
		bool AttributeWasFound = false;

		for (FAnalyticsEventAttribute& AggregatedAttribute : AggregatedAttibutes)
		{
			if (Attribute.GetName() == AggregatedAttribute.GetName())
			{
				AggregatedAttribute += Attribute;

				// If we already have this attribute then great no more to do for this attribute
				AttributeWasFound = true;
				break;
			}
		}

		if (AttributeWasFound == false)
		{
			// No matching attribute so append
			AggregatedAttibutes.Add(Attribute);
		}
	}
}

void FAnalyticsSpan::SetProvider(TSharedPtr<IAnalyticsProvider> Provider)
{
	AnalyticsProvider = Provider;
}

double FAnalyticsSpan::GetDuration() const 
{
	return Duration;
}

void FAnalyticsSpan::Start(const FName NewSpanName, TSharedPtr<IAnalyticsSpan> NewSpanParent, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	// Create a new Guid for this flow, can we assume it is unique?
	Name			= NewSpanName;
	Guid			= FGuid::NewGuid();
	Attributes		= AdditionalAttributes;
	ThreadId		= FPlatformTLS::GetCurrentThreadId();
	StartTime		= FDateTime::UtcNow();
	EndTime			= FDateTime::UtcNow();
	Duration		= 0;
	ScopeDepth		= NewSpanParent.IsValid()? NewSpanParent->GetScopeDepth() + 1 : 0;
	ParentSpan		= NewSpanParent;
	IsActive		= true;

	TRACE_BEGIN_REGION(*Name.ToString());
}

void FAnalyticsSpan::AddChildSpan(TSharedPtr<IAnalyticsSpan> ChildSpan)
{
	ChildSpans.Add(ChildSpan);
}

void FAnalyticsSpan::End(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	// Only End the span once
	if (IsActive == false)
	{
		return;
	}

	// Calculate the duration
	EndTime = FDateTime::UtcNow();
	Duration = (EndTime - StartTime).GetTotalSeconds();
		
	TRACE_END_REGION(*Name.ToString());

	// Append the parent attributes and the the additional attributes to the current span attributes, these will get passed down to the child spans
	AddAttributes(AdditionalAttributes);

	TSharedPtr< IAnalyticsSpan> ParentSpanShared = ParentSpan.Pin();

	if (ParentSpanShared.IsValid())
	{
		AddAttributes(ParentSpanShared->GetAttributes());
	}

	const uint32 SpanSchemaVersion = 1;
	const FString SpanEventName = TEXT("Span");

	TArray<FAnalyticsEventAttribute> EventAttributes = Attributes;

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SchemaVersion"), SpanSchemaVersion));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_Name"), Name.ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_GUID"), Guid.ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_ParentName"), ParentSpanShared.IsValid() ? ParentSpanShared->GetName().ToString() : TEXT("")));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_ThreadId"), ThreadId));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_Depth"), ScopeDepth));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_StartUTC"), StartTime.ToUnixTimestampDecimal()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_EndUTC"), EndTime.ToUnixTimestampDecimal()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Span_TimeInSec"), Duration));

	// Make sure we end all the child spans 
	for (TSharedPtr<IAnalyticsSpan> ChildSpan : ChildSpans)
	{
		ChildSpan->End(Attributes);
	}

	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->RecordEvent(SpanEventName, EventAttributes);
	}

	IsActive = false;
}

void FAnalyticsSpan::AddAttributes(const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	AggregateAttributes(Attributes, AdditionalAttributes);
}

void FAnalyticsSpan::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	if (AnalyticsProvider.IsValid())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes = Attributes;
		AggregateAttributes(EventAttributes, AdditionalAttributes);
		AnalyticsProvider->RecordEvent(EventName, EventAttributes);
	}
}

const FName& FAnalyticsSpan::GetName() const
{
	return Name;
}

const TArray<FAnalyticsEventAttribute>& FAnalyticsSpan::GetAttributes() const
{
	return Attributes;
}

uint32 FAnalyticsSpan::GetScopeDepth() const
{
	return ScopeDepth;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsSpan::GetParentSpan() const
{
	return ParentSpan.Pin();
}

void FAnalyticsTracer::SetProvider(TSharedPtr<IAnalyticsProvider> InProvider)
{
	AnalyticsProvider = InProvider;
}

void FAnalyticsTracer::SetCurrentSpan(TSharedPtr<IAnalyticsSpan> Span)
{
	CurrentSpan = Span;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetCurrentSpan() const
{
	return CurrentSpan;
}

void FAnalyticsTracer::StartSession()
{
	SessionSpan = StartSpan(TEXT("Session"), TSharedPtr<IAnalyticsSpan>());
	SetCurrentSpan(SessionSpan);
}

void FAnalyticsTracer::EndSession()
{
	FScopeLock ScopeLock(&CriticalSection);	
	EndSpan(SessionSpan);
	SessionSpan.Reset();
	AnalyticsProvider.Reset();
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::StartSpan(const FName NewSpanName, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	return StartSpanInternal(NewSpanName, ParentSpan.IsValid()? ParentSpan : GetCurrentSpan(), AdditionalAttributes);
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::StartSpanInternal(const FName NewSpanName, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	TSharedPtr<IAnalyticsSpan> NewSpan = MakeShared<FAnalyticsSpan>();
	
	NewSpan->SetProvider(AnalyticsProvider);
	NewSpan->Start(NewSpanName, ParentSpan, AdditionalAttributes);

	if (ParentSpan.IsValid())
	{
		ParentSpan->AddChildSpan(NewSpan);
	}

	SpanRegistry.Emplace(NewSpanName, NewSpan);

	SetCurrentSpan(NewSpan);

	return NewSpan;
}

bool FAnalyticsTracer::EndSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	return EndSpanInternal(Span, AdditionalAttributes);
}

bool FAnalyticsTracer::EndSpanInternal(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	if (Span.IsValid())
	{
		Span->End(AdditionalAttributes);

		SetCurrentSpan(Span->GetParentSpan());

		SpanRegistry.Remove(Span->GetName());

		return true;
	}

	return false;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSessionSpan() const
{
	return SessionSpan;
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSpanInternal(const FName Name)
{
	TWeakPtr<IAnalyticsSpan>* SpanWeakPtr = SpanRegistry.Find(Name);
	return SpanWeakPtr != nullptr ? (*SpanWeakPtr).Pin() : TSharedPtr<IAnalyticsSpan>();
}

TSharedPtr<IAnalyticsSpan> FAnalyticsTracer::GetSpan(const FName Name)
{
	FScopeLock ScopeLock(&CriticalSection);
	return GetSpanInternal(Name);
}

UE_ENABLE_OPTIMIZATION_SHIP
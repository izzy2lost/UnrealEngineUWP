// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef UE_DNLD_SANDBOX

#include "IOS/IOSBackgroundURLSessionHandler.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"

#include "IOS/IOSAppDelegate.h"

#endif

#include <future>

// Force cancel all pending downloads.
// Useful when testing background downloads as they persist between application sessions.
static constexpr bool bCancelExistingDownloads = false;

#define UE_DNLD_LOG(...) NSLog(@"UEBackgroundDownload " __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------

// We need additional state in NSURLSessionDownloadTask to implement CDN failover.
// Convential way to implement this would be to set a delegate on NSURLSessionDownloadTask and add properties to the delegate.
//
// In case of background downloads API prohibits setting a delegate or setting NSObject values.
// Additionally we want to keep state in NSURLSessionDownloadTask itself to avoid pitfalls of maintaining separate state elsewhere.
//
// [NSURLSessionDownloadTask taskDescription] is one such property that can be used to track state and API maintains state between app sessions.
@interface FBackgroundNSURLSessionDownloadTaskData : NSObject

@property (nonatomic, retain) NSMutableArray<__kindof NSURL*>* URLs;
@property (nonatomic, retain) NSMutableArray<__kindof NSNumber*>* RetryCountPerURL;

// assumes all URL's have same content path but different domain
+ (FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskDataWithURLs:(NSArray<__kindof NSURL*>*)URLs WithRetryCount:(NSInteger)RetryCount;
+ (FBackgroundNSURLSessionDownloadTaskData* _Nullable)TaskDataFromSerializedString:(NSString*)SerializedData;

- (NSString*)ToSerializedString;

- (NSURL* _Nonnull)GetFirstURL;
- (NSURL* _Nullable)GetNextURL;
- (void)ResetRetryCount:(NSInteger)RetryCount;
- (void)Cancel;

@end

// --------------------------------------------------------------------------------------------------------------------

@implementation FBackgroundNSURLSessionDownloadTaskData

NSString* const SerializationKeyProtocolVersion = @"v";
NSString* const SerializationKeyCDNs = @"c";
NSString* const SerializationKeyPath = @"p";
NSString* const SerializationKeyRetryCountPerURL = @"r";

+ (FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskDataWithURLs:(NSArray<__kindof NSURL*>*)URLs WithRetryCount:(NSInteger)RetryCount
{
	FBackgroundNSURLSessionDownloadTaskData* Data = [[FBackgroundNSURLSessionDownloadTaskData alloc] init];

	Data.URLs = [NSMutableArray arrayWithArray:URLs];
	Data.RetryCountPerURL = [NSMutableArray arrayWithCapacity:URLs.count];
	for (NSURL* URL in URLs)
	{
		(void)URL;
		[Data.RetryCountPerURL addObject:[NSNumber numberWithInteger:RetryCount]];
	}
	NSAssert(Data.URLs.count > 0, @"URLs should be non empty");
	NSAssert(Data.URLs.count == Data.RetryCountPerURL.count, @"URLs and RetryCountPerURL arrays should have same size");

	return [Data autorelease];
}

+ (FBackgroundNSURLSessionDownloadTaskData* _Nullable)TaskDataFromSerializedString:(NSString*)SerializedData
{
	FBackgroundNSURLSessionDownloadTaskData* Data = [[FBackgroundNSURLSessionDownloadTaskData alloc] init];

	NSError* Error = nil;
	NSDictionary* Dict = [NSJSONSerialization JSONObjectWithData:[SerializedData dataUsingEncoding:NSUTF8StringEncoding] options:0 error:&Error];

	NSNumber* Version = [Dict valueForKey:SerializationKeyProtocolVersion];
	NSMutableArray<__kindof NSString*>* CDNs = [Dict valueForKey:SerializationKeyCDNs];
	NSString* Path = [Dict valueForKey:SerializationKeyPath];
	NSArray<__kindof NSNumber*>* RetryCountPerURL = [Dict valueForKey:SerializationKeyRetryCountPerURL];

	if (Error != nil || Version == nil || Version.intValue != 1 || CDNs == nil || Path == nil || RetryCountPerURL == nil)
	{
		UE_DNLD_LOG(@"Failed to deserialize task state '%@' due to '%@', %u, %u, %u, %u, %u",
			SerializedData,
			Error != nil ? Error.localizedDescription : @"",
			Version == nil ? 1 : 0,
			Version.intValue,
			CDNs == nil ? 1 : 0,
			Path == nil ? 1 : 0,
			RetryCountPerURL == nil ? 1 : 0
		);
		[Data release];
		return nil;
	}

	Data.URLs = [NSMutableArray arrayWithCapacity:CDNs.count];
	Data.RetryCountPerURL = [NSMutableArray arrayWithArray:RetryCountPerURL];
	for (NSString* CDN in CDNs)
	{
		[Data.URLs addObject:[NSURL URLWithString:[NSString stringWithFormat:@"%@%@", CDN, Path]]];
	}
	NSAssert(Data.URLs.count > 0, @"URLs should be non empty");
	NSAssert(Data.URLs.count == Data.RetryCountPerURL.count, @"URLs and RetryCountPerURL arrays should have same size");

	return [Data autorelease];
}

- (void)dealloc
{
	[_URLs release];
	[_RetryCountPerURL release];
	[super dealloc];
}

- (NSString *)ToSerializedString
{
	NSMutableDictionary* Dict = [NSMutableDictionary dictionaryWithCapacity:4];

	NSString* Path = [[self.URLs firstObject] path];

	NSMutableArray<__kindof NSString*>* CDNs = [NSMutableArray arrayWithCapacity:self.URLs.count];
	for (NSURL* URL in self.URLs)
	{
		NSString* URLString = URL.absoluteString;
		if (![URLString hasSuffix:Path])
		{
			UE_DNLD_LOG(@"Expected all URLs have same path but got '%@' when expected path '%@'", URLString, Path);
			continue;
		}
		NSString* Domain = [[URLString componentsSeparatedByString:URL.path] firstObject];
		[CDNs addObject:Domain];
	}

	[Dict setValue:[NSNumber numberWithInt:1] forKey:SerializationKeyProtocolVersion];
	[Dict setValue:CDNs forKey:SerializationKeyCDNs];
	[Dict setValue:Path forKey:SerializationKeyPath];
	[Dict setValue:self.RetryCountPerURL forKey:SerializationKeyRetryCountPerURL];

	NSError* Error = nil;
	NSData* Data = [NSJSONSerialization dataWithJSONObject:Dict options:NSJSONWritingSortedKeys error:&Error];

	NSString* String = [[NSString alloc] initWithData:Data encoding:NSUTF8StringEncoding];
	return [String autorelease];
}

- (NSURL* _Nonnull)GetFirstURL
{
	return self.URLs.firstObject;
}

- (NSURL* _Nullable)GetNextURL
{
	for (NSUInteger i = 0; i < self.RetryCountPerURL.count; ++i)
	{
		NSUInteger RetryValue = [self.RetryCountPerURL objectAtIndex:i].integerValue;
		if (RetryValue == 0)
		{
			continue;
		}
		else if (RetryValue <= -1) // special case for infinitely retrying same URL
		{
			return [self.URLs objectAtIndex:i];
		}

		RetryValue--;
		[self.RetryCountPerURL replaceObjectAtIndex:i withObject:[NSNumber numberWithInteger:RetryValue]];
		return [self.URLs objectAtIndex:i];
	}

	return nil;
}

- (void)ResetRetryCount:(NSInteger)RetryCount
{
	for (NSUInteger i = 0; i < self.RetryCountPerURL.count; ++i)
	{
		[self.RetryCountPerURL replaceObjectAtIndex:i withObject:[NSNumber numberWithInteger:RetryCount]];
	}
}

- (void)Cancel
{
	[self ResetRetryCount:0];
}

@end

// --------------------------------------------------------------------------------------------------------------------

// NSURLSession wrapper focused on background downloading and CDN failover
@interface FBackgroundNSURLSession : NSObject<NSURLSessionDelegate, NSURLSessionTaskDelegate, NSURLSessionDownloadDelegate>

@property (atomic) BOOL AllowCellular;

+ (FBackgroundNSURLSession*)Shared;
+ (NSUInteger)GetInvalidDownloadId;
+ (NSString*)GetNSURLSessionIdentifier;

- (void)Initialize;
- (void)SetFileHashHelper:(BackgroundHttpFileHashHelperRef)HelperRef;
- (BackgroundHttpFileHashHelperRef)GetFileHashHelper;
- (NSString*)GetTempPathForURL:(NSURL* _Nonnull)URL;
- (NSURLSessionDownloadTask*)CreateDownloadForURL:(NSURL* _Nonnull)URL WithPriority:(float)Priority WithTaskData:(FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskData;
- (NSURLSessionDownloadTask*)CreateDownloadForResumeData:(NSData* _Nonnull)ResumeData WithPriority:(float)Priority WithTaskData:(FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskData;

- (NSUInteger)CreateOrFindDownloadForURLs:(NSArray<__kindof NSString*>*)URLStrings WithPriority:(float)Priority;
- (void)PauseDownload:(NSUInteger)DownloadId;
- (void)ResumeDownload:(NSUInteger)DownloadId;
- (void)CancelDownload:(NSUInteger)DownloadId;
- (void)SetPriority:(float)Priority ForDownload:(NSUInteger)DownloadId;
- (void)SetCurrentDownloadedBytes:(uint64)DownloadedBytes ForTask:(NSURLSessionDownloadTask*)Task;
- (uint64)GetCurrentDownloadedBytes:(NSUInteger)DownloadId;
- (void)RecreateDownloads;

- (NSURLSessionDownloadTask*)FindDownloadTaskFor:(NSUInteger)DownloadId;
- (NSUInteger)FindDownloadIdForTask:(NSURLSessionDownloadTask*)Task;
- (NSUInteger)EnsureTaskIsTracked:(NSURLSessionDownloadTask*)Task;
- (void)ReplaceTrackedTaskWith:(NSURLSessionDownloadTask*)NewTask ForDownloadId:(NSUInteger)DownloadId;
- (void)EnsureTaskIsNotTracked:(NSURLSessionDownloadTask*)Task;

- (void)SetDownloadResult:(NSInteger)HTTPCode WithTempFile:(NSString*)TempFile ForDownload:(NSURLSessionDownloadTask*)Task;
- (NSString* _Nullable)GetDownloadResult:(NSUInteger)DownloadId OutStatus:(BOOL* _Nonnull)OutStatus OutStatusCode:(NSInteger*)OutStatusCode;

// From NSURLSessionDelegate
- (void)URLSessionDidFinishEventsForBackgroundURLSession:(NSURLSession*)Session;

// From NSURLSessionTaskDelegate
- (void)URLSession:(NSURLSession*)Session task:(NSURLSessionTask*)Task didCompleteWithError:(NSError*)Error;
//- (void)URLSession:(NSURLSession *)session taskIsWaitingForConnectivity:(NSURLSessionTask *)Task;
//- (void)URLSession:(NSURLSession *)Session task:(NSURLSessionTask *)Task willBeginDelayedRequest:(NSURLRequest *)Request completionHandler:(void (^)(NSURLSessionDelayedRequestDisposition Disposition, NSURLRequest* NewRequest))CompletionHandler;

// From NSURLSessionDownloadDelegate
- (void)URLSession:(NSURLSession*)Session downloadTask:(NSURLSessionDownloadTask*)Task didFinishDownloadingToURL:(NSURL*)Location;
- (void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)Task didWriteData:(int64_t)BytesWritten totalBytesWritten:(int64_t)TotalBytesWritten totalBytesExpectedToWrite:(int64_t)TotalBytesExpectedToWrite;

@end

// --------------------------------------------------------------------------------------------------------------------

@implementation FBackgroundNSURLSession
{
	NSURLSession* _Session;
	NSMutableDictionary<__kindof NSNumber*, __kindof NSURLSessionDownloadTask*>* _AllDownloads;
	NSUInteger _NextDownloadId;
	std::promise<void> _AllDownloadsPromise;
	std::future<void> _AllDownloadsFuture;
	BackgroundHttpFileHashHelperPtr _HelperPtr;
	int32 RetryResumeDataLimit;
}

static constexpr NSUInteger InvalidDownloadId = 0;

NSString* const NSURLSessionIdentifier = @"com.epicgames.backgrounddownloads";

NSProgressUserInfoKey const NSProgressDownloadCompletedBytes = @"com.epicgames.nsprogress.completedbytes";
NSProgressUserInfoKey const NSProgressDownloadResultStatusCode = @"com.epicgames.nsprogress.resultstatuscode";
NSProgressUserInfoKey const NSProgressDownloadResultTempFilePath = @"com.epicgames.nsprogress.tempfilepath";

static constexpr NSInteger HTTPStatusCodeSuccessCreated = 201;
static constexpr NSInteger HTTPStatusCodeErrorBadRequest = 500;
static constexpr NSInteger HTTPStatusCodeErrorServer = 500;

+ (FBackgroundNSURLSession*)Shared
{
	static FBackgroundNSURLSession* Shared = nil;
	static dispatch_once_t Once;
	dispatch_once(&Once, ^{
		Shared = [[self alloc] init];
	});
	return Shared;
}

+ (NSUInteger)GetInvalidDownloadId;
{
	return InvalidDownloadId;
}

+ (NSString*)GetNSURLSessionIdentifier
{
	return NSURLSessionIdentifier;
}

- (id)init
{
	if (self = [super init])
	{
		[self Initialize];
	}
	return self;
}

- (void)Initialize
{
	bool bUseForegroundSession = false;
	bool bDiscretionary = false;
	bool bShouldSendLaunchEvents = true;
	int32 MaximumConnectionsPerHost = 6;
	double TimeoutIntervalForRequest = 120.0; // Note, ignored in background sessions (if bUseForegroundSession is false).
	double TimeoutIntervalForResource = 60.0 * 60.0;
	RetryResumeDataLimit = 3;

#ifndef UE_DNLD_SANDBOX
	GConfig->GetBool(TEXT("BackgroundHttp.iOSSettings"), TEXT("bUseForegroundSession"), bUseForegroundSession, GEngineIni);
	GConfig->GetBool(TEXT("BackgroundHttp.iOSSettings"), TEXT("bDiscretionary"), bDiscretionary, GEngineIni);
	GConfig->GetBool(TEXT("BackgroundHttp.iOSSettings"), TEXT("bShouldSendLaunchEvents"), bShouldSendLaunchEvents, GEngineIni);
	GConfig->GetInt(TEXT("BackgroundHttp"), TEXT("MaxActiveDownloads"), MaximumConnectionsPerHost, GEngineIni);
	GConfig->GetDouble(TEXT("BackgroundHttp.iOSSettings"), TEXT("BackgroundReceiveTimeout"), TimeoutIntervalForRequest, GEngineIni);
	GConfig->GetDouble(TEXT("BackgroundHttp.iOSSettings"), TEXT("BackgroundHttpResourceTimeout"), TimeoutIntervalForResource, GEngineIni);
	GConfig->GetInt(TEXT("BackgroundHttp.iOSSettings"), TEXT("RetryResumeDataLimit"), RetryResumeDataLimit, GEngineIni);
#endif
	
	_AllDownloads = [NSMutableDictionary new];
	_AllDownloadsFuture = _AllDownloadsPromise.get_future();
	_NextDownloadId = InvalidDownloadId + 1;

	// Never allow cellular unless we get explicit opt-in from the user.
	self.AllowCellular = NO;

	NSURLSessionConfiguration* Configuration = bUseForegroundSession ?
		[NSURLSessionConfiguration defaultSessionConfiguration] :
		[NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier: NSURLSessionIdentifier];

	// iOS will schedule downloads on it's own if true,
	// otherwise all downloads will be scheduled ASAP if false
	Configuration.discretionary = bDiscretionary;

	// In case if our app gets killed in background, iOS will launch it and report finished downloads via handleEventsForBackgroundURLSession.
	// This will help us to retry/fail-over downloads in background without waiting for user to open the game again.
	// Note that this behavior can be disabled via Background App Refresh set to No in iOS settings.
	Configuration.sessionSendsLaunchEvents = bShouldSendLaunchEvents;

	// Set session to allow cellular and instead control this on NSMutableURLRequest level because this value cannot be changed after session is created.
	Configuration.allowsCellularAccess = YES;

	Configuration.networkServiceType = bUseForegroundSession ? NSURLNetworkServiceTypeDefault : NSURLNetworkServiceTypeBackground;

	// TODO Is this any use for us? Needs entitlement.
	//Configuration.multipathServiceType = NSURLSessionMultipathServiceTypeAggregate;

	Configuration.HTTPMaximumConnectionsPerHost = MaximumConnectionsPerHost;

	Configuration.timeoutIntervalForRequest = TimeoutIntervalForRequest;

	Configuration.timeoutIntervalForResource = TimeoutIntervalForResource;

	_Session = [[NSURLSession sessionWithConfiguration:Configuration delegate:self delegateQueue:nil] retain];
	UE_DNLD_LOG(@"sessionWithConfiguration '%@'", Configuration.identifier);

	[_Session getTasksWithCompletionHandler:^(NSArray<__kindof NSURLSessionDataTask*>*, NSArray<__kindof NSURLSessionUploadTask*>*, NSArray<__kindof NSURLSessionDownloadTask*>* Downloads)
	{
		UE_DNLD_LOG(@"getTasksWithCompletionHandler block with %lu tasks", (unsigned long)[Downloads count]);
		
		if (Downloads == nil)
		{
			for (NSURLSessionDownloadTask* ExistingTask in Downloads)
			{
				const bool bCanRestartTask =
					(ExistingTask.state == NSURLSessionTaskStateRunning) ||
					(ExistingTask.state == NSURLSessionTaskStateSuspended);

				if (!bCanRestartTask)
				{
					UE_DNLD_LOG(@"Skipping tracking for existing download task with taskIdentifier %lu because it's not in resumable state", ExistingTask.taskIdentifier);
					continue;
				}

				const NSUInteger DownloadId = [self EnsureTaskIsTracked:ExistingTask];

				if (bCancelExistingDownloads)
				{
					UE_DNLD_LOG(@"Canceling existing download task with taskIdentifier %lu", ExistingTask.taskIdentifier);
					[self CancelDownload:DownloadId];
				}
			}
		}
		

		_AllDownloadsPromise.set_value();
	}];

	const FString& DirectoryPath = FBackgroundHttpFileHashHelper::GetTemporaryRootPath();
	if (ensureAlwaysMsgf(!DirectoryPath.IsEmpty(), TEXT("Invalid FBackgroundHttpFileHashHelper::GetTemporaryRootPath()")))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (bCancelExistingDownloads)
		{
			PlatformFile.DeleteDirectory(*DirectoryPath);
		}

		PlatformFile.CreateDirectory(*DirectoryPath);

		if (!PlatformFile.DirectoryExists(*DirectoryPath))
		{
			ensureAlwaysMsgf(false, TEXT("Failed to create temporary directory for background downloads"));
		}
	}
}

- (void)dealloc
{
	[_Session release];
	[_AllDownloads release];

	[super dealloc];
}

- (void)SetFileHashHelper:(BackgroundHttpFileHashHelperRef)HelperRef
{
	_HelperPtr = HelperRef.ToSharedPtr();
}

- (BackgroundHttpFileHashHelperRef)GetFileHashHelper
{
	// initialize a new instance in case if we get here from handleEventsForBackgroundURLSession
	if (!_HelperPtr.IsValid())
	{
		_HelperPtr = MakeShared<FBackgroundHttpFileHashHelper, ESPMode::ThreadSafe>();
		_HelperPtr->LoadData();
	}

	return _HelperPtr.ToSharedRef();
}

- (NSString*)GetTempPathForURL:(NSURL* _Nonnull)URL
{
	BackgroundHttpFileHashHelperRef HelperRef = [self GetFileHashHelper];

	const FString TaskURL(URL.absoluteString);
	const FString& TempFileName = HelperRef->FindOrAddTempFilenameMappingForURL(TaskURL);
	const FString& DestinationPath = HelperRef->GetFullPathOfTempFilename(TempFileName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString ConvertedPath = PlatformFile.ConvertToAbsolutePathForExternalAppForWrite(*DestinationPath);

	return ConvertedPath.GetNSString();
}

- (NSURLSessionDownloadTask*)CreateDownloadForURL:(NSURL* _Nonnull)URL WithPriority:(float)Priority WithTaskData:(FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskData;
{
	NSMutableURLRequest* URLRequest = [NSMutableURLRequest requestWithURL:URL];
	URLRequest.allowsCellularAccess = self.AllowCellular;

	NSURLSessionDownloadTask* Task = [_Session downloadTaskWithRequest:URLRequest];
	[Task setPriority:Priority];
	[Task setTaskDescription:[TaskData ToSerializedString]];

	UE_DNLD_LOG(@"CreateDownloadForURL '%@' with taskIdentifier %lu", Task.taskDescription, Task.taskIdentifier);
	return Task;
}

- (NSURLSessionDownloadTask*)CreateDownloadForResumeData:(NSData* _Nonnull)ResumeData WithPriority:(float)Priority WithTaskData:(FBackgroundNSURLSessionDownloadTaskData* _Nonnull)TaskData
{
	NSURLSessionDownloadTask* Task = [_Session downloadTaskWithResumeData:ResumeData];
	[Task setPriority:Priority];
	[Task setTaskDescription:[TaskData ToSerializedString]];

	UE_DNLD_LOG(@"CreateDownloadForResumeData '%@' with taskIdentifier %lu", Task.taskDescription, Task.taskIdentifier);
	return Task;
}

- (NSUInteger)CreateOrFindDownloadForURLs:(NSArray<__kindof NSString*>*)URLStrings WithPriority:(float)Priority
{
	if (_AllDownloadsFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		UE_DNLD_LOG(@"Starting wait for existing downloads status");
		
		_AllDownloadsFuture.wait();

		UE_DNLD_LOG(@"Done waiting for existing downloads status");
	}

	// To be able to store less state we assume all strings have same asset path suffix
	NSString* AssetPath = nil;
	NSMutableArray<__kindof NSURL*>* URLs = [NSMutableArray arrayWithCapacity:URLStrings.count];
	for (NSString* URLString in URLStrings)
	{
		NSURL* URLValue = [NSURL URLWithString:URLString];
		if (AssetPath == nil)
		{
			AssetPath = URLValue.path;
		}
		if (![URLValue.absoluteString hasSuffix:AssetPath])
		{
			UE_DNLD_LOG(@"Expected all URLs have same path but got '%@' when expected path '%@'", URLString, AssetPath);
		}
		[URLs addObject:URLValue];
	}

	// Serialize current settings
	FBackgroundNSURLSessionDownloadTaskData* TaskData = [FBackgroundNSURLSessionDownloadTaskData TaskDataWithURLs:URLs WithRetryCount:RetryResumeDataLimit];

	// Check for existing download task, could be from previous app session.
	UE_DNLD_LOG(@"Trying to find existing download for asset path '%@'", AssetPath);
	if (AssetPath != nil)
	{
		__block NSUInteger ExistingDownloadId = InvalidDownloadId;
		__block NSURLSessionDownloadTask* ExistingTask = nil;
		[_AllDownloads enumerateKeysAndObjectsUsingBlock:^(NSNumber* IterKey, NSURLSessionDownloadTask* IterTask, BOOL* IterStop)
		{
			if (IterTask.originalRequest != nil && [IterTask.originalRequest.URL.path isEqualToString:AssetPath])
			{
				ExistingDownloadId = IterKey.unsignedIntegerValue;
				ExistingTask = [IterTask retain]; // Retain in case if task gets killed in another thread.
				*IterStop = YES;
			}
		}];
		
		if (ExistingDownloadId != InvalidDownloadId && ExistingTask != nil)
		{
			UE_DNLD_LOG(@"Found existing download task for path '%@' with DownloadId %lu", AssetPath, ExistingDownloadId);

			// Update existing task state to new one, to reset retry counters, cdn links, etc.
			[ExistingTask setTaskDescription:[TaskData ToSerializedString]];

			[ExistingTask resume]; // Resume task just in case if it was not running before.
			[ExistingTask release];

			return ExistingDownloadId;
		}
	}

	NSURL* URL = [TaskData GetFirstURL];
	NSURLSessionDownloadTask* Task = [self CreateDownloadForURL:URL WithPriority:Priority WithTaskData:TaskData];
	const NSUInteger DownloadId = [self EnsureTaskIsTracked:Task];
	[Task resume];

	return DownloadId;
}

- (void)PauseDownload:(NSUInteger)DownloadId
{
	UE_DNLD_LOG(@"PauseDownload for DownloadId %lu", DownloadId);

	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task != nil)
	{
		[Task suspend];
	}
}

- (void)ResumeDownload:(NSUInteger)DownloadId
{
	UE_DNLD_LOG(@"ResumeDownload for DownloadId %lu", DownloadId);

	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task != nil)
	{
		[Task resume];
	}
}

- (void)CancelDownload:(NSUInteger)DownloadId
{
	UE_DNLD_LOG(@"CancelDownload for DownloadId %lu", DownloadId);

	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task == nil)
	{
		return;
	}

	FBackgroundNSURLSessionDownloadTaskData* TaskData = [FBackgroundNSURLSessionDownloadTaskData TaskDataFromSerializedString:Task.taskDescription];
	if (TaskData != nil)
	{
		// Remove task data from this download task, otherwise didCompleteWithError might retry the request.
		[TaskData Cancel];
		Task.taskDescription = [TaskData ToSerializedString];
	}

	// We're done with this task.
	[self EnsureTaskIsNotTracked:Task];

	// Will invoke didCompleteWithError if task is incomplete.
	[Task cancel];
}

- (void)SetPriority:(float)Priority ForDownload:(NSUInteger)DownloadId
{
	UE_DNLD_LOG(@"SetPriority %f for DownloadId %lu", Priority, DownloadId);

	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task != nil)
	{
		[Task setPriority:Priority];
	}
}

- (void)SetCurrentDownloadedBytes:(uint64)DownloadedBytes ForTask:(NSURLSessionDownloadTask*)Task
{
	if (Task != nil)
	{
		[Task.progress setUserInfoObject:[NSNumber numberWithLongLong:DownloadedBytes] forKey:NSProgressDownloadCompletedBytes];
	}
}

- (uint64)GetCurrentDownloadedBytes:(NSUInteger)DownloadId
{
	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task != nil)
	{
		NSNumber* CompletedBytes = [Task.progress.userInfo objectForKey:NSProgressDownloadCompletedBytes];
		if (CompletedBytes != nil)
		{
			return CompletedBytes.unsignedLongLongValue;
		}
	}

	return 0;
}

- (void)RecreateDownloads
{
	UE_DNLD_LOG(@"RecreateDownloads started");

	// Copy keys to avoid deadlocking in case if cancel/resume/etc will call delegates in-place
	NSArray<__kindof NSNumber*>* AllKeys = nil;
	@synchronized(_AllDownloads)
	{
		AllKeys = [_AllDownloads allKeys];
	}

	for (NSNumber* IterKey in AllKeys)
	{
		const NSUInteger DownloadId = IterKey.unsignedIntegerValue;
		NSURLSessionDownloadTask* OldTask = [_AllDownloads objectForKey:IterKey];
		const float OldTaskPriority = OldTask.priority;
		const NSURLSessionTaskState OldTaskState = OldTask.state;

		FBackgroundNSURLSessionDownloadTaskData* NewTaskData = [FBackgroundNSURLSessionDownloadTaskData TaskDataFromSerializedString:OldTask.taskDescription];
		if (NewTaskData == nil)
		{
			continue;
		}

		// Cancel old task
		[self CancelDownload:DownloadId];

		// Start a new task
		[NewTaskData ResetRetryCount:RetryResumeDataLimit];
		NSURLSessionDownloadTask* NewTask = [self CreateDownloadForURL:[NewTaskData GetFirstURL] WithPriority:OldTaskPriority WithTaskData:NewTaskData];
		[self ReplaceTrackedTaskWith:NewTask ForDownloadId:DownloadId];
		if (OldTaskState != NSURLSessionTaskStateSuspended)
		{
			[NewTask resume];
		}
	}

	UE_DNLD_LOG(@"RecreateDownloads finished");
}

- (NSURLSessionDownloadTask*)FindDownloadTaskFor:(NSUInteger)DownloadId
{
	@synchronized(_AllDownloads)
	{
		return [_AllDownloads objectForKey:[NSNumber numberWithUnsignedInteger:DownloadId]];
	}
}

- (NSUInteger)FindDownloadIdForTask:(NSURLSessionDownloadTask*)Task
{
	// TODO this is slow, optimize if needed.
	__block NSUInteger ExistingDownloadId = InvalidDownloadId;
	@synchronized(_AllDownloads)
	{
		[_AllDownloads enumerateKeysAndObjectsUsingBlock:^(NSNumber* IterKey, NSURLSessionDownloadTask* IterTask, BOOL* IterStop)
		{
			if (IterTask == Task)
			{
				ExistingDownloadId = IterKey.unsignedIntegerValue;
				*IterStop = YES;
			}
		}];
	}

	return ExistingDownloadId;
}

- (NSUInteger)EnsureTaskIsTracked:(NSURLSessionDownloadTask*)Task
{
	const NSUInteger ExistingDownloadId = [self FindDownloadIdForTask:Task];
	if (ExistingDownloadId != InvalidDownloadId)
	{
		return ExistingDownloadId;
	}

	@synchronized(_AllDownloads)
	{
		const NSUInteger DownloadId = _NextDownloadId++;
		[_AllDownloads setObject:Task forKey:[NSNumber numberWithUnsignedInteger:DownloadId]];
		return DownloadId;
	}
}

- (void)ReplaceTrackedTaskWith:(NSURLSessionDownloadTask*)NewTask ForDownloadId:(NSUInteger)DownloadId
{
	@synchronized(_AllDownloads)
	{
		[_AllDownloads setObject:NewTask forKey:[NSNumber numberWithUnsignedInteger:DownloadId]];
	}
}

- (void)EnsureTaskIsNotTracked:(NSURLSessionDownloadTask*)Task
{
	const NSUInteger ExistingDownloadId = [self FindDownloadIdForTask:Task];
	if (ExistingDownloadId != InvalidDownloadId)
	{
		@synchronized(_AllDownloads)
		{
			[_AllDownloads removeObjectForKey:[NSNumber numberWithUnsignedInteger:ExistingDownloadId]];
		}
	}
}

- (void)SetDownloadResult:(NSInteger)HTTPCode WithTempFile:(NSString*)TempFile ForDownload:(NSURLSessionDownloadTask*)Task
{
	const NSUInteger DownloadId = [self FindDownloadIdForTask:Task];
	if (DownloadId == InvalidDownloadId)
	{
		UE_DNLD_LOG(@"Can't find DownloadId for task '%@'", Task.taskDescription);
		return;
	}

	// We don't necessarily care if these values survive between application restarts.
	// Otherwise we need to put them inside FBackgroundNSURLSessionDownloadTaskData.
	[Task.progress setUserInfoObject:[NSNumber numberWithInteger:HTTPCode] forKey:NSProgressDownloadResultStatusCode];
	[Task.progress setUserInfoObject:TempFile forKey:NSProgressDownloadResultTempFilePath];
}

- (NSString* _Nullable)GetDownloadResult:(NSUInteger)DownloadId OutStatus:(BOOL* _Nonnull)OutStatus OutStatusCode:(NSInteger*)OutStatusCode
{
	NSURLSessionDownloadTask* Task = [self FindDownloadTaskFor:DownloadId];
	if (Task == nil)
	{
		*OutStatus = NO;
		return nil;
	}

	NSNumber* ResultStatusCode = [Task.progress.userInfo objectForKey:NSProgressDownloadResultStatusCode];
	if (ResultStatusCode == nil)
	{
		*OutStatus = NO;
		return nil;
	}

	*OutStatus = YES;
	*OutStatusCode = ResultStatusCode.integerValue;

	return [Task.progress.userInfo objectForKey:NSProgressDownloadResultTempFilePath];
}

//- (void)URLSession:(NSURLSession*)Session didBecomeInvalidWithError:(NSError*)Error
//{
//	UE_DNLD_LOG(@"didBecomeInvalidWithError");
//}

- (void)URLSessionDidFinishEventsForBackgroundURLSession:(NSURLSession*)Session
{
	UE_DNLD_LOG(@"URLSessionDidFinishEventsForBackgroundURLSession");

	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	if (AppDelegate == nullptr || AppDelegate.BackgroundSessionEventCompleteDelegate == nil)
	{
		return;
	}
	
	void(^CompletionHandler)() = [AppDelegate.BackgroundSessionEventCompleteDelegate retain];
	AppDelegate.BackgroundSessionEventCompleteDelegate = nil;

	// Completion handler has to be invoked on the main thread.
	[[NSOperationQueue mainQueue] addOperationWithBlock:^
	{
		UE_DNLD_LOG(@"URLSessionDidFinishEventsForBackgroundURLSession calling completion handler.");
		CompletionHandler();
		[CompletionHandler release];
	}];
}

- (void)URLSession:(NSURLSession*)Session task:(NSURLSessionTask*)GenericTask didCompleteWithError:(NSError*)Error
{
	if (GenericTask.state == NSURLSessionTaskStateCompleted || ![GenericTask isKindOfClass:[NSURLSessionDownloadTask class]])
	{
		return;
	}

	NSURLSessionDownloadTask* Task = (NSURLSessionDownloadTask*)GenericTask;
	NSString* LocalizedDescription = Error != nil ? Error.localizedDescription : @"nil";

	const NSUInteger DownloadId = [self FindDownloadIdForTask:Task];
	const bool bIsTrackedTask = DownloadId != InvalidDownloadId;

	FBackgroundNSURLSessionDownloadTaskData* TaskData = [FBackgroundNSURLSessionDownloadTaskData TaskDataFromSerializedString:Task.taskDescription];
	if (bIsTrackedTask && TaskData != nil)
	{
		NSData* ResumeData = [Error.userInfo objectForKey:NSURLSessionDownloadTaskResumeData];
		const bool bHasResumeData = ResumeData != nil && ResumeData.length > 0;
		
		NSURL* NextURL = [TaskData GetNextURL];
		
		// Continue trying if next URL is available.
		if (NextURL != nil)
		{
			// Create resume request if our URL is the same and we have resume data.
			if (bHasResumeData && NextURL != nil && [NextURL.absoluteString isEqualToString:Task.originalRequest.URL.absoluteString])
			{
				UE_DNLD_LOG(@"didCompleteWithError, task '%@' with taskIdentifier %lu failed due to '%@' and has resume data and next url is the same, retrying", Task.taskDescription, Task.taskIdentifier, LocalizedDescription);

				NSURLSessionDownloadTask* NewTask = [self CreateDownloadForResumeData:ResumeData WithPriority:Task.priority WithTaskData:TaskData];
				[self ReplaceTrackedTaskWith:NewTask ForDownloadId:DownloadId];
				[NewTask resume];
				return;
			}
			else
			{
				if (bHasResumeData)
				{
					// It should be possible to patch resume data to point to a new URL. But there is no public API to do that yet.
					UE_DNLD_LOG(@"didCompleteWithError, task '%@' with taskIdentifier %lu failed due to '%@' and has resume data but next url is different, retrying", Task.taskDescription, Task.taskIdentifier, LocalizedDescription);
				}
				else
				{
					UE_DNLD_LOG(@"didCompleteWithError, task '%@' with taskIdentifier %lu failed due to '%@' and has no resume data or next url is different, retrying", Task.taskDescription, Task.taskIdentifier, LocalizedDescription);
				}

				NSURLSessionDownloadTask* NewTask = [self CreateDownloadForURL:NextURL WithPriority:Task.priority WithTaskData:TaskData];
				[self ReplaceTrackedTaskWith:NewTask ForDownloadId:DownloadId];
				[NewTask resume];
				return;
			}
		}
	}

	// Can't retry anymore, fail the request
	{
		UE_DNLD_LOG(@"didCompleteWithError, task '%@' with taskIdentifier %lu failed due to '%@', has no retry data or no next url, failing request", Task.taskDescription, Task.taskIdentifier, LocalizedDescription);

		NSURLResponse* GenericResponse = Task.response;
		NSInteger StatusCode = HTTPStatusCodeErrorServer;
		if ([GenericResponse isKindOfClass:[NSHTTPURLResponse class]])
		{
			NSHTTPURLResponse* Response = (NSHTTPURLResponse*)GenericResponse;
			if (Response.statusCode >= HTTPStatusCodeErrorBadRequest)
			{
				StatusCode = Response.statusCode;
			}
		}

		[self SetDownloadResult:StatusCode WithTempFile:nil ForDownload:Task];
	}
}

//- (void)URLSession:(NSURLSession *)session taskIsWaitingForConnectivity:(NSURLSessionTask *)Task
//{
//	UE_DNLD_LOG(@"taskIsWaitingForConnectivity '%@'", Task.taskDescription);
//}

//- (void)URLSession:(NSURLSession *)Session task:(NSURLSessionTask *)Task willBeginDelayedRequest:(NSURLRequest *)Request completionHandler:(void (^)(NSURLSessionDelayedRequestDisposition Disposition, NSURLRequest* NewRequest))CompletionHandler
//{
//	UE_DNLD_LOG(@"willBeginDelayedRequest '%@' for '%@' with taskIdentifier %lu", Task.taskDescription, Request.debugDescription, Task.taskIdentifier);
//	CompletionHandler(NSURLSessionDelayedRequestContinueLoading, Request);
//}

- (void)URLSession:(NSURLSession*)Session downloadTask:(NSURLSessionDownloadTask*)Task didFinishDownloadingToURL:(NSURL*)Location
{
	// Should not be needed, but ensure this just in case
	[self EnsureTaskIsTracked:Task];

	NSString* DestinationPath = [self GetTempPathForURL:Task.originalRequest.URL];

	// Try to remove existing file in case if we have a stale file.
	if ([[NSFileManager defaultManager] fileExistsAtPath:DestinationPath])
	{
		[[NSFileManager defaultManager] removeItemAtPath:DestinationPath error:nil];
	}

	NSError* Error = nil;
	[[NSFileManager defaultManager] moveItemAtURL:Location toURL:[NSURL fileURLWithPath:DestinationPath] error:&Error];

	// Update task progress just in case didWriteData was not invoked
	const uint64 TotalBytesWritten = [[[NSFileManager defaultManager] attributesOfItemAtPath:DestinationPath error:nil] fileSize];
	[self SetCurrentDownloadedBytes:TotalBytesWritten ForTask:Task];

	if (Error != nil)
	{
		[self SetDownloadResult:HTTPStatusCodeErrorServer WithTempFile:nil ForDownload:Task];

		UE_DNLD_LOG(@"didFinishDownloadingToURL task '%@' with taskIdentifier %lu failed to move file to '%@' due to '%@'", Task.taskDescription, Task.taskIdentifier, DestinationPath, Error.localizedDescription);
	}
	else
	{
		[self SetDownloadResult:HTTPStatusCodeSuccessCreated WithTempFile:DestinationPath ForDownload:Task];

		UE_DNLD_LOG(@"didFinishDownloadingToURL task '%@' with taskIdentifier %lu move file to '%@', download finished", Task.taskDescription, Task.taskIdentifier, DestinationPath);
	}
}

- (void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)Task didWriteData:(int64_t)BytesWritten totalBytesWritten:(int64_t)TotalBytesWritten totalBytesExpectedToWrite:(int64_t)TotalBytesExpectedToWrite
{
	[self SetCurrentDownloadedBytes:TotalBytesWritten ForTask:Task];
}

@end

const uint64 FBackgroundURLSessionHandler::InvalidDownloadId = [FBackgroundNSURLSession GetInvalidDownloadId];

void FBackgroundURLSessionHandler::AllowCellular(bool bAllow)
{
	const BOOL bCurrentValue = [FBackgroundNSURLSession Shared].AllowCellular;
	const BOOL bNewValue = bAllow ? YES : NO;
	if (bCurrentValue == bNewValue)
	{
		return;
	}

	[[FBackgroundNSURLSession Shared] setAllowCellular:bNewValue];
	[[FBackgroundNSURLSession Shared] RecreateDownloads];
}

uint64 FBackgroundURLSessionHandler::CreateOrFindDownload(const TArray<FString>& URLs, const float Priority, BackgroundHttpFileHashHelperRef HelperRef)
{
	NSMutableArray* URLArray = [NSMutableArray arrayWithCapacity:URLs.Num()];
	for (const FString& URL: URLs)
	{
		[URLArray addObject:URL.GetNSString()];
	}

	[[FBackgroundNSURLSession Shared] SetFileHashHelper:HelperRef];
	return [[FBackgroundNSURLSession Shared] CreateOrFindDownloadForURLs:URLArray WithPriority:Priority];
}

void FBackgroundURLSessionHandler::PauseDownload(const uint64 DownloadId)
{
	[[FBackgroundNSURLSession Shared] PauseDownload:DownloadId];
}

void FBackgroundURLSessionHandler::ResumeDownload(const uint64 DownloadId)
{
	[[FBackgroundNSURLSession Shared] ResumeDownload:DownloadId];
}

void FBackgroundURLSessionHandler::CancelDownload(const uint64 DownloadId)
{
	[[FBackgroundNSURLSession Shared] CancelDownload:DownloadId];
}

void FBackgroundURLSessionHandler::SetPriority(const uint64 DownloadId, const float Priority)
{
	[[FBackgroundNSURLSession Shared] SetPriority:Priority ForDownload:DownloadId];
}

uint64 FBackgroundURLSessionHandler::GetCurrentDownloadedBytes(const uint64 DownloadId)
{
	return [[FBackgroundNSURLSession Shared] GetCurrentDownloadedBytes:DownloadId];
}

bool FBackgroundURLSessionHandler::IsDownloadFinished(const uint64 DownloadId, int32& OutResultHTTPCode, FString& OutTemporaryFilePath)
{
	BOOL Status = NO;
	NSInteger StatusCode = 0;
	NSString* TempFile = [[FBackgroundNSURLSession Shared] GetDownloadResult:DownloadId OutStatus:&Status OutStatusCode:&StatusCode];
	if (!Status)
	{
		return false;
	}

	OutResultHTTPCode = (int32)StatusCode;
	if (TempFile != nil)
	{
		OutTemporaryFilePath = FString(TempFile);
		UE_DNLD_LOG(@"DownloadId %lli finished with status code %li and path '%@'", DownloadId, (long)StatusCode, TempFile);
	}
	else
	{
		UE_DNLD_LOG(@"DownloadId %lli finished with status code %li and no path", DownloadId, (long)StatusCode);
	}

	return true;
}

void FBackgroundURLSessionHandler::HandleEventsForBackgroundURLSession(const FString& SessionIdentifier)
{
	NSString* Identifier = SessionIdentifier.GetNSString();
	if (![[FBackgroundNSURLSession GetNSURLSessionIdentifier] isEqualToString:Identifier])
	{
		UE_DNLD_LOG(@"HandleEventsForBackgroundURLSession ignoring session identifier '%@'", Identifier);
		return;
	}

	UE_DNLD_LOG(@"HandleEventsForBackgroundURLSession will initializes session with identifier '%@'", Identifier);
	[FBackgroundNSURLSession Shared];
	// will invoke URLSessionDidFinishEventsForBackgroundURLSession internally.
}

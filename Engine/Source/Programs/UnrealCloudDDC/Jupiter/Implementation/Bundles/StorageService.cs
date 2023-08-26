// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Serialization;
using Jupiter.Implementation.Blob;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging.Abstractions;

namespace Jupiter.Implementation.Bundles;

public interface IStorageService
{
	Task<StorageClient> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken);
}

public class StorageService : IStorageService
{
	private readonly IServiceProvider _provider;
	private readonly ConcurrentDictionary<NamespaceId, StorageClient> _backends = new ConcurrentDictionary<NamespaceId, StorageClient>();

	public StorageService(IServiceProvider provider)
	{
		_provider = provider;
	}

	public Task<StorageClient> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken)
	{
		StorageClient storageClient = _backends.GetOrAdd(namespaceId, x => ActivatorUtilities.CreateInstance<StorageClient>(_provider, namespaceId));
		return Task.FromResult(storageClient);
	}
}

public class StorageClient : BundleStorageClient
{
	private readonly NamespaceId _namespaceId;
	private readonly IBlobService _blobService;
	private readonly IReferencesStore _refStore;
	private readonly IBlobIndex _blobIndex;
	private readonly BucketId _defaultBucket = new BucketId("bundles");
	private readonly BundleReader _treeReader;

	public bool SupportsRedirects { get; set; } = true;

	public StorageClient(NamespaceId namespaceId, IBlobService blobService, IReferencesStore refStore, IBlobIndex blobIndex)
		: base(null, NullLogger.Instance)
	{
		_namespaceId = namespaceId;
		_blobService = blobService;
		_refStore = refStore;
		_blobIndex = blobIndex;
		_treeReader = new BundleReader(this, null, NullLogger.Instance);
	}

#pragma warning disable IDE0060
	public async Task<(BundleLocator Locator, Uri UploadUrl)?> GetWriteRedirectAsync(string prefix, CancellationToken cancellationToken)
#pragma warning restore IDE0060
	{
		BundleLocator locator = BundleLocator.CreateUnique(prefix);
		BlobId blobIdentifier = BlobId.FromBlobLocator(locator);
		Uri? redirectUri = await _blobService.MaybePutObjectWithRedirectAsync(_namespaceId, blobIdentifier);
		if (redirectUri == null)
		{
			return null;
		}
		return (locator, redirectUri);
	}

	public override async Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix, CancellationToken cancellationToken)
	{
		BundleLocator locator = BundleLocator.CreateUnique(prefix);
		BlobId blobIdentifier = BlobId.FromBlobLocator(locator);
		await _blobService.PutObjectAsync(_namespaceId, bundle.AsSequence().ToArray(), blobIdentifier);

		await using ReadOnlySequenceStream bundleStream = new ReadOnlySequenceStream(bundle.AsSequence());
		BundleHeader bundleHeader = await BundleHeader.FromStreamAsync(bundleStream, cancellationToken);
		List<Task> addReferencesTasks = new List<Task>();
		foreach (BundleLocator import in bundleHeader.Imports)
		{
			BlobId dependentBlob = BlobId.FromBlobLocator(import);
			addReferencesTasks.Add(_blobIndex.AddBlobReferencesAsync(_namespaceId, dependentBlob, blobIdentifier));
		}

		await Task.WhenAll(addReferencesTasks);
		
		return locator;
	}

	public override async Task AddAliasAsync(Utf8String name, BundleNodeHandle handle, int rank, CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
	}

	public override async Task RemoveAliasAsync(Utf8String name, BundleNodeHandle handle, CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
	}

	public override async IAsyncEnumerable<BundleNodeHandle> FindAliasAsync(Utf8String name, [EnumeratorCancellation] CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
		yield break;
	}

#pragma warning disable IDE0060
	public async Task<Uri?> GetReadRedirectAsync(BundleLocator locator, CancellationToken cancellationToken)
#pragma warning restore IDE0060
	{
		BlobId blobIdentifier = BlobId.FromBlobLocator(locator);
		Uri? redirectUri = await _blobService.GetObjectWithRedirectAsync(_namespaceId, blobIdentifier);
		return redirectUri;
	}

	public override async Task<Stream> OpenAsync(BundleLocator locator, int offset, int length = 0, CancellationToken cancellationToken = default)
	{
		BlobId blobIdentifier = BlobId.FromBlobLocator(locator);
		BlobContents blobContents = await _blobService.GetObjectAsync(_namespaceId, blobIdentifier);
		return blobContents.Stream;
	}

	public override async Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
	{
		// TODO: Cache time is ignored
		try
		{
			RefRecord record = await _refStore.GetAsync(_namespaceId, _defaultBucket,  RefId.FromName(name.ToString()), IReferencesStore.FieldFlags.IncludePayload, IReferencesStore.OperationFlags.None );
			if (record.InlinePayload == null)
			{
				// if there is no inline payload this is not a bundle ref
				return null;
			}

			RefInlinePayload inlinePayload = CbSerializer.Deserialize<RefInlinePayload>(record.InlinePayload);
			IoHash nodeHash = inlinePayload.BlobHash;
			BundleLocator blobLocator = new BundleLocator(inlinePayload.BlobLocator);
			int exportId = inlinePayload.ExportId;

			return new FlushedNodeHandle(_treeReader, new BundleNodeLocator(nodeHash, blobLocator, exportId));
		}
		catch (RefNotFoundException )
		{
			return null;
		}
	}

	public async Task<BlobHandle> WriteRefAsync(RefName name, Bundle bundle, int exportIdx, Utf8String prefix = default, RefOptions? options = null, CancellationToken cancellationToken = default)
	{
		BundleLocator locator = await WriteBundleAsync(bundle, prefix, cancellationToken);
		BlobHandle target = new FlushedNodeHandle(_treeReader, new BundleNodeLocator(bundle.Header.Exports[exportIdx].Hash, locator, exportIdx));
		await WriteRefTargetAsync(name, target, options, cancellationToken);

		return target;
	}

	public override Task WriteRefTargetAsync(RefName refName, BundleNodeHandle target, RefOptions? requestOptions, CancellationToken cancellationToken)
	{
		return WriteRefTargetAsync(refName, target.GetLocator(), requestOptions, cancellationToken);
	}

#pragma warning disable IDE0060
	public async Task WriteRefTargetAsync(RefName refName, BundleNodeLocator target, RefOptions? requestOptions, CancellationToken cancellationToken)
#pragma warning restore IDE0060
	{
		BlobId bundleBlob = BlobId.FromBlobLocator(target.Blob);
		RefId refKey = RefId.FromName(refName.ToString());
		RefInlinePayload inlinePayload = new RefInlinePayload()
		{
			BlobHash = target.Hash, BlobLocator = target.Blob.ToString(), ExportId = target.ExportIdx
		};
		byte[] payload = CbSerializer.SerializeToByteArray(inlinePayload);
		BlobId blobIdentifier = BlobId.FromBlob(payload);
		await _blobIndex.AddRefToBlobsAsync(_namespaceId, _defaultBucket, refKey, new BlobId[] { bundleBlob });

		// TODO: Calculate isFinalized which requires us to be able to resovle references
		await _refStore.PutAsync(_namespaceId, _defaultBucket, refKey, blobIdentifier, payload, isFinalized: true); 
	}

	public override async Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
	{
		return await _refStore.DeleteAsync(_namespaceId, _defaultBucket, RefId.FromName(name.ToString()));
	}
}

public class RefInlinePayload
{
	[CbField("hash")]
	public IoHash BlobHash { get; set; }

	[CbField("loc")]
	public string BlobLocator { get; set; } = null!;

	[CbField("export")]
	public int ExportId { get; set; }
}
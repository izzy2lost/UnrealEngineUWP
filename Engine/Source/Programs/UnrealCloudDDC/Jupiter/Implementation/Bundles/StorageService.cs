// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
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

public sealed class JupiterStorageBackend : EpicGames.Horde.Storage.IStorageBackend
{
	class StorageObject : IStorageObject
	{
		public ReadOnlyMemory<byte> Data { get; }
		public StorageObject(ReadOnlyMemory<byte> data) => Data = data;
		public void Dispose() { }
	}

	private readonly NamespaceId _namespaceId;
	private readonly IBlobService _blobService;
	private readonly IBlobIndex _blobIndex;

	public bool SupportsRedirects => true;

	public JupiterStorageBackend(NamespaceId namespaceId, IBlobService blobService, IBlobIndex blobIndex)
	{
		_namespaceId = namespaceId;
		_blobService = blobService;
		_blobIndex = blobIndex;
	}

	public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => throw new NotSupportedException();

	public void Dispose()
	{
	}

	public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => throw new NotSupportedException();

	public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => throw new NotSupportedException();

	public async Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
	{
		BlobId blobIdentifier = BlobId.FromBlobLocator(new BundleLocator(path));
		BlobContents blobContents = await _blobService.GetObjectAsync(_namespaceId, blobIdentifier);
		if (offset != 0)
		{
			blobContents.Stream.Seek(offset, SeekOrigin.Begin);
		}
		return blobContents.Stream;
	}

	public async Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
	{
		using (Stream stream = await OpenAsync(path, offset, length, cancellationToken))
		{
			byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
			return new StorageObject(data);
		}
	}

	public async ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default)
	{
		BlobId blobIdentifier = BlobId.FromBlobLocator(new BundleLocator(path));
		Uri? redirectUri = await _blobService.GetObjectWithRedirectAsync(_namespaceId, blobIdentifier);
		return redirectUri;
	}

	public async Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
	{
		byte[] data = await stream.ReadAllBytesAsync(cancellationToken);

		string locator = StorageHelpers.CreateUniqueName(prefix);
		BlobId blobIdentifier = BlobId.FromBlobLocator(new BundleLocator(locator));
		await _blobService.PutObjectAsync(_namespaceId, data, blobIdentifier);

		BundleHeader bundleHeader = BundleHeader.Read(data);
		List<Task> addReferencesTasks = new List<Task>();
		foreach (BundleLocator import in bundleHeader.Imports)
		{
			BlobId dependentBlob = BlobId.FromBlobLocator(import);
			addReferencesTasks.Add(_blobIndex.AddBlobReferencesAsync(_namespaceId, dependentBlob, blobIdentifier));
		}

		await Task.WhenAll(addReferencesTasks);

		return locator;
	}

	public async ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
	{
		string locator = StorageHelpers.CreateUniqueName(prefix);
		BlobId blobIdentifier = BlobId.FromBlobLocator(new BundleLocator(locator));
		Uri? redirectUri = await _blobService.MaybePutObjectWithRedirectAsync(_namespaceId, blobIdentifier);
		if (redirectUri == null)
		{
			return null;
		}
		return (locator, redirectUri);
	}

	public void GetStats(StorageStats stats)
	{
		throw new NotImplementedException();
	}

	public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => throw new NotSupportedException();
}

public class StorageClient : BundleStorageClient
{
	private readonly NamespaceId _namespaceId;
	private readonly IReferencesStore _refStore;
	private readonly IBlobIndex _blobIndex;
	private readonly BucketId _defaultBucket = new BucketId("bundles");

	public new JupiterStorageBackend Backend { get; }
	public bool SupportsRedirects { get; set; } = true;

	public StorageClient(NamespaceId namespaceId, IBlobService blobService, IReferencesStore refStore, IBlobIndex blobIndex)
#pragma warning disable CA2000 // Dispose objects before losing scope
		: this(new JupiterStorageBackend(namespaceId, blobService, blobIndex), namespaceId, refStore, blobIndex)
#pragma warning restore CA2000 // Dispose objects before losing scope
	{
	}

	private StorageClient(JupiterStorageBackend backend, NamespaceId namespaceId, IReferencesStore refStore, IBlobIndex blobIndex)
		: base(backend, BundleReaderCache.None, NullLogger.Instance)
	{
		Backend = backend;

		_namespaceId = namespaceId;
		_refStore = refStore;
		_blobIndex = blobIndex;
	}

	public override async Task AddAliasAsync(Utf8String name, BundleNodeLocator locator, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
	}

	public override async Task RemoveAliasAsync(Utf8String name, BundleNodeLocator locator, CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
	}

	public override async Task<BlobAlias[]> FindAliasesAsync(Utf8String name, int? maxResults = null, CancellationToken cancellationToken = default)
	{
		// TODO: Implement aliases
		await Task.CompletedTask;
		throw new NotImplementedException();
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

			throw new NotImplementedException();
			// Creating a BundleReader requires a BundleReaderCache and Client, which bring in a few to many types
			//return new FlushedNodeHandle(BundleReader, new BundleNodeLocator(nodeHash, blobLocator, exportId));
		}
		catch (RefNotFoundException )
		{
			return null;
		}
	}

	public async Task<BlobHandle> WriteRefAsync(RefName name, Bundle bundle, int exportIdx, Utf8String prefix = default, RefOptions? options = null, CancellationToken cancellationToken = default)
	{
		BundleLocator locator = await this.WriteBundleAsync(bundle, prefix, cancellationToken);
		BundleNodeLocator nodeLocator = new BundleNodeLocator(bundle.Header.Exports[exportIdx].Hash, locator, exportIdx);
		await WriteRefTargetAsync(name, nodeLocator, options, cancellationToken);

		return CreateNodeHandle(nodeLocator);
	}

#pragma warning disable IDE0060
	public override async Task WriteRefTargetAsync(RefName refName, BundleNodeLocator target, RefOptions? requestOptions, CancellationToken cancellationToken)
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
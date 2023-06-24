// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
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

    public async Task<(BundleLocator Locator, Uri UploadUrl)?> GetWriteRedirectAsync(string prefix, CancellationToken cancellationToken)
    {
        BundleLocator locator = BundleLocator.CreateUnique(prefix);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        Uri? redirectUri = await _blobService.MaybePutObjectWithRedirect(_namespaceId, blobIdentifier);
        if (redirectUri == null)
        {
            return null;
        }
        return (locator, redirectUri);
    }

    public override async Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix, CancellationToken cancellationToken)
    {
        BundleLocator locator = BundleLocator.CreateUnique(prefix);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        await _blobService.PutObject(_namespaceId, bundle.AsSequence().ToArray(), blobIdentifier);

        await using ReadOnlySequenceStream bundleStream = new ReadOnlySequenceStream(bundle.AsSequence());
        BundleHeader bundleHeader = await BundleHeader.FromStreamAsync(bundleStream, cancellationToken);
        List<Task> addReferencesTasks = new List<Task>();
        foreach (BundleLocator import in bundleHeader.Imports)
        {
            BlobIdentifier dependentBlob = BlobIdentifier.FromBlobLocator(import);
            addReferencesTasks.Add(_blobIndex.AddBlobReferences(_namespaceId, dependentBlob, blobIdentifier));
        }

        await Task.WhenAll(addReferencesTasks);
        
        return locator;
    }

    public override async Task AddAliasAsync(Utf8String name, BundleNodeHandle handle, CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
    }

    public override async Task RemoveAliasAsync(Utf8String name, BundleNodeHandle handle, CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
    }

    public override async IAsyncEnumerable<BundleNodeHandle> FindNodesAsync(Utf8String name, [EnumeratorCancellation] CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
        yield break;
    }

    public async Task<Uri?> GetReadRedirectAsync(BundleLocator locator, CancellationToken cancellationToken)
    {
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        Uri? redirectUri = await _blobService.GetObjectWithRedirect(_namespaceId, blobIdentifier);
        return redirectUri;
    }

    public override async Task<Bundle> ReadBundleAsync(BundleLocator locator, CancellationToken cancellationToken)
    {
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        BlobContents blobContents = await _blobService.GetObject(_namespaceId, blobIdentifier);
        return await Bundle.FromStreamAsync(blobContents.Stream, cancellationToken);
    }

    public override async Task<ReadOnlyMemory<byte>> ReadBundleRangeAsync(BundleLocator locator, int offset, int length, CancellationToken cancellationToken)
    {
        Bundle bundle = await ReadBundleAsync(locator, cancellationToken);
        ReadOnlySequence<byte> sequence = bundle.AsSequence();
		sequence = sequence.Slice(offset);

		if(sequence.Length > length)
		{
			sequence = sequence.Slice(0, length);
		}

        return sequence.AsSingleSegment();
    }

    public override async Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
    {
        // TODO: Cache time is ignored
        try
        {
            ObjectRecord record = await _refStore.Get(_namespaceId, _defaultBucket,  IoHashKey.FromName(name.ToString()), IReferencesStore.FieldFlags.IncludePayload);
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
        catch (ObjectNotFoundException )
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

    public async Task WriteRefTargetAsync(RefName refName, BundleNodeLocator target, RefOptions? requestOptions, CancellationToken cancellationToken)
    {
        BlobIdentifier bundleBlob = BlobIdentifier.FromBlobLocator(target.Blob);
        IoHashKey refKey = IoHashKey.FromName(refName.ToString());
        RefInlinePayload inlinePayload = new RefInlinePayload()
        {
            BlobHash = target.Hash, BlobLocator = target.Blob.ToString(), ExportId = target.ExportIdx
        };
        byte[] payload = CbSerializer.SerializeToByteArray(inlinePayload);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlob(payload);
        await _blobIndex.AddRefToBlobs(_namespaceId, _defaultBucket, refKey, new BlobIdentifier[] { bundleBlob });

        // TODO: Calculate isFinalized which requires us to be able to resovle references
        await _refStore.Put(_namespaceId, _defaultBucket, refKey, blobIdentifier, payload, isFinalized: true); 
    }

    public override async Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
    {
        await _refStore.Delete(_namespaceId, _defaultBucket, IoHashKey.FromName(name.ToString()));
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
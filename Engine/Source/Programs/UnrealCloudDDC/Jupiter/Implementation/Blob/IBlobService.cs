// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Common.Implementation;

namespace Jupiter.Implementation;

public interface IBlobService
{
    Task<ContentHash> VerifyContentMatchesHash(Stream content, ContentHash identifier);
    Task<BlobIdentifier> PutObjectKnownHash(NamespaceId ns, IBufferedPayload content, BlobIdentifier identifier);
    Task<BlobIdentifier> PutObject(NamespaceId ns, IBufferedPayload payload, BlobIdentifier identifier);
    Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] payload, BlobIdentifier identifier);
    Task<Uri?> MaybePutObjectWithRedirect(NamespaceId ns, BlobIdentifier identifier);

    Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, List<string>? storageLayers = null, bool supportsRedirectUri = false);
    
    Task<Uri?> GetObjectWithRedirect(NamespaceId ns, BlobIdentifier blobIdentifier, List<string>? storageLayers = null);

    Task<BlobContents> ReplicateObject(NamespaceId ns, BlobIdentifier blob, bool force = false);

    Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, List<string>? storageLayers = null);

    /// <summary>
    /// Checks that the blob exists in the root store, the store which is last in the list and thus is intended to have every blob in it
    /// </summary>
    /// <param name="ns">The namespace</param>
    /// <param name="blob">The identifier of the blob</param>
    /// <returns></returns>
    Task<bool> ExistsInRootStore(NamespaceId ns, BlobIdentifier blob);

    // Delete a object
    Task DeleteObject(NamespaceId ns, BlobIdentifier blob);

    // delete the whole namespace
    Task DeleteNamespace(NamespaceId ns);

    IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns);
    Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IEnumerable<BlobIdentifier> blobs);
    Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IAsyncEnumerable<BlobIdentifier> blobs);
    Task<BlobContents> GetObjects(NamespaceId ns, BlobIdentifier[] refRequestBlobReferences);

    bool ShouldFetchBlobOnDemand(NamespaceId ns);
}


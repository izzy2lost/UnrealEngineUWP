// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles.V2
{
	/// <summary>
	/// Utility class for constructing BlobData objects from a packet, caching any computed handles to other blobs.
	/// </summary>
	public sealed class PacketReader : IDisposable
	{
		readonly IStorageClient _storageClient;
		readonly BundleCache _cache;
		readonly IBlobHandle _bundleHandle;
		readonly PacketHandle _packetHandle;
		readonly Packet _decodedPacket;
		readonly IRefCountedHandle? _memoryOwner;
		readonly IBlobHandle?[] _cachedImportHandles;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="storageClient"></param>
		/// <param name="cache"></param>
		/// <param name="bundleHandle"></param>
		/// <param name="packetHandle"></param>
		/// <param name="decodedPacket">Data for the packet</param>
		/// <param name="memoryOwner">Owner for the packet data</param>
		public PacketReader(IStorageClient storageClient, BundleCache cache, IBlobHandle bundleHandle, PacketHandle packetHandle, Packet decodedPacket, IRefCountedHandle? memoryOwner)
		{
			_storageClient = storageClient;
			_cache = cache;
			_bundleHandle = bundleHandle;
			_packetHandle = packetHandle;
			_decodedPacket = decodedPacket;
			_memoryOwner = memoryOwner;
			_cachedImportHandles = new IBlobHandle?[_decodedPacket.GetImportCount()];
		}

		/// <inheritdoc/>
		public void Dispose() => _memoryOwner?.Dispose();

		/// <summary>
		/// Reads an export from this packet
		/// </summary>
		/// <param name="exportIdx">Index of the export</param>
		public BlobData ReadExport(int exportIdx)
		{
			PacketExport export = _decodedPacket.GetExport(exportIdx);
			PacketExportHeader exportHeader = export.GetHeader();

			BlobType type = _decodedPacket.GetType(exportHeader.TypeIdx);

			IBlobHandle[] imports = new IBlobHandle[exportHeader.Imports.Length];
			for (int idx = 0; idx < exportHeader.Imports.Length; idx++)
			{
				imports[idx] = GetImportHandle(exportHeader.Imports[idx]);
			}

			if (_memoryOwner == null)
			{
				return new BlobData(type, export.GetPayload(), imports);
			}
			else
			{
				return new BlobDataWithOwner(type, export.GetPayload(), imports, _memoryOwner.AddRef());
			}
		}

		/// <summary>
		/// Reads an export from this packet
		/// </summary>
		/// <param name="exportIdx">Index of the export</param>
		public IReadOnlyMemoryOwner<byte> ReadExportBody(int exportIdx)
		{
			PacketExport export = _decodedPacket.GetExport(exportIdx);
			return ReadOnlyMemoryOwner.Create(export.GetPayload(), _memoryOwner?.AddRef());
		}

		/// <summary>
		/// Gets an import handle for the packet
		/// </summary>
		IBlobHandle GetImportHandle(int index)
		{
			IBlobHandle? importHandle = _cachedImportHandles[index];
			if (importHandle is null)
			{
				PacketImport import = _decodedPacket.GetImport(index);

				switch (import.BaseIdx)
				{
					case PacketImport.InvalidBaseIdx:
						importHandle = _storageClient.CreateBlobHandle(new BlobLocator(import.Fragment));
						break;
					case PacketImport.CurrentBundleBaseIdx:
						importHandle = new PacketHandle(_storageClient, _bundleHandle, import.Fragment, _cache);
						break;
					case PacketImport.CurrentPacketBaseIdx:
						importHandle = new ExportHandle(_packetHandle, import.Fragment);
						break;
					default:
						importHandle = GetImportHandle(import.BaseIdx).GetFragmentHandle(import.Fragment);
						break;
				}

				_cachedImportHandles[index] = importHandle;
			}
			return importHandle;
		}
	}
}

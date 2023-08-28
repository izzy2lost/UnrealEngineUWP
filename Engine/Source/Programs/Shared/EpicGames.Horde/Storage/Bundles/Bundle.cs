// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using K4os.Compression.LZ4;
using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Bundle version number
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1027:Mark enums with FlagsAttribute")]
	public enum BundleVersion
	{
		/// <summary>
		/// Initial version number
		/// </summary>
		Initial = 0,

		/// <summary>
		/// Added the BundleExport.Alias property
		/// </summary>
		ExportAliases = 1,

		/// <summary>
		/// Back out change to include aliases. Will likely do this through an API rather than baked into the data. 
		/// </summary>
		RemoveAliases = 2,

		/// <summary>
		/// Use data structures which support in-place reading and writing.
		/// </summary>
		InPlace = 3,

		/// <summary>
		/// Add import hashes to imported nodes
		/// </summary>
		ImportHashes = 4,

		/// <summary>
		/// Last item in the enum. Used for <see cref="Latest"/>
		/// </summary>
		LatestPlusOne,

#pragma warning disable CA1069 // Enums values should not be duplicated
		/// <summary>
		/// The current version number
		/// </summary>
		Latest = (int)LatestPlusOne - 1,
#pragma warning restore CA1069 // Enums values should not be duplicated
	}

	/// <summary>
	/// Header for the contents of a bundle. May contain an inlined payload object containing the object data itself.
	/// </summary>
	public class Bundle
	{
		/// <summary>
		/// Maximum number of exports from a single bundle
		/// </summary>
		public const int MaxExports = 60000;

		/// <summary>
		/// Header for the bundle
		/// </summary>
		public BundleHeader Header { get; }

		/// <summary>
		/// Packet data as described in the header
		/// </summary>
		public IReadOnlyList<ReadOnlyMemory<byte>> Packets { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public Bundle(BundleHeader header, IReadOnlyList<ReadOnlyMemory<byte>> packets)
		{
			Header = header;
			Packets = packets;
		}

		/// <summary>
		/// Reads a bundle from a block of memory
		/// </summary>
		public Bundle(ReadOnlyMemory<byte> memory)
		{
			int offset = BundleHeader.ReadPrelude(memory.Span);
			Header = BundleHeader.Read(memory.Slice(0, offset));

			ReadOnlyMemory<byte>[] packets = new ReadOnlyMemory<byte>[Header.Packets.Count];
			for (int idx = 0; idx < Header.Packets.Count; idx++)
			{
				BundlePacket packet = Header.Packets[idx];
				packets[idx] = memory.Slice(offset, packet.EncodedLength);
				offset += packet.EncodedLength;
			}

			Packets = packets;
		}

		/// <summary>
		/// Reads a bundle from the given stream
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> FromStreamAsync(Stream stream, CancellationToken cancellationToken = default)
		{
			BundleHeader header = await BundleHeader.FromStreamAsync(stream, cancellationToken);

			ReadOnlyMemory<byte>[] packets = new ReadOnlyMemory<byte>[header.Packets.Count];
			for (int idx = 0; idx < header.Packets.Count; idx++)
			{
				BundlePacket packet = header.Packets[idx];

				byte[] data = new byte[packet.EncodedLength];
				await stream.ReadFixedLengthBytesAsync(data, cancellationToken);

				packets[idx] = data;
			}

			return new Bundle(header, packets);
		}

		/// <summary>
		/// Serializes the bundle to a sequence of bytes
		/// </summary>
		/// <returns>Sequence for the bundle</returns>
		public ReadOnlySequence<byte> AsSequence()
		{
			ReadOnlySequenceBuilder<byte> sequence = new ReadOnlySequenceBuilder<byte>();
			Header.AppendTo(sequence);

			foreach (ReadOnlyMemory<byte> packet in Packets)
			{
				sequence.Append(packet);
			}

			return sequence.Construct();
		}
	}

	/// <summary>
	/// Indicates the compression format in the bundle
	/// </summary>
	public enum BundleCompressionFormat
	{
		/// <summary>
		/// Packets are uncompressed
		/// </summary>
		None = 0,

		/// <summary>
		/// LZ4 compression
		/// </summary>
		LZ4 = 1,

		/// <summary>
		/// Gzip compression
		/// </summary>
		Gzip = 2,

		/// <summary>
		/// Oodle compression (Kraken)
		/// </summary>
		Oodle = 3,

		/// <summary>
		/// Brotli compression
		/// </summary>
		Brotli = 4,
	}

	/// <summary>
	/// Identifier for the type of a section in the bundle header
	/// </summary>
	public enum BundleSectionType
	{
		/// <summary>
		/// List of custom types
		/// </summary>
		Types = 0,

		/// <summary>
		/// Imports of other bundles
		/// </summary>
		Imports = 1,

		/// <summary>
		/// List of exports
		/// </summary>
		Exports = 2,

		/// <summary>
		/// References to exports in other bundles
		/// </summary>
		ExportRefs = 3,

		/// <summary>
		/// Packet headers
		/// </summary>
		Packets = 4,
	}

	/// <summary>
	/// Header for the contents of a bundle.
	/// </summary>
	public class BundleHeader
	{
		/// <summary>
		/// Signature bytes
		/// </summary>
		public static ReadOnlyMemory<byte> Signature { get; } = Encoding.UTF8.GetBytes("UEBN");

		/// <summary>
		/// Length of the prelude data
		/// </summary>
		public const int PreludeLength = 8;

		/// <summary>
		/// Types for exports within this bundle
		/// </summary>
		public BundleTypeCollection Types { get; }

		/// <summary>
		/// Bundles that we reference nodes in
		/// </summary>
		public BundleImportCollection Imports { get; }

		/// <summary>
		/// Nodes exported from this bundle
		/// </summary>
		public BundleExportCollection Exports { get; }

		/// <summary>
		/// List of data packets within this bundle
		/// </summary>
		public BundlePacketCollection Packets { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleHeader(BundleTypeCollection types, BundleImportCollection imports, BundleExportCollection exports, BundlePacketCollection packets)
		{
			Types = types;
			Imports = imports;
			Exports = exports;
			Packets = packets;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleHeader(IReadOnlyList<BlobType> types, IReadOnlyList<BundleLocator> imports, IReadOnlyList<BundleExport> exports, IReadOnlyList<BundlePacket> packets)
		{
			Types = new BundleTypeCollection(types);
			Imports = new BundleImportCollection(imports);
			Exports = new BundleExportCollection(exports);
			Packets = new BundlePacketCollection(packets);
		}

		/// <summary>
		/// Constructs a new bundle header
		/// </summary>
		/// <param name="types">Type array indexed by each export</param>
		/// <param name="imports">Imported bundles</param>
		/// <param name="exports">Exports for nodes</param>
		/// <param name="packets">Compression packets within the bundle</param>
		public static BundleHeader Create(IReadOnlyList<BlobType> types, IReadOnlyList<BundleLocator> imports, IReadOnlyList<BundleExport> exports, IReadOnlyList<BundlePacket> packets)
		{
			return new BundleHeader(types, imports, exports, packets);
		}

		/// <summary>
		/// Writes data for this bundle to a sequence builder
		/// </summary>
		public void AppendTo(ReadOnlySequenceBuilder<byte> builder)
		{
			long initialLength = builder.Length;

			// Find the total size of the header
			int length = PreludeLength;
			if (Types.Data.Length > 0)
			{
				length += SectionHeaderLength + Types.Data.Length;
			}
			if (Imports.Data.Length > 0)
			{
				length += SectionHeaderLength + Imports.Data.Length;
			}
			if (Exports.Data.Length > 0)
			{
				length += SectionHeaderLength + Exports.Data.Length;
			}
			if (Exports.Refs.Length > 0)
			{
				length += SectionHeaderLength + Exports.Refs.Length;
			}
			if (Packets.Data.Length > 0)
			{
				length += SectionHeaderLength + Packets.Data.Length;
			}

			// Allocate the prelude
			byte[] prelude = new byte[PreludeLength];
			prelude[0] = (byte)'U';
			prelude[1] = (byte)'B';
			prelude[2] = (byte)'N';
			prelude[3] = (byte)BundleVersion.Latest;
			BinaryPrimitives.WriteInt32LittleEndian(prelude.AsSpan(4), length);
			builder.Append(prelude);

			// Write all the sections
			AppendSection(builder, BundleSectionType.Types, Types.Data);
			AppendSection(builder, BundleSectionType.Imports, Imports.Data);
			AppendSection(builder, BundleSectionType.Exports, Exports.Data);
			AppendSection(builder, BundleSectionType.ExportRefs, Exports.Refs);
			AppendSection(builder, BundleSectionType.Packets, Packets.Data);
			Debug.Assert(builder.Length == initialLength + length);
		}

		/// <summary>
		/// Serialize the bundle to a byte array
		/// </summary>
		public byte[] ToByteArray()
		{
			ReadOnlySequenceBuilder<byte> builder = new ReadOnlySequenceBuilder<byte>();
			AppendTo(builder);
			return builder.Construct().ToArray();
		}

		const int SectionHeaderLength = 4;

		static void AppendSection(ReadOnlySequenceBuilder<byte> builder, BundleSectionType type, ReadOnlyMemory<byte> data)
		{
			if (data.Length > 0)
			{
				byte[] header = new byte[SectionHeaderLength];
				WriteSectionHeader(header, type, data.Length);
				builder.Append(header);

				builder.Append(data);
			}
		}

		static void WriteSectionHeader(Span<byte> span, BundleSectionType type, int length)
		{
			BinaryPrimitives.WriteInt32LittleEndian(span, (int)type | (length << 8));
		}

		record class ExportInfo(int TypeIdx, IoHash Hash, int Length, List<BundleExportRef> References);

		/// <summary>
		/// Reads a bundle header from memory
		/// </summary>
		/// <param name="memory">Memory to deserialize from</param>
		/// <returns>New header object</returns>
		public static BundleHeader Read(ReadOnlyMemory<byte> memory)
		{
			ReadOnlySpan<byte> span = memory.Span;
			if (span.StartsWith(s_latestSignature))
			{
				int headerLength = BinaryPrimitives.ReadInt32LittleEndian(memory.Slice(4).Span);
				return ReadLatest((BundleVersion)span[3], memory.Slice(PreludeLength, headerLength - PreludeLength));
			}
			else if (span.StartsWith(s_legacySignature))
			{
				int headerLength = BinaryPrimitives.ReadInt32BigEndian(memory.Slice(4).Span);
				return ReadLegacy(memory.Slice(PreludeLength, headerLength - PreludeLength));
			}
			else
			{
				throw new NotSupportedException();
			}
		}

		/// <summary>
		/// Reads a bundle header from a stream
		/// </summary>
		/// <param name="stream">Stream to deserialize from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New header object</returns>
		public static async Task<BundleHeader> ReadAsync(Stream stream, CancellationToken cancellationToken = default)
		{
			byte[] prelude = new byte[PreludeLength];
			await stream.ReadFixedLengthBytesAsync(prelude, cancellationToken);

			return await ReadAsync(prelude, stream, cancellationToken);
		}

		/// <summary>
		/// Reads a bundle header from a stream
		/// </summary>
		/// <param name="prelude">Prelude data</param>
		/// <param name="stream">Stream to deserialize from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New header object</returns>
		public static async Task<BundleHeader> ReadAsync(ReadOnlyMemory<byte> prelude, Stream stream, CancellationToken cancellationToken = default)
		{
			if (prelude.Span.StartsWith(s_latestSignature))
			{
				BundleVersion version = (BundleVersion)prelude.Span[3];
				int headerLength = BinaryPrimitives.ReadInt32LittleEndian(prelude.Span.Slice(4));

				return await ReadLatestAsync(version, stream, headerLength - PreludeLength, cancellationToken);
			}
			else if (prelude.Span.StartsWith(s_legacySignature))
			{
				int headerLength = BinaryPrimitives.ReadInt32BigEndian(prelude.Span.Slice(4));

				byte[] data = new byte[headerLength - PreludeLength];
				await stream.ReadFixedLengthBytesAsync(data, cancellationToken);

				return ReadLegacy(data);
			}
			else
			{
				throw new NotSupportedException();
			}
		}

		static readonly byte[] s_latestSignature = new byte[] { (byte)'U', (byte)'B', (byte)'N' };

		/// <summary>
		/// Construct a header from the given data encoded in the latest format
		/// </summary>
		/// <param name="version">Version to serialize as</param>
		/// <param name="data">Data for the header, including the prelude</param>
		static BundleHeader ReadLatest(BundleVersion version, ReadOnlyMemory<byte> data)
		{
			ReadOnlyMemory<byte> exportData = ReadOnlyMemory<byte>.Empty;
			ReadOnlyMemory<byte> exportRefData = ReadOnlyMemory<byte>.Empty;

			BundleTypeCollection types = new BundleTypeCollection();
			BundleImportCollection imports = new BundleImportCollection();
			BundlePacketCollection packets = new BundlePacketCollection();

			for (int offset = 0; offset < data.Length;)
			{
				uint header = BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset).Span);
				offset += SectionHeaderLength;

				int length = (int)(header >> 8);
				ReadOnlyMemory<byte> sectionData = data.Slice(offset, length);

				BundleSectionType type = (BundleSectionType)(header & 255);
				switch (type)
				{
					case BundleSectionType.Types:
						types = new BundleTypeCollection(sectionData);
						break;
					case BundleSectionType.Imports:
						imports = new BundleImportCollection(sectionData);
						break;
					case BundleSectionType.Exports:
						exportData = sectionData;
						break;
					case BundleSectionType.ExportRefs:
						exportRefData = sectionData;
						break;
					case BundleSectionType.Packets:
						packets = new BundlePacketCollection(sectionData);
						break;
				}

				offset += length;
			}

			BundleExportCollection exports = new BundleExportCollection(exportData, exportRefData, version);
			return new BundleHeader(types, imports, exports, packets);
		}

		/// <summary>
		/// Construct a header from the given data encoded in the latest format
		/// </summary>
		/// <param name="version">Version to serialize as</param>
		/// <param name="stream">Stream to read from</param>
		/// <param name="length">Length of the header</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		static async Task<BundleHeader> ReadLatestAsync(BundleVersion version, Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] exportData = Array.Empty<byte>();
			byte[] exportRefData = Array.Empty<byte>();

			BundleTypeCollection types = new BundleTypeCollection();
			BundleImportCollection imports = new BundleImportCollection();
			BundlePacketCollection packets = new BundlePacketCollection();

			byte[] sectionHeader = new byte[SectionHeaderLength];
			for (int offset = 0; offset < length;)
			{
				await stream.ReadFixedLengthBytesAsync(sectionHeader, cancellationToken);
				offset += SectionHeaderLength;

				BundleSectionType sectionType = (BundleSectionType)(sectionHeader[0] & 255);
				int sectionLength = (int)(BinaryPrimitives.ReadUInt32LittleEndian(sectionHeader) >> 8);

				switch (sectionType)
				{
					case BundleSectionType.Types:
						types = await BundleTypeCollection.ReadAsync(stream, sectionLength, cancellationToken);
						break;
					case BundleSectionType.Imports:
						imports = await BundleImportCollection.ReadAsync(stream, sectionLength, cancellationToken);
						break;
					case BundleSectionType.Exports:
						exportData = new byte[sectionLength];
						await stream.ReadFixedLengthBytesAsync(exportData, cancellationToken);
						break;
					case BundleSectionType.ExportRefs:
						exportRefData = new byte[sectionLength];
						await stream.ReadFixedLengthBytesAsync(exportRefData, cancellationToken);
						break;
					case BundleSectionType.Packets:
						packets = await BundlePacketCollection.ReadAsync(stream, sectionLength, cancellationToken);
						break;
				}

				offset += sectionLength;
			}

			BundleExportCollection exports = new BundleExportCollection(exportData, exportRefData, version);
			return new BundleHeader(types, imports, exports, packets);
		}

		static readonly byte[] s_legacySignature = new byte[] { (byte)'U', (byte)'E', (byte)'B', (byte)'N' };

		static BundleHeader ReadLegacy(ReadOnlyMemory<byte> memory)
		{
			MemoryReader reader = new MemoryReader(memory);

			BundleVersion version = (BundleVersion)reader.ReadUnsignedVarInt();
			if (version > BundleVersion.RemoveAliases)
			{
				throw new InvalidDataException($"Invalid bundle version {(int)version}. Max supported version in legacy format is {(int)BundleVersion.RemoveAliases}.");
			}

			BundleCompressionFormat compressionFormat = (BundleCompressionFormat)reader.ReadUnsignedVarInt();

			// Read the types
			int numTypes = (int)reader.ReadUnsignedVarInt();
			List<BlobType> types = new List<BlobType>(numTypes);

			for (int typeIdx = 0; typeIdx < numTypes; typeIdx++)
			{
				Guid guid = reader.ReadGuid();
				int serializerVersion = (int)reader.ReadUnsignedVarInt();

				types.Add(new BlobType(guid, serializerVersion));
			}

			// Read the imports
			int numImports = (int)reader.ReadUnsignedVarInt();
			List<BundleLocator> imports = new List<BundleLocator>(numImports);
			List<BundleExportRef> allExportReferences = new List<BundleExportRef>();

			for (int importIdx = 0; importIdx < numImports; importIdx++)
			{
				BundleLocator locator = reader.ReadBlobLocator();
				imports.Add(locator);

				int[] exportIndexes = reader.ReadVariableLengthArray(() => (int)reader.ReadUnsignedVarInt());
				for (int exportIdx = 0; exportIdx < exportIndexes.Length; exportIdx++)
				{
					BundleExportRef exportReference = new BundleExportRef(imports.Count - 1, exportIndexes[exportIdx], IoHash.Zero);
					allExportReferences.Add(exportReference);
				}
			}

			// Read the exports
			int numExports = (int)reader.ReadUnsignedVarInt();
			List<ExportInfo> exportInfos = new List<ExportInfo>(numExports);

			for (int exportIdx = 0; exportIdx < numExports; exportIdx++)
			{
				allExportReferences.Add(new BundleExportRef(-1, exportIdx, IoHash.Zero));

				int typeIdx = (int)reader.ReadUnsignedVarInt();
				IoHash hash = reader.ReadIoHash();
				int length = (int)reader.ReadUnsignedVarInt();

				List<BundleExportRef> exportRefs = new List<BundleExportRef>();

				int numReferences = (int)reader.ReadUnsignedVarInt();
				if (numReferences > 0)
				{
					for (int idx = 0; idx < numReferences; idx++)
					{
						int referenceIdx = (int)reader.ReadUnsignedVarInt();
						exportRefs.Add(allExportReferences[referenceIdx]);
					}
				}

				if (version == BundleVersion.ExportAliases)
				{
					_ = reader.ReadUtf8String();
				}

				exportInfos.Add(new ExportInfo(typeIdx, hash, length, exportRefs));
			}

			// Read the compression packets
			List<BundlePacket> packets;
			if (compressionFormat == BundleCompressionFormat.None)
			{
				packets = new List<BundlePacket>(exportInfos.Count);

				int encodedOffset = 0;
				foreach (ExportInfo export in exportInfos)
				{
					packets.Add(new BundlePacket(compressionFormat, encodedOffset, export.Length, export.Length));
					encodedOffset += export.Length;
				}
			}
			else
			{
				int numPackets = (int)reader.ReadUnsignedVarInt();
				packets = new List<BundlePacket>(numPackets);

				int encodedOffset = 0;
				for (int packetIdx = 0; packetIdx < numPackets; packetIdx++)
				{
					int encodedLength = (int)reader.ReadUnsignedVarInt();
					int decodedLength = (int)reader.ReadUnsignedVarInt();
					packets.Add(new BundlePacket(compressionFormat, encodedOffset, encodedLength, decodedLength));
					encodedOffset += encodedLength;
				}
			}

			// Create the final export list
			List<BundleExport> exports = new List<BundleExport>();
			{
				int packetIdx = 0;
				int packetOffset = 0;

				foreach (ExportInfo exportInfo in exportInfos)
				{
					if (packetOffset + exportInfo.Length > packets[packetIdx].DecodedLength)
					{
						packetIdx++;
						packetOffset = 0;
					}

					exports.Add(new BundleExport(exportInfo.TypeIdx, exportInfo.Hash, packetIdx, packetOffset, exportInfo.Length, exportInfo.References));
					packetOffset += exportInfo.Length;
				}
			}

			return BundleHeader.Create(types, imports, exports, packets);
		}

		/// <summary>
		/// Validates that the prelude bytes for a bundle header are correct
		/// </summary>
		/// <param name="prelude">The prelude bytes</param>
		/// <returns>Length of the header data, including the prelude</returns>
		public static int ReadPrelude(ReadOnlySpan<byte> prelude)
		{
			if (prelude[0] == 'U' && prelude[1] == 'E' && prelude[2] == 'B' && prelude[3] == 'N')
			{
				return BinaryPrimitives.ReadInt32BigEndian(prelude.Slice(4));
			}
			else if (prelude[0] == 'U' && prelude[1] == 'B' && prelude[2] == 'N')
			{
				return BinaryPrimitives.ReadInt32LittleEndian(prelude.Slice(4));
			}
			else
			{
				throw new InvalidDataException("Invalid signature bytes for bundle. Corrupt data?");
			}
		}

		/// <summary>
		/// Reads a bundle header from a stream
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="cancellationToken">Cancellation token for the stream</param>
		/// <returns>New header</returns>
		public static async Task<BundleHeader> FromStreamAsync(Stream stream, CancellationToken cancellationToken)
		{
			byte[] prelude = new byte[PreludeLength];
			await stream.ReadFixedLengthBytesAsync(prelude, cancellationToken);

			int headerLength = ReadPrelude(prelude);

			byte[] header = new byte[headerLength];
			prelude.CopyTo(header.AsSpan());

			await stream.ReadFixedLengthBytesAsync(header.AsMemory(prelude.Length), cancellationToken);
			return BundleHeader.Read(header);
		}
	}

	/// <summary>
	/// Collection of node types in a bundle
	/// </summary>
	public struct BundleTypeCollection
	{
		/// <summary>
		/// Number of bytes in a serialized <see cref="BlobType"/> instance
		/// </summary>
		const int NumBytesPerType = 20;

		/// <summary>
		/// Data for this collection
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleTypeCollection(ReadOnlyMemory<byte> data) => Data = data;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleTypeCollection(IReadOnlyList<BlobType> types)
		{
			byte[] data = new byte[Measure(types)];
			Write(data, types);
			Data = data;
		}

		/// <summary>
		/// Reads a type collection from a stream
		/// </summary>
		public static async Task<BundleTypeCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return new BundleTypeCollection(data);
		}

		/// <inheritdoc/>
		public int Count => Data.Length / NumBytesPerType;

		/// <inheritdoc/>
		public BlobType this[int index]
		{
			get
			{
				ReadOnlySpan<byte> span = Data.Slice(index * NumBytesPerType, NumBytesPerType).Span;

				Guid guid = new Guid(span.Slice(0, 16));
				int version = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(16));

				return new BlobType(guid, version);
			}
		}

		/// <summary>
		/// Gets the size of memory required to serialize a collection of types
		/// </summary>
		/// <param name="types">Type collection</param>
		/// <returns>Number of bytes required to serialize the types</returns>
		public static int Measure(IReadOnlyCollection<BlobType> types) => types.Count * NumBytesPerType;

		/// <summary>
		/// Serializes a set of types to a fixed block of memory
		/// </summary>
		/// <param name="span">Span to write the types to</param>
		/// <param name="types">Collection of types to be written</param>
		public static void Write(Span<byte> span, IReadOnlyCollection<BlobType> types)
		{
			Span<byte> next = span;
			foreach (BlobType type in types)
			{
				type.Guid.TryWriteBytes(next.Slice(0, 16));
				next = next.Slice(16);

				BinaryPrimitives.WriteInt32LittleEndian(next, type.Version);
				next = next.Slice(4);
			}
			Debug.Assert(next.Length == 0);
		}
	}

	/// <summary>
	/// Collection of imported node references
	/// </summary>
	public struct BundleImportCollection : IReadOnlyList<BundleLocator>
	{
		readonly ReadOnlyMemory<byte> _data;

		/// <summary>
		/// Data for the imports
		/// </summary>
		public ReadOnlyMemory<byte> Data => _data;

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		public BundleImportCollection(ReadOnlyMemory<byte> data) => _data = data;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="locators">Locators to write to the </param>
		public BundleImportCollection(IReadOnlyCollection<BundleLocator> locators)
		{
			byte[] data = new byte[Measure(locators)];
			Write(data, locators);
			_data = data;
		}

		/// <summary>
		/// Reads a collection from a stream
		/// </summary>
		public static async Task<BundleImportCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return new BundleImportCollection(data);
		}

		/// <summary>
		/// Retrieve a single import from the collection
		/// </summary>
		public BundleLocator this[int index]
		{
			get
			{
				Debug.Assert(index < Count);
				ReadOnlySpan<byte> span = _data.Span;
				int offset = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(index * sizeof(int)));
				int length = span.Slice(offset).IndexOf((byte)0);
				return new BundleLocator(new Utf8String(_data.Slice(offset, length)));
			}
		}

		/// <inheritdoc/>
		public int Count => (_data.Length == 0) ? 0 : BinaryPrimitives.ReadInt32LittleEndian(_data.Span) / sizeof(int);

		/// <inheritdoc/>
		public IEnumerator<BundleLocator> GetEnumerator()
		{
			int count = Count;
			for (int idx = 0; idx < count; idx++)
			{
				yield return this[idx];
			}
		}

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Measure the size of memory required to store a collection of import locators
		/// </summary>
		/// <param name="locators">Locators to write</param>
		/// <returns>Size in bytes of the output buffer</returns>
		public static int Measure(IReadOnlyCollection<BundleLocator> locators)
		{
			int length = 0;
			foreach (BundleLocator locator in locators)
			{
				length += sizeof(int) + locator.Path.Length + 1;
			}
			return length;
		}

		/// <summary>
		/// Serialize a collection of locators to memory
		/// </summary>
		/// <param name="data">Output buffer for the serialized data</param>
		/// <param name="locators">Locators to write</param>
		public static void Write(Span<byte> data, IReadOnlyCollection<BundleLocator> locators)
		{
			Span<byte> next = data;

			int offset = locators.Count * sizeof(int);
			foreach (BundleLocator locator in locators)
			{
				BinaryPrimitives.WriteInt32LittleEndian(next, offset);
				offset += locator.Path.Length + 1;
				next = next.Slice(sizeof(int));
			}

			foreach (BundleLocator locator in locators)
			{
				locator.Path.Span.CopyTo(next);
				next = next.Slice(locator.Path.Length);

				next[0] = 0;
				next = next.Slice(1);
			}

			Debug.Assert(next.Length == 0);
		}
	}

	/// <summary>
	/// Descriptor for a compression packet
	/// </summary>
	public struct BundlePacket
	{
		/// <summary>
		/// Size of this structure when serialized
		/// </summary>
		public const int NumBytes = 16;

		/// <summary>
		/// Compression format for this packet
		/// </summary>
		public BundleCompressionFormat CompressionFormat { get; }

		/// <summary>
		/// Offset of the packet within the payload stream
		/// </summary>
		public int EncodedOffset { get; }

		/// <summary>
		/// Encoded length of the packet
		/// </summary>
		public int EncodedLength { get; }

		/// <summary>
		/// Decoded length of the packet
		/// </summary>
		public int DecodedLength { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="compressionFormat">Compression format for the packet</param>
		/// <param name="encodedOffset">Offset of the data within the payload stream</param>
		/// <param name="encodedLength">Size of the encoded data</param>
		/// <param name="decodedLength">Size of the decoded data</param>
		public BundlePacket(BundleCompressionFormat compressionFormat, int encodedOffset, int encodedLength, int decodedLength)
		{
			CompressionFormat = compressionFormat;
			EncodedOffset = encodedOffset;
			EncodedLength = encodedLength;
			DecodedLength = decodedLength;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundlePacket(ReadOnlySpan<byte> span)
		{
			CompressionFormat = (BundleCompressionFormat)BinaryPrimitives.ReadInt32LittleEndian(span);
			EncodedOffset = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(4));
			EncodedLength = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(8));
			DecodedLength = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(12));
		}

		/// <summary>
		/// Serialize the struct to memory
		/// </summary>
		public void CopyTo(Span<byte> span)
		{
			BinaryPrimitives.WriteInt32LittleEndian(span, (int)CompressionFormat);
			BinaryPrimitives.WriteInt32LittleEndian(span.Slice(4), EncodedOffset);
			BinaryPrimitives.WriteInt32LittleEndian(span.Slice(8), EncodedLength);
			BinaryPrimitives.WriteInt32LittleEndian(span.Slice(12), DecodedLength);
		}
	}

	/// <summary>
	/// Collection of information about packets in a bundle
	/// </summary>
	public struct BundlePacketCollection : IReadOnlyCollection<BundlePacket>
	{
		readonly ReadOnlyMemory<byte> _data;

		/// <summary>
		/// Data for the collection
		/// </summary>
		public ReadOnlyMemory<byte> Data => _data;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data"></param>
		public BundlePacketCollection(ReadOnlyMemory<byte> data) => _data = data;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="packets">Packets to include in this collection</param>
		public BundlePacketCollection(IReadOnlyCollection<BundlePacket> packets)
		{
			byte[] data = new byte[Measure(packets)];
			Write(data, packets);
			_data = data;
		}

		/// <summary>
		/// Reads a collection from a stream
		/// </summary>
		public static async Task<BundlePacketCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return new BundlePacketCollection(data);
		}

		/// <inheritdoc/>
		public int Count => _data.Length / BundlePacket.NumBytes;

		/// <inheritdoc/>
		public BundlePacket this[int index]
		{
			get
			{
				Debug.Assert(index < Count);
				return new BundlePacket(_data.Slice(index * BundlePacket.NumBytes, BundlePacket.NumBytes).Span);
			}
		}

		/// <inheritdoc/>
		public IEnumerator<BundlePacket> GetEnumerator()
		{
			ReadOnlyMemory<byte> remaining = _data;
			while (remaining.Length > 0)
			{
				yield return new BundlePacket(remaining.Slice(0, BundlePacket.NumBytes).Span);
				remaining = remaining.Slice(BundlePacket.NumBytes);
			}
		}

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Measure the size of memory required to store a collection of import locators
		/// </summary>
		/// <param name="packets">Locators to write</param>
		/// <returns>Size in bytes of the output buffer</returns>
		public static int Measure(IReadOnlyCollection<BundlePacket> packets) => BundlePacket.NumBytes * packets.Count;

		/// <summary>
		/// Serialize a collection of packets to memory
		/// </summary>
		/// <param name="data">Output buffer for the serialized data</param>
		/// <param name="packets">Packets to write</param>
		public static void Write(Span<byte> data, IReadOnlyCollection<BundlePacket> packets)
		{
			Span<byte> next = data;
			foreach (BundlePacket packet in packets)
			{
				packet.CopyTo(next);
				next = next.Slice(BundlePacket.NumBytes);
			}
			Debug.Assert(next.Length == 0);
		}
	}

	/// <summary>
	/// Entry for a node exported from an object
	/// </summary>
	public struct BundleExport
	{
		/// <summary>
		/// Number of bytes in a serialized export object
		/// </summary>
		public const int NumBytes = 32;

		/// <summary>
		/// Raw data for this export
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Hash of the node data
		/// </summary>
		public IoHash Hash => new IoHash(Data.Slice(0, 20).Span);

		/// <summary>
		/// Type id of the node. Can be used to look up the type information from the bundle header.
		/// </summary>
		public int TypeIdx => BinaryPrimitives.ReadUInt16LittleEndian(Data.Slice(20).Span);

		/// <summary>
		/// Packet containing this export's data
		/// </summary>
		public int Packet => BinaryPrimitives.ReadUInt16LittleEndian(Data.Slice(22).Span);

		/// <summary>
		/// Offset within the packet of the node data
		/// </summary>
		public int Offset => BinaryPrimitives.ReadInt32LittleEndian(Data.Slice(24).Span);

		/// <summary>
		/// Length of the node
		/// </summary>
		public int Length => BinaryPrimitives.ReadInt32LittleEndian(Data.Slice(28).Span);

		/// <summary>
		/// References to other nodes
		/// </summary>
		public BundleExportRefCollection References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExport(ReadOnlyMemory<byte> data, BundleExportRefCollection references)
		{
			Data = data;
			References = references;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExport(int typeIdx, IoHash hash, int packetIdx, int offset, int length, IReadOnlyList<BundleExportRef> references)
		{
			byte[] data = new byte[NumBytes];
			Write(data, typeIdx, hash, packetIdx, offset, length);
			Data = data;
			References = new BundleExportRefCollection(references);
		}

		/// <summary>
		/// Writes a new export to a block of memory
		/// </summary>
		public static void Write(Span<byte> data, int typeIdx, IoHash hash, int packet, int offset, int length)
		{
			hash.CopyTo(data);
			BinaryPrimitives.WriteUInt16LittleEndian(data.Slice(20), (ushort)typeIdx);
			BinaryPrimitives.WriteUInt16LittleEndian(data.Slice(22), (ushort)packet);
			BinaryPrimitives.WriteInt32LittleEndian(data.Slice(24), offset);
			BinaryPrimitives.WriteInt32LittleEndian(data.Slice(28), length);
		}
	}

	/// <summary>
	/// Entry for a node exported from an object
	/// </summary>
	public class BundleExportCollection : IReadOnlyList<BundleExport>
	{
		readonly int _count;
		readonly ReadOnlyMemory<byte> _data;
		readonly ReadOnlyMemory<byte> _refs;

		internal ReadOnlyMemory<byte> Data => _data;
		internal ReadOnlyMemory<byte> Refs => _refs;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExportCollection(IReadOnlyList<BundleExport> exports)
		{
			_count = exports.Count;

			byte[] data = new byte[Measure(exports)];
			Write(data, exports);
			_data = data;

			byte[] refs = new byte[MeasureRefs(exports)];
			WriteRefs(refs, exports);
			_refs = refs;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExportCollection(ReadOnlyMemory<byte> data, ReadOnlyMemory<byte> refs, BundleVersion version)
		{
			_count = data.Length / BundleExport.NumBytes;
			_data = data;
			_refs = refs;

			if (version < BundleVersion.ImportHashes && _refs.Length > 0)
			{
				const int OldRefNumBytes = 4;

				int headerLength = BinaryPrimitives.ReadInt32LittleEndian(refs.Span);
				int count = (refs.Length - headerLength) / OldRefNumBytes;

				byte[] newRefs = new byte[headerLength + (count * BundleExportRef.NumBytes)];
				for (int headerOffset = 0; headerOffset < headerLength; headerOffset += sizeof(int))
				{
					int index = (BinaryPrimitives.ReadInt32LittleEndian(refs.Slice(headerOffset).Span) - headerLength) / OldRefNumBytes;
					BinaryPrimitives.WriteInt32LittleEndian(newRefs.AsSpan(headerOffset), headerLength + (index * BundleExportRef.NumBytes));
				}
				for (int index = 0; index < count; index++)
				{
					ReadOnlyMemory<byte> source = refs.Slice(headerLength + (index * OldRefNumBytes), OldRefNumBytes);
					source.CopyTo(newRefs.AsMemory(headerLength + (index * BundleExportRef.NumBytes)));
				}

				_refs = newRefs;
			}
		}

		/// <inheritdoc/>
		public int Count => _data.Length / BundleExport.NumBytes;

		/// <inheritdoc/>
		public BundleExport this[int index]
		{
			get
			{
				Debug.Assert(index < _count);

				ReadOnlyMemory<byte> exportData = _data.Slice(index * BundleExport.NumBytes);

				BundleExportRefCollection exportRefs = new BundleExportRefCollection();
				if (_refs.Length > 0)
				{
					int minOffset = BinaryPrimitives.ReadInt32LittleEndian(_refs.Span.Slice(index * sizeof(int)));
					int maxOffset = (index == Count - 1) ? _refs.Length : BinaryPrimitives.ReadInt32LittleEndian(_refs.Span.Slice((index + 1) * sizeof(int)));
					exportRefs = new BundleExportRefCollection(_refs.Slice(minOffset, maxOffset - minOffset));
				}

				return new BundleExport(exportData, exportRefs);
			}
		}

		/// <inheritdoc/>
		public IEnumerator<BundleExport> GetEnumerator()
		{
			int count = Count;
			for (int idx = 0; idx < count; idx++)
			{
				yield return this[idx];
			}
		}

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Measure the size of memory required to store a collection of exports
		/// </summary>
		/// <param name="exports">Exports to write</param>
		/// <returns>Size in bytes of the output buffer</returns>
		public static int Measure(IReadOnlyCollection<BundleExport> exports) => BundleExport.NumBytes * exports.Count;

		/// <summary>
		/// Serialize a collection of exports to memory
		/// </summary>
		/// <param name="data">Output buffer for the serialized data</param>
		/// <param name="exports">Exports to write</param>
		public static void Write(Span<byte> data, IReadOnlyCollection<BundleExport> exports)
		{
			Span<byte> next = data;
			foreach (BundleExport export in exports)
			{
				export.Data.Span.CopyTo(next);
				next = next.Slice(export.Data.Length);
			}
		}

		/// <summary>
		/// Measure the size of memory required to store a collection of export refs
		/// </summary>
		/// <param name="exports">Exports to write</param>
		/// <returns>Size in bytes of the output buffer</returns>
		public static int MeasureRefs(IReadOnlyCollection<BundleExport> exports) => (sizeof(int) * exports.Count) + exports.Sum(x => x.References.Data.Length);

		/// <summary>
		/// Serialize a collection of export refs to memory
		/// </summary>
		/// <param name="data">Output buffer for the serialized data</param>
		/// <param name="exports">Exports to write</param>
		public static void WriteRefs(Span<byte> data, IReadOnlyCollection<BundleExport> exports)
		{
			int indexOffset = 0;
			int dataOffset = sizeof(int) * exports.Count;

			foreach (BundleExport export in exports)
			{
				BinaryPrimitives.WriteInt32LittleEndian(data.Slice(indexOffset), dataOffset);
				indexOffset += sizeof(int);

				export.References.Data.Span.CopyTo(data.Slice(dataOffset));
				dataOffset += export.References.Data.Span.Length;
			}
		}
	}

	/// <summary>
	/// Reference to a node in another bundle
	/// </summary>
	/// <param name="ImportIdx">Index into the import table of the blob containing the referenced node. Can be -1 for references within the same bundle.</param>
	/// <param name="NodeIdx">Node imported from the bundle</param>
	/// <param name="Hash">Hash of the referenced node</param>
	public record struct BundleExportRef(int ImportIdx, int NodeIdx, IoHash Hash)
	{
		/// <summary>
		/// Number of bytes in the serialized object
		/// </summary>
		public const int NumBytes = 4 + IoHash.NumBytes;

		/// <summary>
		/// Deserialize this object from memory
		/// </summary>
		public static BundleExportRef Read(ReadOnlySpan<byte> data)
		{
			int importIdx = BinaryPrimitives.ReadInt16LittleEndian(data);
			int nodeIdx = BinaryPrimitives.ReadUInt16LittleEndian(data.Slice(2));
			return new BundleExportRef(importIdx, nodeIdx, new IoHash(data.Slice(4)));
		}

		/// <summary>
		/// Serialize this object to memory
		/// </summary>
		public void CopyTo(Span<byte> data)
		{
			BinaryPrimitives.WriteInt16LittleEndian(data, (short)ImportIdx);
			BinaryPrimitives.WriteUInt16LittleEndian(data.Slice(2), (ushort)NodeIdx);
			Hash.CopyTo(data.Slice(4));
		}
	}

	/// <summary>
	/// Collection of information about exported nodes
	/// </summary>
	public struct BundleExportRefCollection : IReadOnlyList<BundleExportRef>
	{
		/// <summary>
		/// Data used to store this collection
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExportRefCollection(ReadOnlyMemory<byte> data) => Data = data;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExportRefCollection(IReadOnlyList<BundleExportRef> exportRefs)
		{
			byte[] data = new byte[exportRefs.Count * BundleExportRef.NumBytes];
			for (int idx = 0; idx < exportRefs.Count; idx++)
			{
				exportRefs[idx].CopyTo(data.AsSpan(idx * BundleExportRef.NumBytes));
			}
			Data = data;
		}

		/// <inheritdoc/>
		public int Count => Data.Length / BundleExportRef.NumBytes;

		/// <inheritdoc/>
		public BundleExportRef this[int index]
		{
			get
			{
				Debug.Assert(index < Count);
				return BundleExportRef.Read(Data.Span.Slice(index * BundleExportRef.NumBytes));
			}
		}

		/// <inheritdoc/>
		public IEnumerator<BundleExportRef> GetEnumerator()
		{
			ReadOnlyMemory<byte> data = Data;
			while (data.Length > 0)
			{
				yield return BundleExportRef.Read(data.Span);
				data = data.Slice(BundleExportRef.NumBytes);
			}
		}

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
	}

	/// <summary>
	/// Utility methods for bundles
	/// </summary>
	public static class BundleData
	{
		/// <summary>
		/// Compress a data packet
		/// </summary>
		/// <param name="format">Format for the compressed data</param>
		/// <param name="input">The data to compress</param>
		/// <param name="writer">Writer for output data</param>
		/// <returns>The compressed data</returns>
		public static int Compress(BundleCompressionFormat format, ReadOnlyMemory<byte> input, IMemoryWriter writer)
		{
			switch (format)
			{
				case BundleCompressionFormat.None:
					{
						writer.WriteFixedLengthBytes(input.Span);
						return input.Length;
					}
				case BundleCompressionFormat.LZ4:
					{
						int maxSize = LZ4Codec.MaximumOutputSize(input.Length);

						Span<byte> buffer = writer.GetSpan(maxSize);
						int encodedLength = LZ4Codec.Encode(input.Span, buffer);

						writer.Advance(encodedLength);
						return encodedLength;
					}
				case BundleCompressionFormat.Gzip:
					{
						using MemoryStream outputStream = new MemoryStream(input.Length);

						using (GZipStream gzipStream = new GZipStream(outputStream, CompressionLevel.Fastest, true))
						{
							gzipStream.Write(input.Span);
						}

						writer.WriteFixedLengthBytes(outputStream.ToArray());
						return (int)outputStream.Length;
					}
				case BundleCompressionFormat.Oodle:
					{
#if WITH_OODLE
						int maxSize = Oodle.MaximumOutputSize(OodleCompressorType.Selkie, input.Length);

						Span<byte> outputSpan = builder.GetSpan(maxSize);
						int encodedLength = Oodle.Compress(OodleCompressorType.Selkie, input.Span, outputSpan, OodleCompressionLevel.HyperFast);
						builder.Advance(encodedLength);

						return encodedLength;
#else
						throw new NotSupportedException("Oodle is not compiled into this build.");
#endif
					}
				case BundleCompressionFormat.Brotli:
					{
						int maxSize = BrotliEncoder.GetMaxCompressedLength(input.Length);

						Span<byte> buffer = writer.GetSpan(maxSize);
						if (!BrotliEncoder.TryCompress(input.Span, buffer, out int encodedLength))
						{
							throw new InvalidOperationException("Unable to compress data using Brotli");
						}

						writer.Advance(encodedLength);
						return encodedLength;
					}
				default:
					throw new InvalidDataException($"Invalid compression format '{(int)format}'");
			}
		}

		/// <summary>
		/// Decompress a packet of data
		/// </summary>
		/// <param name="format">Format of the compressed data</param>
		/// <param name="input">Compressed data</param>
		/// <param name="output">Buffer to receive the decompressed data</param>
		public static void Decompress(BundleCompressionFormat format, ReadOnlyMemory<byte> input, Memory<byte> output)
		{
			switch (format)
			{
				case BundleCompressionFormat.None:
					input.CopyTo(output);
					break;
				case BundleCompressionFormat.LZ4:
					LZ4Codec.Decode(input.Span, output.Span);
					break;
				case BundleCompressionFormat.Gzip:
					{
						using ReadOnlyMemoryStream inputStream = new ReadOnlyMemoryStream(input);
						using GZipStream inflatedStream = new GZipStream(inputStream, CompressionMode.Decompress, true);

						int length = inflatedStream.Read(output.Span);
						if (length != output.Length)
						{
							throw new InvalidDataException($"Decoded data is shorter than expected (expected {output.Length} bytes, got {length} bytes)");
						}
						break;
					}
				case BundleCompressionFormat.Oodle:
#if WITH_OODLE
					Oodle.Decompress(input.Span, output.Span);
					break;
#else
					throw new NotSupportedException("Oodle is not compiled into this build.");
#endif
				case BundleCompressionFormat.Brotli:
					{
						int bytesWritten;
						if (!BrotliDecoder.TryDecompress(input.Span, output.Span, out bytesWritten) || bytesWritten != output.Length)
						{
							throw new InvalidOperationException("Unable to decompress data using Brotli");
						}
						break;
					}
				default:
					throw new InvalidDataException($"Invalid compression format '{(int)format}'");
			}
		}
	}
}

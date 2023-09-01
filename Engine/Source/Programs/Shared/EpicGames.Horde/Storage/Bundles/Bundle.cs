// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Compression;
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
		/// Reads a bundle from the given stream
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> FromStreamAsync(Stream stream, CancellationToken cancellationToken = default)
		{
			BundleHeader header = await BundleHeader.ReadAsync(stream, cancellationToken);

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
		/// List of export headers
		/// </summary>
		ExportHeaders = 2,

		/// <summary>
		/// References to exports in other bundles
		/// </summary>
		ExportReferences = 3,

		/// <summary>
		/// Packet headers
		/// </summary>
		Packets = 4,

		/// <summary>
		/// Merged export headers and references
		/// </summary>
		Exports = 5,
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
		public BundleHeader(BlobType[] types, BundleLocator[] imports, BundleExport[] exports, BundlePacket[] packets)
		{
			Types = new BundleTypeCollection(types);
			Imports = new BundleImportCollection(imports);
			Exports = new BundleExportCollection(exports);
			Packets = new BundlePacketCollection(packets);
		}

		/// <summary>
		/// Writes data for this bundle to a sequence builder
		/// </summary>
		public void AppendTo(ReadOnlySequenceBuilder<byte> builder)
		{
			// Create a writer for the output data
			ChunkedArrayMemoryWriter writer = new ChunkedArrayMemoryWriter();

			// Create the prelude
			Span<byte> prelude = writer.GetSpanAndAdvance(PreludeLength);
			prelude[0] = (byte)'U';
			prelude[1] = (byte)'B';
			prelude[2] = (byte)'N';
			prelude[3] = (byte)BundleVersion.Latest;

			// Write all the sections
			WriteSectionHeader(writer, BundleSectionType.Types, Types.Measure());
			Types.Write(writer);

			WriteSectionHeader(writer, BundleSectionType.Imports, Imports.Measure());
			Imports.Write(writer);

			WriteSectionHeader(writer, BundleSectionType.ExportHeaders, Exports.MeasureHeaders());
			Exports.WriteHeaders(writer);

			WriteSectionHeader(writer, BundleSectionType.ExportReferences, Exports.MeasureRefs());
			Exports.WriteRefs(writer);

			WriteSectionHeader(writer, BundleSectionType.Packets, Packets.Measure());
			Packets.Write(writer);

			// Fill in the length of the header, and append it to the builder
			BinaryPrimitives.WriteInt32LittleEndian(prelude[4..], writer.Length);
			writer.AppendTo(builder);
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

		static void WriteSectionHeader(IMemoryWriter writer, BundleSectionType type, int length)
		{
			writer.WriteInt32((int)type | (length << 8));
		}

		record class ExportInfo(int TypeIdx, IoHash Hash, int Length, List<BundleExportRef> References);

		/// <summary>
		/// Reads a bundle header from memory
		/// </summary>
		/// <param name="memory">Memory to read from</param>
		/// <returns>New header object</returns>
		public static BundleHeader Read(ReadOnlyMemory<byte> memory)
		{
			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(memory);
			return ReadAsync(stream).Result;
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
		/// <param name="stream">Stream to read from</param>
		/// <param name="length">Length of the header</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		static async Task<BundleHeader> ReadLatestAsync(BundleVersion version, Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] exportHeaders = Array.Empty<byte>();
			byte[] exportReferences = Array.Empty<byte>();

			BundleTypeCollection types = new BundleTypeCollection();
			BundleImportCollection imports = new BundleImportCollection();
			BundleExportCollection exports = new BundleExportCollection();
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
						exports = await BundleExportCollection.ReadAsync(stream, sectionLength, cancellationToken);
						break;
					case BundleSectionType.ExportHeaders:
						exportHeaders = new byte[sectionLength];
						await stream.ReadFixedLengthBytesAsync(exportHeaders, cancellationToken);
						break;
					case BundleSectionType.ExportReferences:
						exportReferences = new byte[sectionLength];
						await stream.ReadFixedLengthBytesAsync(exportReferences, cancellationToken);
						break;
					case BundleSectionType.Packets:
						packets = await BundlePacketCollection.ReadAsync(stream, sectionLength, cancellationToken);
						break;
				}

				offset += sectionLength;
			}

			if (exportHeaders.Length > 0)
			{
				exports = BundleExportCollection.Read(exportHeaders, exportReferences, version);
			}

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

			return new BundleHeader(types.ToArray(), imports.ToArray(), exports.ToArray(), packets.ToArray());
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
	}

	/// <summary>
	/// Collection of node types in a bundle
	/// </summary>
	public struct BundleTypeCollection : IReadOnlyList<BlobType>
	{
		readonly BlobType[] _types;

		/// <inheritdoc/>
		public int Count => _types.Length;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleTypeCollection(BlobType[] types) => _types = types;

		/// <summary>
		/// Reads a type collection from a block of memory
		/// </summary>
		public static BundleTypeCollection Read(ReadOnlyMemory<byte> data)
		{
			BlobType[] types = new BlobType[data.Length / BlobType.NumBytes];
			for (int idx = 0; idx < types.Length; idx++)
		{
				types[idx] = BlobType.Read(data.Span.Slice(idx * BlobType.NumBytes));
			}

			return new BundleTypeCollection(types);
		}

		/// <summary>
		/// Reads a type collection from a stream
		/// </summary>
		public static async Task<BundleTypeCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return Read(data);
		}

		/// <summary>
		/// Gets the size of memory required to serialize a collection of types
		/// </summary>
		/// <returns>Number of bytes required to serialize the types</returns>
		public int Measure() => _types.Length * BlobType.NumBytes;

		/// <summary>
		/// Serializes a set of types to a fixed block of memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(IMemoryWriter writer)
		{
			Span<byte> span = writer.GetSpanAndAdvance(_types.Length * BlobType.NumBytes);
			foreach (BlobType type in _types)
			{
				type.Write(span);
				span = span.Slice(BlobType.NumBytes);
			}
		}

		/// <inheritdoc/>
		public BlobType this[int index] => _types[index];

		/// <inheritdoc/>
		public IEnumerator<BlobType> GetEnumerator() => ((IEnumerable<BlobType>)_types).GetEnumerator();

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
	}

	/// <summary>
	/// Collection of imported node references
	/// </summary>
	public struct BundleImportCollection : IReadOnlyList<BundleLocator>
	{
		readonly BundleLocator[] _imports;

		/// <inheritdoc/>
		public int Count => _imports.Length;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleImportCollection(BundleLocator[] imports) => _imports = imports;

		/// <summary>
		/// Reads a collection from memory
		/// </summary>
		public static BundleImportCollection Read(ReadOnlyMemory<byte> data)
		{
			ReadOnlySpan<byte> span = data.Span;
			int count = (span.Length == 0) ? 0 : BinaryPrimitives.ReadInt32LittleEndian(span) / sizeof(int);

			BundleLocator[] imports = new BundleLocator[count];
			for (int idx = 0; idx < count; idx++)
		{
				int offset = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(idx * sizeof(int)));
				int length = span.Slice(offset).IndexOf((byte)0);
				imports[idx] = new BundleLocator(new Utf8String(data.Slice(offset, length)));
			}

			return new BundleImportCollection(imports);
		}

		/// <summary>
		/// Reads a collection from a stream
		/// </summary>
		public static async Task<BundleImportCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return Read(data);
		}

		/// <inheritdoc/>
		public BundleLocator this[int index] => _imports[index];

		/// <inheritdoc/>
		public IEnumerator<BundleLocator> GetEnumerator() => _imports.AsEnumerable().GetEnumerator();

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Measure the size of memory required to store a collection of import locators
		/// </summary>
		/// <returns>Size in bytes of the output buffer</returns>
		public int Measure() => _imports.Sum(x => sizeof(int) + x.Path.Length + 1);

		/// <summary>
		/// Serialize a collection of locators to memory
		/// </summary>
		/// <param name="writer">Writer for output data</param>
		public void Write(IMemoryWriter writer)
		{
			int offset = _imports.Length * sizeof(int);
			foreach (BundleLocator import in _imports)
			{
				writer.WriteInt32(offset);
				offset += import.Path.Length + 1;
			}
			foreach (BundleLocator import in _imports)
			{
				writer.WriteNullTerminatedUtf8String(import.Path);
			}
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
		/// Read from a byte array
		/// </summary>
		public static BundlePacket Read(ReadOnlySpan<byte> span)
		{
			BundleCompressionFormat compressionFormat = (BundleCompressionFormat)BinaryPrimitives.ReadInt32LittleEndian(span);
			int encodedOffset = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(4));
			int encodedLength = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(8));
			int decodedLength = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(12));
			return new BundlePacket(compressionFormat, encodedOffset, encodedLength, decodedLength);
		}

		/// <summary>
		/// Serialize the struct to memory
		/// </summary>
		public void Write(Span<byte> span)
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
		readonly BundlePacket[] _packets;

		/// <inheritdoc/>
		public int Count => _packets.Length;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundlePacketCollection(BundlePacket[] packets) => _packets = packets;

		/// <summary>
		/// Reads a collection from memory
		/// </summary>
		public static BundlePacketCollection Read(ReadOnlyMemory<byte> data)
		{
			int count = data.Length / BundlePacket.NumBytes;

			BundlePacket[] packets = new BundlePacket[count];
			for (int idx = 0; idx < count; idx++)
			{
				packets[idx] = BundlePacket.Read(data.Span.Slice(idx * BundlePacket.NumBytes));
			}

			return new BundlePacketCollection(packets);
		}

		/// <summary>
		/// Reads a collection from a stream
		/// </summary>
		public static async Task<BundlePacketCollection> ReadAsync(Stream stream, int length, CancellationToken cancellationToken)
		{
			byte[] data = new byte[length];
			await stream.ReadFixedLengthBytesAsync(data, cancellationToken);
			return BundlePacketCollection.Read(data);
		}

		/// <inheritdoc/>
		public BundlePacket this[int index] => _packets[index];

		/// <inheritdoc/>
		public IEnumerator<BundlePacket> GetEnumerator() => _packets.AsEnumerable().GetEnumerator();

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Measure the size of memory required to store a collection of packets
		/// </summary>
		/// <returns>Size in bytes of the output buffer</returns>
		public int Measure() => BundlePacket.NumBytes * _packets.Length;

		/// <summary>
		/// Serialize a collection of packets to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(IMemoryWriter writer)
		{
			Span<byte> span = writer.GetMemoryAndAdvance(_packets.Length * BundlePacket.NumBytes).Span;
			foreach (BundlePacket packet in _packets)
			{
				packet.Write(span);
				span = span.Slice(BundlePacket.NumBytes);
			}
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
		public const int NumHeaderBytes = 32;

		/// <summary>
		/// Raw data for this export
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Raw data for the header
		/// </summary>
		public ReadOnlyMemory<byte> HeaderData => Data.Slice(0, NumHeaderBytes);

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
		public BundleExportRefCollection References => new BundleExportRefCollection(Data.Slice(32));

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExport(ReadOnlyMemory<byte> data)
		{
			Data = data;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExport(int typeIdx, IoHash hash, int packetIdx, int offset, int length, IReadOnlyList<BundleExportRef> references)
		{
			byte[] data = new byte[NumHeaderBytes + (BundleExportRef.NumBytes * references.Count)];
			WriteHeader(data, typeIdx, hash, packetIdx, offset, length);

			for (int idx = 0; idx < references.Count; idx++)
			{
				references[idx].CopyTo(data.AsSpan(NumHeaderBytes + (BundleExportRef.NumBytes * idx)));
			}

			Data = data;
		}

		/// <summary>
		/// Writes a new export to a block of memory
		/// </summary>
		public static void WriteHeader(Span<byte> data, int typeIdx, IoHash hash, int packet, int offset, int length)
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
	public struct BundleExportCollection : IReadOnlyList<BundleExport>
	{
		readonly BundleExport[] _exports;

		/// <inheritdoc/>
		public int Count => _exports.Length;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExportCollection(BundleExport[] exports) => _exports = exports;

		/// <summary>
		/// Constructor
		/// </summary>
		public static BundleExportCollection Read(ReadOnlyMemory<byte> data, ReadOnlyMemory<byte> refs, BundleVersion version)
		{
			int count = data.Length / BundleExport.NumHeaderBytes;

			if (version < BundleVersion.ImportHashes && refs.Length > 0)
			{
				const int OldRefNumBytes = 4;

				int headerLength = BinaryPrimitives.ReadInt32LittleEndian(refs.Span);
				int refCount = (refs.Length - headerLength) / OldRefNumBytes;

				byte[] newRefs = new byte[headerLength + (refCount * BundleExportRef.NumBytes)];
				for (int headerOffset = 0; headerOffset < headerLength; headerOffset += sizeof(int))
				{
					int index = (BinaryPrimitives.ReadInt32LittleEndian(refs.Slice(headerOffset).Span) - headerLength) / OldRefNumBytes;
					BinaryPrimitives.WriteInt32LittleEndian(newRefs.AsSpan(headerOffset), headerLength + (index * BundleExportRef.NumBytes));
				}
				for (int index = 0; index < refCount; index++)
				{
					ReadOnlyMemory<byte> source = refs.Slice(headerLength + (index * OldRefNumBytes), OldRefNumBytes);
					source.CopyTo(newRefs.AsMemory(headerLength + (index * BundleExportRef.NumBytes)));
				}

				refs = newRefs;
			}

			BundleExport[] exports = new BundleExport[count];
			for(int index = 0; index < count; index++)
			{
				ReadOnlyMemory<byte> exportData = data.Slice(index * BundleExport.NumHeaderBytes, BundleExport.NumHeaderBytes);

				BundleExportRefCollection exportRefs = new BundleExportRefCollection();
				if (refs.Length > 0)
				{
					int minOffset = BinaryPrimitives.ReadInt32LittleEndian(refs.Span.Slice(index * sizeof(int)));
					int maxOffset = (index == count - 1) ? refs.Length : BinaryPrimitives.ReadInt32LittleEndian(refs.Span.Slice((index + 1) * sizeof(int)));
					exportRefs = new BundleExportRefCollection(refs.Slice(minOffset, maxOffset - minOffset));
				}

				byte[] export = new byte[BundleExport.NumHeaderBytes + exportRefs.Data.Length];
				exportData.CopyTo(export);
				exportRefs.Data.CopyTo(export.AsMemory(BundleExport.NumHeaderBytes));

				exports[index] = new BundleExport(export);
			}

			return new BundleExportCollection(exports);
		}

		/// <summary>
		/// Reads an export collection from a stream
		/// </summary>
		public static async Task<BundleExportCollection> ReadAsync(Stream stream, int sectionLength, CancellationToken cancellationToken)
		{
			BundleExport[] exports = Array.Empty<BundleExport>();
			if (sectionLength > 0)
			{
				const int MaxPageLength = 16384;

				// Read the offsets of each entry
				byte[] headerBuffer = new byte[sizeof(int)];
				await stream.ReadFixedLengthBytesAsync(headerBuffer, cancellationToken);

				int headerLength = BinaryPrimitives.ReadInt32LittleEndian(headerBuffer);
				Array.Resize(ref headerBuffer, headerLength);

				await stream.ReadFixedLengthBytesAsync(headerBuffer.AsMemory(sizeof(int)), cancellationToken);

				// Create the packet array
				int exportCount = headerLength / sizeof(int);
				exports = new BundleExport[exportCount];

				// Read the exports in pages
				int exportIdx = 0;
				for (int exportOffset = headerLength; exportOffset < sectionLength;)
				{
					int pageOffset = exportOffset;

					// Find the exports to read into this page
					int nextPageOffset = sectionLength;
					for (int nextExportIdx = exportIdx + 1; nextExportIdx < exportCount; nextExportIdx++)
					{
						int nextExportOffset = BinaryPrimitives.ReadInt32LittleEndian(headerBuffer.AsSpan(nextExportIdx * sizeof(int)));
						if (nextExportOffset > pageOffset + MaxPageLength)
						{
							nextPageOffset = nextExportOffset;
							break;
						}
					}

					// Read the page data
					byte[] pageBuffer = new byte[nextPageOffset - pageOffset];
					await stream.ReadFixedLengthBytesAsync(pageBuffer, cancellationToken);

					// Parse the individual exports
					for (; exportOffset < nextPageOffset; exportIdx++)
					{
						int startExportOffset = exportOffset;
						if (exportIdx + 1 < exportCount)
						{
							exportOffset = BinaryPrimitives.ReadInt32LittleEndian(headerBuffer.AsSpan((exportIdx + 1) * sizeof(int)));
						}
						else
						{
							exportOffset = nextPageOffset;
						}
						exports[exportIdx] = new BundleExport(pageBuffer.AsMemory(startExportOffset - pageOffset, exportOffset - startExportOffset));
					}
				}
			}
			return new BundleExportCollection(exports);
		}

		/// <inheritdoc/>
		public BundleExport this[int index] => _exports[index];

		/// <inheritdoc/>
		public IEnumerator<BundleExport> GetEnumerator() => _exports.AsEnumerable().GetEnumerator();

		/// <inheritdoc/>
		IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

		/// <summary>
		/// Serializes this collection as a single section
		/// </summary>
		public void Write(IMemoryWriter writer)
		{
			int offset = _exports.Length * sizeof(int);
			Span<byte> index = writer.GetSpanAndAdvance(offset);

			foreach (BundleExport export in _exports)
			{
				BinaryPrimitives.WriteInt32LittleEndian(index, offset);
				index = index.Slice(sizeof(int));

				writer.WriteFixedLengthBytes(export.Data.Span);
				offset += export.Data.Length;
			}
		}

		/// <summary>
		/// Measure the size of memory required to store a collection of exports
		/// </summary>
		/// <returns>Size in bytes of the output buffer</returns>
		public int MeasureHeaders() => BundleExport.NumHeaderBytes * _exports.Length;

		/// <summary>
		/// Serialize a collection of exports to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void WriteHeaders(IMemoryWriter writer)
		{
			foreach (BundleExport export in _exports)
			{
				writer.WriteFixedLengthBytes(export.HeaderData.Span);
			}
		}

		/// <summary>
		/// Measure the size of memory required to store a collection of export refs
		/// </summary>
		/// <returns>Size in bytes of the output buffer</returns>
		public int MeasureRefs() => (sizeof(int) * _exports.Length) + _exports.Sum(x => x.References.Data.Length);

		/// <summary>
		/// Serialize a collection of export refs to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void WriteRefs(IMemoryWriter writer)
		{
			int offset = sizeof(int) * _exports.Length;
			foreach(BundleExport export in _exports)
			{
				writer.WriteInt32(offset);
				offset += export.References.Data.Length;
			}
			foreach (BundleExport export in _exports)
			{
				writer.WriteFixedLengthBytes(export.References.Data.Span);
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
						int maxSize = Oodle.MaximumOutputSize(OodleCompressorType.Selkie, input.Length);

						Span<byte> outputSpan = writer.GetSpan(maxSize);
						int encodedLength = Oodle.Compress(OodleCompressorType.Selkie, input.Span, outputSpan, OodleCompressionLevel.Normal);
						writer.Advance(encodedLength);

						return encodedLength;
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

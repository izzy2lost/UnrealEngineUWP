// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Representation of a data stream, split into chunks along content-aware boundaries using a rolling hash (<see cref="BuzHash"/>).
	/// Chunks are pushed into a tree hierarchy as data is appended to the root, with nodes of the tree also split along content-aware boundaries with <see cref="IoHash.NumBytes"/> granularity.
	/// Once a chunk has been written to storage, it is treated as immutable.
	/// </summary>
	public abstract class ChunkedDataNode : Node
	{
		/// <summary>
		/// Copies the contents of this node and its children to the given output stream
		/// </summary>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken);

		static readonly Guid s_leafNodeGuid = Node.GetNodeType<LeafChunkedDataNode>().Guid;
		static readonly Guid s_interiorNodeGuid = Node.GetNodeType<InteriorChunkedDataNode>().Guid;

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="handle">Handle to the data to read</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(IBlobHandle handle, Stream outputStream, CancellationToken cancellationToken)
		{
			using BlobData blobData = await handle.ReadAsync(cancellationToken);
			if (blobData.Type.Guid == s_leafNodeGuid)
			{
				await LeafChunkedDataNode.CopyToStreamAsync(blobData, outputStream, cancellationToken);
			}
			else if (blobData.Type.Guid == s_interiorNodeGuid)
			{
				await InteriorChunkedDataNode.CopyToStreamAsync(blobData, outputStream, cancellationToken);
			}
			else
			{
				throw new ArgumentException($"Unexpected {nameof(ChunkedDataNode)} type found.");
			}
		}

		/// <summary>
		/// Extracts the contents of this node to a file
		/// </summary>
		/// <param name="handle">Handle to the data to read</param>
		/// <param name="file">File to write with the contents of this node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task CopyToFileAsync(IBlobHandle handle, FileInfo file, CancellationToken cancellationToken)
		{
			if(file.Exists && (file.Attributes & FileAttributes.ReadOnly) != 0)
			{
				file.Attributes &= ~FileAttributes.ReadOnly;
			}
			using (FileStream stream = file.Open(FileMode.Create, FileAccess.Write, FileShare.Read))
			{
				await CopyToStreamAsync(handle, stream, cancellationToken);
			}
		}

		/// <summary>
		/// Serialize this node and its children into a byte array
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns>Array of data stored by the tree</returns>
		public async Task<byte[]> ToByteArrayAsync(CancellationToken cancellationToken)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await CopyToStreamAsync(stream, cancellationToken);
				return stream.ToArray();
			}
		}
	}

	/// <summary>
	/// Type of a chunked data node
	/// </summary>
	public enum ChunkedDataNodeType
	{
		/// <summary>
		/// Unknown node type
		/// </summary>
		Unknown,

		/// <summary>
		/// Leaf node
		/// </summary>
		Leaf,

		/// <summary>
		/// An interior node
		/// </summary>
		Interior
	}

	/// <summary>
	/// Reference to a chunked data node
	/// </summary>
	/// <param name="Type">Type of the referenced node</param>
	/// <param name="Hash">Hash of the target node</param>
	/// <param name="Length">Length of the data stream within this node</param>
	/// <param name="Handle">Handle to the target node</param>
	public record class ChunkedDataNodeRef(ChunkedDataNodeType Type, IoHash Hash, long Length, IBlobHandle Handle)
	{
		/// <summary>
		/// Read the node which is the target of this ref
		/// </summary>
		public ValueTask<ChunkedDataNode> ExpandAsync(CancellationToken cancellationToken = default)
			=> Handle.ReadNodeAsync<ChunkedDataNode>(cancellationToken);
	}

	/// <summary>
	/// Stores the flat list of chunks produced from chunking a single data stream
	/// </summary>
	/// <param name="Hash">Hash of the data</param>
	/// <param name="LeafHandles">Handles to the leaf chunks</param>
	public record class LeafChunkedData(IoHash Hash, List<ChunkedDataNodeRef> LeafHandles);

	/// <summary>
	/// File node that contains a chunk of data
	/// </summary>
	[BlobType("{B27AFB68-4A4B-9E20-8A78-D8A439D49840}", 1)]
	public sealed class LeafChunkedDataNode : ChunkedDataNode
	{
		/// <summary>
		/// Static accessor for the blob type
		/// </summary>
		public static BlobType BlobType { get; } = Node.GetNodeType<LeafChunkedDataNode>();

		/// <summary>
		/// Data for this node
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Create an empty leaf node
		/// </summary>
		public LeafChunkedDataNode()
		{
		}

		/// <summary>
		/// Create a leaf node from the given serialized data
		/// </summary>
		public LeafChunkedDataNode(IBlobReader reader)
		{
			// Keep this code in sync with CopyToStreamAsync
			Data = reader.GetMemory().ToArray();
		}

		/// <inheritdoc/>
		public override void Serialize(IBlobWriter writer)
		{
			writer.WriteFixedLengthBytes(Data.Span);
		}

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken)
		{
			await outputStream.WriteAsync(Data, cancellationToken);
		}

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="nodeData">The raw node data</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(BlobData nodeData, Stream outputStream, CancellationToken cancellationToken)
		{
			// Keep this code in sync with the constructor
			await outputStream.WriteAsync(nodeData.Data, cancellationToken);
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer">Writer for output nodes</param>
		/// <param name="file">File info</param>
		/// <param name="options">Options for finding chunk boundaries</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of the full file data</returns>
		public static async Task<LeafChunkedData> CreateFromFileAsync(IStorageWriter writer, FileReference file, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			using (FileStream stream = FileReference.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				return await CreateFromStreamAsync(writer, stream, options, null, cancellationToken);
			}
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer">Writer for output nodes</param>
		/// <param name="file">File info</param>
		/// <param name="options">Options for finding chunk boundaries</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of the full file data</returns>
		public static async Task<LeafChunkedData> CreateFromFileAsync(IStorageWriter writer, FileInfo file, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			using (FileStream stream = file.Open(FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				return await CreateFromStreamAsync(writer, stream, options, null, cancellationToken);
			}
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer">Writer for output nodes</param>
		/// <param name="stream">Stream to read from</param>
		/// <param name="options">Options for finding chunk boundaries</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of the full file data</returns>
		public static Task<LeafChunkedData> CreateFromStreamAsync(IStorageWriter writer, Stream stream, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			return CreateFromStreamAsync(writer, stream, options, null, cancellationToken);
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer">Writer for output nodes</param>
		/// <param name="stream">Stream to read from</param>
		/// <param name="options">Options for finding chunk boundaries</param>
		/// <param name="copyStats">Stats for the copy operation</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of the full file data</returns>
		internal static async Task<LeafChunkedData> CreateFromStreamAsync(IStorageWriter writer, Stream stream, LeafChunkedDataNodeOptions options, CopyStats? copyStats, CancellationToken cancellationToken)
		{
			using Blake3.Hasher hasher = Blake3.Hasher.New();
			using IMemoryOwner<byte> readBuffer = MemoryPool<byte>.Shared.Rent(options.MaxSize);

			List<ChunkedDataNodeRef> leafNodeRefs = new List<ChunkedDataNodeRef>();

			int size = 0;
			int sizeSinceProgressUpdate = 0;
			for (; ; )
			{
				size += await stream.ReadGreedyAsync(readBuffer.Memory.Slice(size), cancellationToken);
				if (size == 0 && leafNodeRefs.Count > 0)
				{
					break;
				}

				int nextLength = GetChunkLength(readBuffer.Memory.Span.Slice(0, size), options);

				Memory<byte> outputBuffer = writer.GetOutputBuffer(0, nextLength);
				readBuffer.Memory.Slice(0, nextLength).CopyTo(outputBuffer);
				hasher.Update(outputBuffer.Span);

				sizeSinceProgressUpdate += nextLength;
				if (sizeSinceProgressUpdate > 512 * 1024)
				{
					copyStats?.Update(0, sizeSinceProgressUpdate);
					sizeSinceProgressUpdate = 0;
				}

				HashedNodeRef<ChunkedDataNode> nodeRef = await writer.WriteHashedNodeRefAsync<ChunkedDataNode>(GetNodeType<LeafChunkedDataNode>(), nextLength, Array.Empty<IBlobHandle>(), cancellationToken);
				leafNodeRefs.Add(new ChunkedDataNodeRef(ChunkedDataNodeType.Leaf, nodeRef.Hash, nextLength, nodeRef.Handle));

				readBuffer.Memory.Slice(nextLength, size - nextLength).CopyTo(readBuffer.Memory);
				size -= nextLength;
			}

			copyStats?.Update(1, sizeSinceProgressUpdate);

			IoHash hash = IoHash.FromBlake3(hasher);
			return new LeafChunkedData(hash, leafNodeRefs);
		}

		/// <summary>
		/// Determines how much data to append to an existing leaf node
		/// </summary>
		/// <param name="inputData">Data to be appended</param>
		/// <param name="options">Options for chunking the data</param>
		/// <returns>The number of bytes to append</returns>
		public static int GetChunkLength(ReadOnlySpan<byte> inputData, LeafChunkedDataNodeOptions options)
		{
			// If the target option sizes are fixed, just chunk the data along fixed boundaries
			if (options.MinSize == options.TargetSize && options.MaxSize == options.TargetSize)
			{
				return Math.Min(inputData.Length, options.MaxSize);
			}

			// Cap the append data span to the maximum amount we can add
			int maxLength = options.MaxSize;
			if (maxLength < inputData.Length)
			{
				inputData = inputData.Slice(0, maxLength);
			}

			int windowSize = options.MinSize;

			// Fast path for appending data to the buffer up to the chunk window size
			int length = Math.Min(windowSize, inputData.Length);
			uint rollingHash = BuzHash.Add(0, inputData.Slice(0, length));

			// Get the threshold for the rolling hash to split the output
			uint rollingHashThreshold = (uint)((1L << 32) / options.TargetSize);

			// Step through the rest of the data which is completely contained in appendData.
			if (length < inputData.Length)
			{
				Debug.Assert(length >= windowSize);

				ReadOnlySpan<byte> tailSpan = inputData.Slice(length - windowSize, inputData.Length - windowSize);
				ReadOnlySpan<byte> headSpan = inputData.Slice(length);

				int count = BuzHash.Update(tailSpan, headSpan, rollingHashThreshold, ref rollingHash);
				if (count != -1)
				{
					length += count;
					return length;
				}

				length += headSpan.Length;
			}

			return length;
		}
	}

	/// <summary>
	/// Options for creating interior nodes
	/// </summary>
	/// <param name="MinChildCount">Minimum number of children in each node</param>
	/// <param name="TargetChildCount">Target number of children in each node</param>
	/// <param name="MaxChildCount">Maximum number of children in each node</param>
	/// <param name="SliceThreshold">Threshold hash value for splitting interior nodes</param>
	public record class InteriorChunkedDataNodeOptions(int MinChildCount, int TargetChildCount, int MaxChildCount, uint SliceThreshold)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public InteriorChunkedDataNodeOptions(int minChildCount, int targetChildCount, int maxChildCount)
			: this(minChildCount, targetChildCount, maxChildCount, (uint)((1UL << 32) / (uint)targetChildCount))
		{
		}
	}

	/// <summary>
	/// An interior file node
	/// </summary>
	[BlobType("{F4DEDDBC-4C7A-70CB-11F0-4783B9CDCCAF}", 2)] // Pending V3
	public class InteriorChunkedDataNode : ChunkedDataNode
	{
		/// <summary>
		/// Static accessor for the blob type
		/// </summary>
		public static BlobType BlobType { get; } = Node.GetNodeType<InteriorChunkedDataNode>();

		/// <summary>
		/// Child nodes
		/// </summary>
		public IReadOnlyList<ChunkedDataNodeRef> Children { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="children"></param>
		public InteriorChunkedDataNode(IReadOnlyList<ChunkedDataNodeRef> children)
		{
			Children = children;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public InteriorChunkedDataNode(IBlobReader reader)
		{
			// Keep this code in sync with CopyToStreamAsync
			List<ChunkedDataNodeRef> children = new List<ChunkedDataNodeRef>();
			while (reader.GetMemory().Length > 0)
			{
				IoHash hash = reader.ReadIoHash();

				ChunkedDataNodeType type = ChunkedDataNodeType.Unknown;
				if (reader.Version >= 2)
				{
					type = (ChunkedDataNodeType)reader.ReadUnsignedVarInt();
				}

				long length = 0;
				if (reader.Version >= 3)
				{
					length = (long)reader.ReadUnsignedVarInt();
				}

				IBlobHandle handle = reader.ReadBlobReference();

				children.Add(new ChunkedDataNodeRef(type, hash, length, handle));
			}
			Children = children;
		}

		/// <inheritdoc/>
		public override void Serialize(IBlobWriter writer)
		{
			foreach (ChunkedDataNodeRef child in Children)
			{
				writer.WriteIoHash(child.Hash);
				writer.WriteUnsignedVarInt((int)child.Type);
				// Pending V3: writer.WriteUnsignedVarInt((ulong)child.Length);
				writer.WriteBlobReference(child.Handle);
			}
		}

		/// <summary>
		/// Create a tree of nodes from the given list of handles, splitting nodes in each layer based on the hash of the last node.
		/// </summary>
		/// <param name="leafChunkedData">List of leaf handles</param>
		/// <param name="options">Options for splitting the tree</param>
		/// <param name="writer">Output writer for new interior nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root node of the tree</returns>
		public static async Task<ChunkedData> CreateTreeAsync(LeafChunkedData leafChunkedData, InteriorChunkedDataNodeOptions options, IStorageWriter writer, CancellationToken cancellationToken)
		{
			ChunkedDataNodeRef rootRef = await CreateTreeAsync(leafChunkedData.LeafHandles, options, writer, cancellationToken);
			return new ChunkedData(leafChunkedData.Hash, rootRef);
		}

		/// <summary>
		/// Create a tree of nodes from the given list of handles, splitting nodes in each layer based on the hash of the last node.
		/// </summary>
		/// <param name="nodeRefs">List of leaf nodes</param>
		/// <param name="options">Options for splitting the tree</param>
		/// <param name="writer">Output writer for new interior nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root node of the tree</returns>
		public static async Task<ChunkedDataNodeRef> CreateTreeAsync(List<ChunkedDataNodeRef> nodeRefs, InteriorChunkedDataNodeOptions options, IStorageWriter writer, CancellationToken cancellationToken)
		{
			List<ChunkedDataNodeRef> handleBuffer = new List<ChunkedDataNodeRef>();

			List<(InteriorChunkedDataNode, long)> interiorNodes = new List<(InteriorChunkedDataNode, long)>();
			while (nodeRefs.Count > 1)
			{
				interiorNodes.Clear();
				CreateTreeLayer(nodeRefs, options, interiorNodes);

				handleBuffer.Clear();
				foreach ((InteriorChunkedDataNode interiorNode, long interiorLength) in interiorNodes)
				{
					HashedNodeRef<ChunkedDataNode> newNodeRef = await writer.WriteHashedNodeAsync<ChunkedDataNode>(interiorNode, cancellationToken);
					handleBuffer.Add(new ChunkedDataNodeRef(ChunkedDataNodeType.Interior, newNodeRef.Hash, interiorLength, newNodeRef.Handle));
				}

				nodeRefs = handleBuffer;
			}

			return nodeRefs[0];
		}

		/// <summary>
		/// Split a list of leaf handles into a layer of interior nodes
		/// </summary>
		static void CreateTreeLayer(List<ChunkedDataNodeRef> nodeRefs, InteriorChunkedDataNodeOptions options, List<(InteriorChunkedDataNode, long)> interiorNodes)
		{
			Span<byte> buffer = stackalloc byte[IoHash.NumBytes];

			for (int index = 0; index < nodeRefs.Count; )
			{
				int minIndex = index;
				int maxIndex = Math.Min(minIndex + options.MaxChildCount, nodeRefs.Count);

				index = Math.Min(index + options.MinChildCount, nodeRefs.Count);
				for (; index < maxIndex; index++)
				{
					nodeRefs[index].Hash.CopyTo(buffer);

					uint value = BinaryPrimitives.ReadUInt32LittleEndian(buffer);
					if (value < options.SliceThreshold)
					{
						break;
					}
				}

				ChunkedDataNodeRef[] children = new ChunkedDataNodeRef[index - minIndex];
				for (int childIndex = minIndex; childIndex < index; childIndex++)
				{
					children[childIndex - minIndex] = nodeRefs[childIndex];
				}

				long interiorLength = children.Sum(x => x.Length);
				interiorNodes.Add((new InteriorChunkedDataNode(children), interiorLength));
			}
		}

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken)
		{
			foreach (ChunkedDataNodeRef childNodeRef in Children)
			{
				ChunkedDataNode childNode = await childNodeRef.ExpandAsync(cancellationToken);
				await childNode.CopyToStreamAsync(outputStream, cancellationToken);
			}
		}

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="nodeData">Source data</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(BlobData nodeData, Stream outputStream, CancellationToken cancellationToken)
		{
			BlobReader nodeReader = new BlobReader(nodeData);
			while (nodeReader.GetMemory(0).Length > 0)
			{
				_ = nodeReader.ReadIoHash();
				IBlobHandle handle = nodeReader.ReadBlobReference();
				if (nodeReader.Version >= 2)
				{
					_ = nodeReader.ReadUnsignedVarInt();
				}
				await ChunkedDataNode.CopyToStreamAsync(handle, outputStream, cancellationToken);
			}
		}
	}
}

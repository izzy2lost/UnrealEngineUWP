// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Stores a reference to a <see cref="Node"/> object in the storage system.
	/// </summary>
	public class NodeRef
	{
		/// <summary>
		/// Handle to the node if in storage (or pending write to storage)
		/// </summary>
		public IBlobHandle Handle { get; }

		/// <summary>
		/// Creates a reference to a node in storage.
		/// </summary>
		/// <param name="handle">Handle to the referenced node</param>
		public NodeRef(IBlobHandle handle) => Handle = handle;

		/// <summary>
		/// Creates a reference to a node in storage.
		/// </summary>
		/// <param name="other">Other noderef to copy from</param>
		public NodeRef(NodeRef other) => Handle = other.Handle;

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader"></param>
		public NodeRef(INodeReader reader) => Handle = reader.ReadBlobReference();

		/// <summary>
		/// Serialize the node to the given writer
		/// </summary>
		/// <param name="writer"></param>
		public virtual void Serialize(INodeWriter writer) => writer.WriteBlobReference(Handle);

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<Node> ExpandAsync(CancellationToken cancellationToken = default)
		{
			using BlobData blobData = await Handle!.ReadAsync(cancellationToken);
			return Node.Deserialize(blobData);
		}
	}

	/// <summary>
	/// Strongly typed reference to a <see cref="Node"/>
	/// </summary>
	/// <typeparam name="T">Type of the node</typeparam>
	public class NodeRef<T> : NodeRef where T : Node
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public NodeRef(IBlobHandle handle) : base(handle)
		{ }

		/// <summary>
		/// Constructor
		/// </summary>
		public NodeRef(NodeRef<T> other) : base(other)
		{ }

		/// <summary>
		/// Constructor
		/// </summary>
		public NodeRef(INodeReader reader) : base(reader)
		{ }

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public new async ValueTask<T> ExpandAsync(CancellationToken cancellationToken = default) => (T)await base.ExpandAsync(cancellationToken);
	}

	/// <summary>
	/// Stores a reference to a <see cref="Node"/> object in the storage system.
	/// </summary>
	public class HashedNodeRef : NodeRef
	{
		/// <summary>
		/// Hash of the referenced node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Creates a reference to a node in storage.
		/// </summary>
		/// <param name="hash">Hash of the target node</param>
		/// <param name="handle">Handle to the referenced node</param>
		public HashedNodeRef(IoHash hash, IBlobHandle handle) : base(handle) => Hash = hash;

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader"></param>
		public HashedNodeRef(INodeReader reader) : this(reader.ReadIoHash(), reader.ReadBlobReference())
		{ }

		/// <summary>
		/// Serialize the node to the given writer
		/// </summary>
		/// <param name="writer"></param>
		public override void Serialize(INodeWriter writer)
		{
			base.Serialize(writer);
			writer.WriteIoHash(Hash);
		}
	}

	/// <summary>
	/// Strongly typed reference to a <see cref="Node"/>
	/// </summary>
	/// <typeparam name="T">Type of the node</typeparam>
	public class HashedNodeRef<T> : HashedNodeRef where T : Node
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public HashedNodeRef(IoHash hash, IBlobHandle handle) : base(hash, handle)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public HashedNodeRef(HashedNodeRef<T> other) : base(other.Hash, other.Handle)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public HashedNodeRef(INodeReader reader) : base(reader)
		{
		}

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public new async ValueTask<T> ExpandAsync(CancellationToken cancellationToken = default)
		{
			return (T)await base.ExpandAsync(cancellationToken);
		}
	}

	/// <summary>
	/// Extension methods for writing node
	/// </summary>
	public static class NodeRefExtensions
	{
		/// <summary>
		/// Read an untyped ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New untyped ref</returns>
		public static NodeRef ReadNodeRef(this INodeReader reader)
		{
			return new NodeRef(reader);
		}

		/// <summary>
		/// Read a strongly typed ref from the reader
		/// </summary>
		/// <typeparam name="T">Type of the referenced node</typeparam>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New strongly typed ref</returns>
		public static NodeRef<T> ReadNodeRef<T>(this INodeReader reader) where T : Node
		{
			return new NodeRef<T>(reader);
		}

		/// <summary>
		/// Read an untyped ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New untyped ref</returns>
		public static HashedNodeRef ReadHashedNodeRef(this INodeReader reader)
		{
			return new HashedNodeRef(reader);
		}

		/// <summary>
		/// Read a strongly typed ref from the reader
		/// </summary>
		/// <typeparam name="T">Type of the referenced node</typeparam>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New strongly typed ref</returns>
		public static HashedNodeRef<T> ReadHashedNodeRef<T>(this INodeReader reader) where T : Node
		{
			return new HashedNodeRef<T>(reader);
		}

		/// <summary>
		/// Read an optional untyped ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New untyped ref</returns>
		public static NodeRef? ReadOptionalNodeRef(this INodeReader reader)
		{
			if (reader.ReadBoolean())
			{
				return reader.ReadNodeRef();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Read an optional strongly typed ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New strongly typed ref</returns>
		public static NodeRef<T>? ReadOptionalNodeRef<T>(this INodeReader reader) where T : Node
		{
			if (reader.ReadBoolean())
			{
				return reader.ReadNodeRef<T>();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Writes a ref to storage
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="nodeRef">Value to write</param>
		public static void WriteNodeRef(this INodeWriter writer, NodeRef nodeRef)
		{
			nodeRef.Serialize(writer);
		}

		/// <summary>
		/// Writes a ref to storage
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="nodeRef">Value to write</param>
		public static void WriteHashedNodeRef(this INodeWriter writer, HashedNodeRef nodeRef)
		{
			nodeRef.Serialize(writer);
		}

		/// <summary>
		/// Writes an optional ref value to storage
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteOptionalNodeRef(this INodeWriter writer, NodeRef? value)
		{
			if (value == null)
			{
				writer.WriteBoolean(false);
			}
			else
			{
				writer.WriteBoolean(true);
				writer.WriteNodeRef(value);
			}
		}

		/// <summary>
		/// Writes an individual node to storage
		/// </summary>
		/// <param name="writer">Writer to serialize nodes to</param>
		/// <param name="node">Node to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>A flag indicating whether the node is dirty, and if it is, an optional bundle that contains it</returns>
		public static async ValueTask<HashedNodeRef<TNode>> WriteHashedNodeAsync<TNode>(this IStorageWriter writer, TNode node, CancellationToken cancellationToken = default) where TNode : Node
		{
			// Serialize the node
			NodeWriter nodeWriter = new NodeWriter(writer);
			node.Serialize(nodeWriter);
			IoHash hash = nodeWriter.ComputeHash();

			// Write the final data
			IBlobHandle handle = await writer.WriteBlobAsync(node.NodeType, nodeWriter.Length, nodeWriter.References, cancellationToken);
			return new HashedNodeRef<TNode>(hash, handle);
		}

		/// <summary>
		/// Reads and deserializes a node for the given ref
		/// </summary>
		/// <param name="storageClient"></param>
		/// <param name="refName">Name of the ref to write</param>
		/// <param name="cacheTime"></param>
		/// <param name="cancellationToken"></param>
		public static async ValueTask<NodeRef<TNode>> ReadRefTargetAsync<TNode>(this IStorageClient storageClient, RefName refName, DateTime cacheTime = default, CancellationToken cancellationToken = default) where TNode : Node
		{
			NodeRef<TNode>? result = await TryReadRefTargetAsync<TNode>(storageClient, refName, cacheTime, cancellationToken);
			if (result == null)
			{
				throw new RefNameNotFoundException(refName);
			}
			return result;
		}

		/// <summary>
		/// Reads and deserializes a node for the given ref
		/// </summary>
		/// <param name="storageClient"></param>
		/// <param name="refName">Name of the ref to write</param>
		/// <param name="cacheTime"></param>
		/// <param name="cancellationToken"></param>
		public static async ValueTask<NodeRef<TNode>?> TryReadRefTargetAsync<TNode>(this IStorageClient storageClient, RefName refName, DateTime cacheTime = default, CancellationToken cancellationToken = default) where TNode : Node
		{
			IBlobHandle? handle = await storageClient.TryReadRefTargetAsync(refName, cacheTime, cancellationToken);
			if (handle == null)
			{
				return null;
			}
			return new NodeRef<TNode>(handle);
		}

		/// <summary>
		/// Writes a node to the given ref
		/// </summary>
		/// <param name="storageClient"></param>
		/// <param name="refName">Name of the ref to write</param>
		/// <param name="node"></param>
		/// <param name="refOptions"></param>
		/// <param name="cancellationToken"></param>
		public static async ValueTask WriteRefTargetAsync<TNode>(this IStorageClient storageClient, RefName refName, HashedNodeRef<TNode> node, RefOptions? refOptions = null, CancellationToken cancellationToken = default) where TNode : Node
		{
			await storageClient.WriteRefTargetAsync(refName, node.Handle, refOptions, cancellationToken);
		}

		/// <summary>
		/// Flushes all the current nodes to storage
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="root">Root for the tree</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<IBlobHandle> FlushAsync(this IStorageWriter writer, Node root, CancellationToken cancellationToken = default)
		{
			HashedNodeRef<Node> rootRef = await writer.WriteHashedNodeAsync(root, cancellationToken);
			await writer.FlushAsync(cancellationToken);
			return rootRef.Handle!;
		}
	}
}

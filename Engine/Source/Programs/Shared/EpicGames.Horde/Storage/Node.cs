// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Reflection.Emit;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Attribute used to define a factory for a particular node type
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class NodeTypeAttribute : Attribute // TODO: THIS SHOULD BE NODEATTRIBUTE, not NODETYPEATTRIBUTE
	{
		/// <summary>
		/// Name of the type to store in the bundle header
		/// </summary>
		public string Guid { get; }

		/// <summary>
		/// Version number of the serializer
		/// </summary>
		public int Version { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public NodeTypeAttribute(string guid, int version = 1)
		{
			Guid = guid;
			Version = version;
		}
	}

	/// <summary>
	/// Base class for user-defined types that are stored in a tree
	/// </summary>
	public abstract class Node
	{
		/// <summary>
		/// Revision number of the node. Incremented whenever the node is modified, and used to track whether nodes are modified between 
		/// writes starting and completing.
		/// </summary>
		public uint Revision { get; private set; }

		/// <summary>
		/// Accessor for the bundle type definition associated with this node
		/// </summary>
		public BlobType NodeType => GetNodeType(GetType());

		/// <summary>
		/// Mark this node as dirty
		/// </summary>
		protected void MarkAsDirty()
		{
			Revision++;
		}

		/// <summary>
		/// Serialize the contents of this node
		/// </summary>
		/// <returns>Data for the node</returns>
		public abstract void Serialize(IBlobWriter writer);

		#region Static methods

		static readonly object s_writeLock = new object();
		static readonly ConcurrentDictionary<Type, BlobType> s_typeToNodeType = new ConcurrentDictionary<Type, BlobType>();
		static readonly ConcurrentDictionary<Guid, Type> s_guidToType = new ConcurrentDictionary<Guid, Type>();
		static readonly ConcurrentDictionary<Guid, Func<BlobData, Node>> s_guidToDeserializer = new ConcurrentDictionary<Guid, Func<BlobData, Node>>();

		static BlobType CreateNodeType(NodeTypeAttribute attribute)
		{
			return new BlobType(Guid.Parse(attribute.Guid), attribute.Version);
		}

		/// <summary>
		/// Attempts to get the concrete type with the given node. The type must have been registered via a previous call to <see cref="RegisterType(Type)"/>.
		/// </summary>
		/// <param name="guid">Guid specified in the <see cref="NodeTypeAttribute"/></param>
		/// <param name="type">On success, receives the C# type associated with this GUID</param>
		/// <returns>True if the type was found</returns>
		public static bool TryGetConcreteType(Guid guid, [NotNullWhen(true)] out Type? type) => s_guidToType.TryGetValue(guid, out type);

		/// <summary>
		/// Gets the node type corresponding to the given C# type
		/// </summary>
		/// <param name="type"></param>
		/// <returns></returns>
		public static BlobType GetNodeType(Type type)
		{
			BlobType nodeType;
			if (!s_typeToNodeType.TryGetValue(type, out nodeType))
			{
				lock (s_writeLock)
				{
					if (!s_typeToNodeType.TryGetValue(type, out nodeType))
					{
						NodeTypeAttribute? attribute = type.GetCustomAttribute<NodeTypeAttribute>();
						if (attribute == null)
						{
							throw new InvalidOperationException($"Missing {nameof(NodeTypeAttribute)} from type {type.Name}");
						}
						nodeType = s_typeToNodeType.GetOrAdd(type, CreateNodeType(attribute));
					}
				}
			}
			return nodeType;
		}

		/// <summary>
		/// Gets the type descriptor for the given type
		/// </summary>
		/// <typeparam name="T">Type to get a <see cref="NodeType"/> for</typeparam>
		/// <returns></returns>
		public static BlobType GetNodeType<T>() where T : Node => GetNodeType(typeof(T));

		/// <summary>
		/// Deserialize a node from the given reader
		/// </summary>
		/// <param name="nodeData">Data to deserialize from</param>
		/// <returns>New node instance</returns>
		public static Node Deserialize(BlobData nodeData)
		{
			return s_guidToDeserializer[nodeData.Type.Guid](nodeData);
		}

		/// <summary>
		/// Deserialize a node from the given reader
		/// </summary>
		/// <param name="nodeData">Data to deserialize from</param>
		/// <returns>New node instance</returns>
		public static TNode Deserialize<TNode>(BlobData nodeData) where TNode : Node => (TNode)Deserialize(nodeData);

		/// <summary>
		/// Static constructor. Registers all the types in the current assembly.
		/// </summary>
		static Node()
		{
			RegisterTypesFromAssembly(Assembly.GetExecutingAssembly());
		}

		/// <summary>
		/// Registers a single deserializer
		/// </summary>
		/// <param name="type"></param>
		/// <exception cref="NotImplementedException"></exception>
		public static void RegisterType(Type type)
		{
			NodeTypeAttribute? attribute = type.GetCustomAttribute<NodeTypeAttribute>();
			if (attribute == null)
			{
				throw new InvalidOperationException($"Missing {nameof(NodeTypeAttribute)} from type {type.Name}");
			}
			RegisterType(type, attribute);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <typeparam name="T"></typeparam>
		public static void RegisterType<T>() where T : Node => RegisterType(typeof(T));

		/// <summary>
		/// Register all node types with the <see cref="NodeTypeAttribute"/> from the given assembly
		/// </summary>
		/// <param name="assembly">Assembly to register types from</param>
		public static void RegisterTypesFromAssembly(Assembly assembly)
		{
			Type[] types = assembly.GetTypes();
			lock (s_writeLock)
			{
				foreach (Type type in types)
				{
					if (type.IsClass)
					{
						NodeTypeAttribute? attribute = type.GetCustomAttribute<NodeTypeAttribute>();
						if (attribute != null)
						{
							RegisterType(type, attribute);
						}
					}
				}
			}
		}

		static void RegisterType(Type type, NodeTypeAttribute attribute)
		{
			BlobType nodeType = CreateNodeType(attribute);
			s_typeToNodeType.TryAdd(type, nodeType);

			Func<BlobData, Node> deserializer = CreateDeserializer(type);
			s_guidToType.TryAdd(nodeType.Guid, type);
			s_guidToDeserializer.TryAdd(nodeType.Guid, deserializer);
		}

		static readonly ConstructorInfo? s_nodeReaderTypeCtor = typeof(BlobReader).GetConstructor(new[] { typeof(BlobData) });

		static Func<BlobData, Node> CreateDeserializer(Type type)
		{
			Type[] signature = new[] { typeof(BlobData) };

			ConstructorInfo? nodeDataCtor = type.GetConstructor(signature);
			if (nodeDataCtor != null)
			{
				DynamicMethod method = new DynamicMethod($"Create_{type.Name}", type, signature, true);

				ILGenerator generator = method.GetILGenerator();
				generator.Emit(OpCodes.Ldarg_0);
				generator.Emit(OpCodes.Newobj, nodeDataCtor);
				generator.Emit(OpCodes.Ret);

				return (Func<BlobData, Node>)method.CreateDelegate(typeof(Func<BlobData, Node>));
			}

			ConstructorInfo? nodeReaderCtor = type.GetConstructor(new[] { typeof(BlobReader) });
			if (nodeReaderCtor != null)
			{
				DynamicMethod method = new DynamicMethod($"Create_{type.Name}", type, signature, true);

				ILGenerator generator = method.GetILGenerator();
				generator.Emit(OpCodes.Ldarg_0);
				generator.Emit(OpCodes.Newobj, s_nodeReaderTypeCtor!);
				generator.Emit(OpCodes.Newobj, nodeReaderCtor);
				generator.Emit(OpCodes.Ret);

				return (Func<BlobData, Node>)method.CreateDelegate(typeof(Func<BlobData, Node>));
			}

			throw new InvalidOperationException($"Type {type.Name} does not have a constructor taking a {typeof(BlobData).Name} or {typeof(BlobReader).Name} instance as parameter.");
		}

		#endregion
	}

	/// <summary>
	/// Extension methods for serializing <see cref="Node"/> objects
	/// </summary>
	public static class NodeExtensions
	{
		/// <summary>
		/// Reads and deserializes a node from storage
		/// </summary>
		/// <typeparam name="TNode"></typeparam>
		/// <param name="handle"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async ValueTask<TNode> ReadNodeAsync<TNode>(this IBlobHandle handle, CancellationToken cancellationToken = default) where TNode : Node
		{
			using BlobData blobData = await handle.ReadAsync(cancellationToken);
			return Node.Deserialize<TNode>(blobData);
		}

		/// <summary>
		/// Writes a node to storage
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">Name of the ref containing this node</param>
		/// <param name="node">Node to be written</param>
		/// <param name="refOptions">Options for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Location of node targetted by the ref</returns>
		public static async Task<IBlobHandle> WriteRefAsync(this IStorageClient store, RefName name, Node node, RefOptions? refOptions = null, CancellationToken cancellationToken = default)
		{
			await using IStorageWriter writer = store.CreateWriter(name);
			HashedNodeRef<Node> nodeRef = await writer.WriteHashedNodeAsync(node, cancellationToken);
			await writer.FlushAsync();
			await store.WriteRefTargetAsync(name, nodeRef.Handle, refOptions, cancellationToken);
			return nodeRef.Handle;
		}

		/// <summary>
		/// Reads data for a ref from the store, along with the node's contents.
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node for the given ref, or null if it does not exist</returns>
		public static async Task<TNode?> TryReadRefAsync<TNode>(this IStorageClient store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) where TNode : Node
		{
			IBlobHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (refTarget == null)
			{
				return null;
			}

			using BlobData blobData = await refTarget.ReadAsync(cancellationToken);
			return Node.Deserialize<TNode>(blobData);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static async Task<TNode> ReadRefAsync<TNode>(this IStorageClient store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) where TNode : Node
		{
			TNode? refValue = await store.TryReadRefAsync<TNode>(name, cacheTime, cancellationToken);
			if (refValue == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return refValue;
		}
	}
}

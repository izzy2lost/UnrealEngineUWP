// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[NodeType("{BE09E54F-7A6B-47CA-BCAF-972A38153B18}", 1)]
	public class RedirectNode : Node 
	{
		/// <summary>
		/// The target handle
		/// </summary>
		public IBlobHandle Handle { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="target">Target node for the redirect</param>
		public RedirectNode(NodeRef target) => Handle = target.Handle;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="handle">Target node for the redirect</param>
		public RedirectNode(IBlobHandle handle) => Handle = handle;

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public RedirectNode(IBlobReader reader) => Handle = reader.ReadBlobReference();

		/// <summary>
		/// Gets a typed reference to the target node
		/// </summary>
		public NodeRef ToNodeRef() => new NodeRef(Handle);

		/// <summary>
		/// Gets a typed reference to the target node
		/// </summary>
		public NodeRef<TTarget> ToNodeRef<TTarget>() where TTarget : Node => new NodeRef<TTarget>(Handle);

		/// <inheritdoc/>
		public override void Serialize(IBlobWriter writer) => writer.WriteBlobReference(Handle);
	}
}

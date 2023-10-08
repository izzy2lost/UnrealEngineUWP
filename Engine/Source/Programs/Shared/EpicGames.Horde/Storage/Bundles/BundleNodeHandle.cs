// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Handle to a blob stored in a bundle
	/// </summary>
	public abstract class BundleNodeHandle : BlobHandle
	{
		/// <summary>
		/// Determines if the node has been written to storage
		/// </summary>
		public abstract bool HasLocator();

		/// <summary>
		/// Gets the node locator. May throw if the node has not been written to storage yet.
		/// </summary>
		/// <returns>Locator for the node</returns>
		public abstract BundleNodeLocator GetLocator();

		/// <summary>
		/// Adds a callback to be executed once the node has been written. Triggers immediately if the node has already been written.
		/// </summary>
		/// <param name="callback">Action to be executed after the write</param>
		public abstract void AddWriteCallback(BlobWriteCallback callback);

		/// <inheritdoc/>
		public override string ToString() => HasLocator() ? GetLocator().ToString() : base.ToString() ?? String.Empty;
	}

	/// <summary>
	/// Object to receive notifications on a node being written
	/// </summary>
	public abstract class BlobWriteCallback
	{
		internal BlobWriteCallback? _next;

		/// <summary>
		/// Callback for the node being written
		/// </summary>
		public abstract void OnWrite();
	}
}

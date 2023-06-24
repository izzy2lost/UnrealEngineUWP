// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Handle to a blob stored in a bundle
	/// </summary>
	public abstract class BundleNodeHandle : BlobHandle
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hash">Hash of the node</param>
		protected BundleNodeHandle(IoHash hash) : base(hash)
		{
		}

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

		/// <summary>
		/// Flush the node to storage and retrieve its locator
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<BundleNodeLocator> FlushAsync(CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public override string ToString() => HasLocator() ? GetLocator().ToString() : Hash.ToString();
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

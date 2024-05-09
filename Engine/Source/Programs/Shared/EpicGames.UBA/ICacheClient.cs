// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.UBA
{
	/// <summary>
	/// Base interface for root paths used by cache system to normalize paths
	/// </summary>
	public interface IRootPaths : IBaseInterface
	{
		/// <summary>
		/// Register roots used to normalize paths in caches
		/// </summary>
		public abstract bool RegisterRoot(string path, bool includeInKey);

		/// <summary>
		/// Register system roots used to normalize paths in caches
		/// </summary>
		public abstract bool RegisterSystemRoots();

		/// <summary>
		/// Create root paths instance
		/// </summary>
		public static IRootPaths Create(ILogger logger)
		{
			return new RootPathsImpl(logger);
		}
	}

	/// <summary>
	/// Base interface for a cache client
	/// </summary>
	public interface ICacheClient : IBaseInterface
	{
		/// <summary>
		/// Connect to cache client
		/// </summary>
		/// <param name="host">Cache server address</param>
		/// <param name="port">Cache server port</param>
		/// <returns>True if successful</returns>
		public abstract bool Connect(string host, int port);

		/// <summary>
		/// Write to cache
		/// </summary>
		/// <param name="rootPaths">RootPath instance</param>
		/// <param name="bucket">Bucket to store cache entry</param>
		/// <param name="process">Process</param>
		/// <param name="inputs">Input files</param>
		/// <param name="inputsSize">Input files size</param>
		/// <param name="outputs">Output files</param>
		/// <param name="outputsSize">Output files size</param>
		/// <returns>True if successful</returns>
		public abstract bool WriteToCache(IRootPaths rootPaths, uint bucket, IProcess process, byte[] inputs, uint inputsSize, byte[] outputs, uint outputsSize);

		/// <summary>
		/// Fetch from cache
		/// </summary>
		/// <param name="rootPaths">RootPath instance</param>
		/// <param name="bucket">Bucket to search for cache entry</param>
		/// <param name="info">Process start info</param>
		/// <returns>True if successful</returns>
		public abstract bool FetchFromCache(IRootPaths rootPaths, uint bucket, ProcessStartInfo info);

		/// <summary>
		/// Create a ICacheClient object
		/// </summary>
		/// <param name="session">The session</param>
		/// <param name="reportMissReason">Output reason for cache miss to log.</param>
		/// <returns>The ICacheClient</returns>
		public static ICacheClient CreateCacheClient(ISessionServer session, bool reportMissReason)
		{
			return new CacheClientImpl(session, reportMissReason);
		}
	}
}

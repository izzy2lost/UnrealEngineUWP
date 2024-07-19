// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Secrets;

namespace HordeServer.Secrets
{
	/// <summary>
	/// Collection of secrets
	/// </summary>
	public interface ISecretCollection
	{
		/// <summary>
		/// Resolve a secret to concrete values
		/// </summary>
		/// <param name="secretId">Identifier for the secret</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<ISecret?> GetAsync(SecretId secretId, CancellationToken cancellationToken);
	}
}

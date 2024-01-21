// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Horde
{
	/// <summary>
	/// Base interface for Horde functionality.
	/// </summary>
	public interface IHordeClient
	{
		/// <summary>
		/// Creates a http client 
		/// </summary>
		/// <returns></returns>
		HordeHttpClient CreateHttpClient();
	}
}

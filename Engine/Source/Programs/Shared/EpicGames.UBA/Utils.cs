// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.UBA
{
	/// <summary>
	/// Utils
	/// </summary>
	public static partial class Utils
	{
		static Lazy<bool> Available { get; set; } = new Lazy<bool>(() => false);

		/// <summary>
		/// Is available?
		/// </summary>
		public static bool IsAvailable() => Available.Value;
	}
}

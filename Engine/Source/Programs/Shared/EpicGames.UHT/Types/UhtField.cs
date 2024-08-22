// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Represents a UField
	/// </summary>
	public abstract class UhtField : UhtObject
	{
		/// <inheritdoc/>
		public override string EngineClassName => "Field";

		/// <summary>
		/// Construct a new field
		/// </summary>
		/// <param name="headerFile">Header file being parsed</param>
		/// <param name="outer">Outer object</param>
		/// <param name="lineNumber">Line number of declaration</param>
		protected UhtField(UhtHeaderFile headerFile, UhtType outer, int lineNumber) : base(headerFile, outer, lineNumber)
		{
		}
	}
}

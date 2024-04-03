// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool
{
#nullable enable

	/// <summary>
	/// Attribute which can be applied to a ModuleRules-dervied class to indicate which module groups it belongs to
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
	public sealed class ModuleGroupsAttribute : Attribute
	{
		/// <summary>
		/// Array of module group names
		/// </summary>
		public readonly string[] ModuleGroups;

		/// <summary>
		/// Initialize the attribute with a list of module groups
		/// </summary>
		/// <param name="ModuleGroups">Variable-length array of module group arguments</param>
		public ModuleGroupsAttribute(params string[] ModuleGroups)
		{
			this.ModuleGroups = ModuleGroups;
		}
	}
}

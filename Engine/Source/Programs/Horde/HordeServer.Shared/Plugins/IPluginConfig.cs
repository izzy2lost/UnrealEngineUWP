// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using HordeServer.Acls;

namespace HordeServer.Plugins
{
	/// <summary>
	/// Interface for plugin extensions to the global config object
	/// </summary>
	public interface IPluginConfig
	{
		/// <summary>
		/// Called to fixup a plugin's configuration after deserialization
		/// </summary>
		/// <param name="parentAcl">The parent ACL scope</param>
		void PostLoad(AclConfig parentAcl);
	}
}

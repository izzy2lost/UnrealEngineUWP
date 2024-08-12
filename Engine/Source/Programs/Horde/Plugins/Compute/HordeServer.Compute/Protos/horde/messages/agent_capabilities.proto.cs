// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents;

#pragma warning disable CS1591

namespace HordeCommon.Rpc.Messages
{
	partial class RpcAgentCapabilities
	{
		public RpcAgentCapabilities(IEnumerable<string> properties)
		{
			Properties.AddRange(properties);
		}

		public void Flatten(out List<string> properties, out Dictionary<string, int> resources)
		{
			properties = new List<string>();
			resources = new Dictionary<string, int>();

			properties.AddRange(Properties);

			if (Devices.Count <= 0)
			{
				return;
			}

			RpcDeviceCapabilities device = Devices[0];
			if (device.Properties == null)
			{
				return;
			}

			properties.AddRange(device.Properties);
			properties.Sort(StringComparer.OrdinalIgnoreCase);

			CopyPropertyToResource(KnownPropertyNames.LogicalCores, properties, resources);
			CopyPropertyToResource(KnownPropertyNames.Ram, properties, resources);
		}

		static void CopyPropertyToResource(string name, List<string> properties, Dictionary<string, int> resources)
		{
			foreach (string property in properties)
			{
				if (property.Length > name.Length && property.StartsWith(name, StringComparison.OrdinalIgnoreCase) && property[name.Length] == '=')
				{
					int value;
					if (Int32.TryParse(property.AsSpan(name.Length + 1), out value))
					{
						resources[name] = value;
					}
				}
			}
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using EpicGames.Perforce;
using EpicGames.Redis;
using HordeServer.Commits;
using HordeServer.Server;
using HordeServer.Streams;
using HordeServer.Users;
using HordeServer.Utilities;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;
using MongoDB.Driver;
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace HordeServer.Utilities
{
	/// <summary>
	/// Extensions for OpenTelemetry spans
	/// </summary>
	public static class OpenTelemetryExtensions
	{
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static TelemetrySpan SetAttribute(this TelemetrySpan span, string key, ContentHash value)
		{
			span.SetAttribute(key, value.ToString());
			return span;
		}

		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static TelemetrySpan SetAttribute(this TelemetrySpan span, string key, SubResourceId value)
		{
			span.SetAttribute(key, value.ToString());
			return span;
		}

		/// <inheritdoc cref="TelemetrySpan.SetAttribute(System.String, System.String)"/>
		public static TelemetrySpan SetAttribute(this TelemetrySpan span, string key, StreamId? value) => span.SetAttribute(key, value?.ToString());

		/// <inheritdoc cref="TelemetrySpan.SetAttribute(System.String, System.String)"/>
		public static TelemetrySpan SetAttribute(this TelemetrySpan span, string key, TemplateId? value) => span.SetAttribute(key, value?.ToString());

		/// <inheritdoc cref="TelemetrySpan.SetAttribute(System.String, System.String)"/>
		public static TelemetrySpan SetAttribute(this TelemetrySpan span, string key, TemplateId[]? values) => span.SetAttribute(key, values != null ? String.Join(',', values.Select(x => x.Id.ToString())) : null);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Text;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles.V2
{
	/// <summary>
	/// Handle to an export within a packet. Same implementation is used for flushed and pending exports.
	/// </summary>
	public class ExportHandle : IBlobHandle
	{
		static readonly Utf8String s_fragmentPrefix = new Utf8String("exp=");

		readonly PacketHandle _packet;
		readonly int _exportIdx;

		/// <inheritdoc/>
		public IBlobHandle? Outer => _packet;

		/// <summary>
		/// Constructor
		/// </summary>
		public ExportHandle(PacketHandle packet, int exportIdx)
		{
			_packet = packet;
			_exportIdx = exportIdx;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ExportHandle(PacketHandle packet, ReadOnlySpan<byte> fragment)
		{
			_packet = packet;
			if (!TryParse(fragment, out _exportIdx))
			{
				throw new FormatException($"Invalid fragment {Encoding.UTF8.GetString(fragment)} relative to {packet}");
			}
		}

		/// <inheritdoc/>
		public ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

		/// <summary>
		/// Attempt to parse an export index from the given fragment
		/// </summary>
		static bool TryParse(ReadOnlySpan<byte> fragment, out int exportIdx)
		{
			if (!fragment.StartsWith(s_fragmentPrefix))
			{
				exportIdx = 0;
				return false;
			}

			fragment = fragment.Slice(s_fragmentPrefix.Length);
			if (!Utf8Parser.TryParse(fragment, out exportIdx, out int numBytesRead) || numBytesRead != fragment.Length)
			{
				exportIdx = 0;
				return false;
			}

			return true;
		}

		/// <inheritdoc/>
		public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			=> _packet.ReadExportAsync(_exportIdx, cancellationToken);

		/// <inheritdoc/>
		public bool TryAppendIdentifier(Utf8StringBuilder builder)
		{
			AppendIdentifier(builder, _exportIdx);
			return true;
		}

		/// <summary>
		/// Appends an export identifier to the given string builder
		/// </summary>
		public static void AppendIdentifier(Utf8StringBuilder builder, int exportIdx)
		{
			builder.Append(s_fragmentPrefix);
			builder.Append(exportIdx);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj)
			=> obj is ExportHandle other && _packet.Equals(other._packet) && _exportIdx == other._exportIdx;

		/// <inheritdoc/>
		public override int GetHashCode()
			=> HashCode.Combine(_packet, _exportIdx);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Logs;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Differencing;
using Microsoft.Extensions.FileSystemGlobbing.Internal;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;

#nullable enable
#pragma warning disable MA0048

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for LLVM Thread Sanitizer reports
	/// 
	/// Note, TSAN reports emit to stderr/stdout which can cause issues for LogEventMatchers since they assume multiple lines will not be interleaved
	/// with irrelevant lines which should be skipped / contextualized by other matchers. As such this matcher doesn't try to keep the whole TSAN report as a single series of events
	/// and instead tries to annotate lines that look like report lines when possible. 
	/// 
	/// If you don't see TSAN output at all in your logs, it's likely you need -VeryVerbose to be passed to gauntlet to ensure stdout is redirected
	/// </summary>
	class ThreadSanitizerEventMatcher : ILogEventMatcher
	{
		// All TSAN reports begin with a WARNING and then SUMMARY line.
		static readonly Regex s_WarningPattern = new Regex(@"^WARNING: ThreadSanitizer:\s*(.+)$");

		// e.g. Note line number, column number and BuildId are optional
		// #1 MyType<Type, OtherType>::Function(unsigned long*&) const /mnt/somepath/file.h:90:10 (BinaryName+0x2bc102e0) (BuildId: 8b17597a9f5444a0)
		static readonly Regex s_StackTracePattern = new Regex(
			@"^\s+\#[\d]+?\s" +
			@"(?<symbol>.+?)\s" +
			@"(?<sourcefile>(\/|\w:).+?)" +
			@"(:(?<linenumber>\d+?))?" +
			@"(:(?<colnumber>\d+?))?" +
			@"\s+\(.+?\+(?<address>0x[0-9a-fA-F]+?)\)" +
			@"(\s+?\(BuildId:\s+?(?<buildid>.+?)\))?$");

		// e.g.
		// SUMMARY: ThreadSanitizer: data race /some/path.cpp:169:33 in MyType::Foo()
		static readonly Regex s_SummaryPattern = new Regex(
			@"^SUMMARY: ThreadSanitizer:\s*" +
			@"(?<error>.+)\s" +
			@"(?<sourcefile>(\/|\w:).+?)" +
			@"(:(?<linenumber>\d+?))?" +
			@"(:(?<colnumber>\d+?))?\s+?in\s+?" + 
			@"(?<symbol>.+)$");

		// When files/symbols can't resolve, addr2line and llvm-symbolizer can replace symbol names with special names like "<null>"
		// so instead of overcomplicating the above regexes to handle those conditional names, we use a catch all fallback patterns
		static readonly Regex s_StackTraceFallbackPattern = new Regex(@"^\s+\#[\d]+?\s*(.+)$");
		static readonly Regex s_SummaryFallbackPattern = new Regex(@"^SUMMARY: ThreadSanitizer:\s*(.+)$");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			Match? match;
			if (cursor.TryMatch(s_WarningPattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Warning, KnownLogEvents.Sanitizer_Thread);
			}

			if (cursor.TryMatch(s_StackTracePattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);

				do
				{
					builder.AnnotateSymbol(match!.Groups["symbol"]);
					builder.AnnotateSourceFile(match.Groups["sourcefile"], "");
					builder.Annotate(match.Groups["linenumber"], LogEventMarkup.LineNumber);
					builder.Annotate(match.Groups["colnumber"], LogEventMarkup.ColumnNumber);
					builder.Annotate(match.Groups["address"], "");
					builder.Annotate(match.Groups["buildid"], "");

					builder.MoveNext();
				} while (builder.Current.TryMatch(s_StackTracePattern, out match));

				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Information, KnownLogEvents.Sanitizer_Thread);
			}
			else if (cursor.TryMatch(s_StackTraceFallbackPattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);

				do
				{
					builder.MoveNext();
				} while (builder.Current.TryMatch(s_StackTraceFallbackPattern, out match));

				return builder.ToMatch(LogEventPriority.Low, LogLevel.Information, KnownLogEvents.Sanitizer_Thread);
			}

			if (cursor.TryMatch(s_SummaryPattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.Annotate(match.Groups["error"], LogEventMarkup.ErrorCode);
				builder.AnnotateSourceFile(match.Groups["sourcefile"], "");
				builder.Annotate(match.Groups["linenumber"], LogEventMarkup.LineNumber);
				builder.Annotate(match.Groups["colnumber"], LogEventMarkup.ColumnNumber);
				builder.AnnotateSymbol(match.Groups["symbol"]);

				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.Sanitizer_Thread);
			}
			else if (cursor.TryMatch(s_SummaryFallbackPattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);

				return builder.ToMatch(LogEventPriority.Low, LogLevel.Error, KnownLogEvents.Sanitizer_Thread);
			}

			return null;
		}
	}
}

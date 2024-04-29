// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Issues.Handlers;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for generated Thread Sanitizer reports
	/// 
	/// </summary>
	class ThreadSanitizerEventMatcher : ILogEventMatcher
	{
		// All TSAN reports start and end with this delimiter.
		static readonly Regex s_StartEndDelimiter = new Regex(@"^==================$");

		// e.g. Note line number, column number and BuildId are optional
		// #1 MyType<Type, OtherType>::Function(unsigned long*&) const /mnt/somepath/file.h:90:10 (BinaryName+0x2bc102e0) (BuildId: 8b17597a9f5444a0)
		static readonly Regex s_StackTracePattern = new Regex(
			@"^\s+\#[\d]+?\s" +
			@"(?<Symbol>.+?)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?" +
			@"\s+\(.+?\+(?<Address>0x[0-9a-fA-F]+?)\)" +
			@"(\s+?\(BuildId:\s+?(?<BuildId>.+?)\))?$");

		// e.g.
		// SUMMARY: ThreadSanitizer: data race /some/path.cpp:169:33 in MyType::Foo()
		static readonly Regex s_SummaryPattern = new Regex(
			@"^SUMMARY: ThreadSanitizer:\s*" +
			@"(?<SummaryReason>.+)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?\s+?in\s+?" + 
			@"(?<Symbol>.+)$");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_StartEndDelimiter))
			{
				Match? match;
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.MoveNext();

				while (builder.Current.CurrentLine != null && !builder.Current.IsMatch(s_StartEndDelimiter))
				{
					if (builder.Current.TryMatch(s_StackTracePattern, out match))
					{
						do
						{
							builder.AnnotateSymbol(match!.Groups["Symbol"]);
							builder.AnnotateSourceFile(match.Groups["SourceFile"], "");
							builder.Annotate(match.Groups["Line"], LogEventMarkup.LineNumber);
							builder.Annotate(match.Groups["Column"], LogEventMarkup.ColumnNumber);
							builder.Annotate(match.Groups["Address"], "");
							builder.Annotate(match.Groups["BuildId"], "");

							builder.MoveNext();
						} while (builder.Current.TryMatch(s_StackTracePattern, out match));
					}

					if (builder.Current.TryMatch(s_SummaryPattern, out match))
					{
						// Used by IssueHandler
						builder.Annotate(match.Groups["SummaryReason"], ThreadSanitizerIssueHandler.SummaryReason);
						builder.Annotate("SummarySourceFile", match.Groups["SourceFile"], ThreadSanitizerIssueHandler.SummarySourceFile);

						// Annotate the source file again so that we may get UGS link resolution
						builder.AnnotateSourceFile(match.Groups["SourceFile"], "");
						builder.Annotate(match.Groups["Line"], LogEventMarkup.LineNumber);
						builder.Annotate(match.Groups["Column"], LogEventMarkup.ColumnNumber);
						builder.AnnotateSymbol(match.Groups["Symbol"]);

						builder.MoveNext();
						return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.Sanitizer_Thread);
					}

					if (builder.Current.CurrentLine != null)
					{
						builder.MoveNext();
					}
				}
			}

			return null;
		}
	}
}

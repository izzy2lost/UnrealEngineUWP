// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Issues.Handlers;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for LLVM Sanitizer reports
	/// 
	/// </summary>
	class SanitizerEventMatcher : ILogEventMatcher
	{
		// e.g.
		// ASAN: =================================================================
		// TSAN: ==================
		static readonly Regex s_ReportStartPattern = new Regex(@"(^=================================================================$)|(^==================$)");

		// e.g.
		// ASAN: ==30562==ABORTING
		// TSAN: ==================
		static readonly Regex s_ReportEndPattern = new Regex(@"(^==\d+==(\s*\w+)?$)|(^==================$)");

		// e.g.
		// ASAN: ==30562==ERROR: AddressSanitizer: heap-use-after-free on address 0x617002aa8418 at pc 0x7f98a08bd090 bp 0x7ffc30203af0 sp 0x7ffc30203ae8
		// TSAN: WARNING: ThreadSanitizer: data race (pid=6049)
		static readonly Regex s_ReportLevelPattern = new Regex(@"^(==\d+==\s*)?(?<ReportLevel>WARNING|ERROR):\s+(?<SanitizerName>\w+)Sanitizer:\s*(.+)$");

		// e.g. Note line number, column number and BuildId are optional
		// ASAN: #0 0x7f98a08bd08f in UObjectBase::GetFName() const /mnt/somepath/file.h:90:10
		// TSAN: #0 MyType<Type, OtherType>::Function(unsigned long*&) const /mnt/somepath/file.h:90:10 (BinaryName+0x2bc102e0) (BuildId: 8b17597a9f5444a0)
		static readonly Regex s_StackTracePattern = new Regex(
			@"^\s+\#[\d]+\s+(0x[0-9a-fA-F]+\s+in\s+)?" + // The optional capture group is to handle ASAN's slightly different stack trace syntax from TSAN
			@"(?<Symbol>.+?)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?" +
			@"(\s+\(.+?\+(?<Address>0x[0-9a-fA-F]+?)\))?" +
			@"(\s+?\(BuildId:\s+?(?<BuildId>.+?)\))?$");

		// e.g.
		// ASAN: SUMMARY: AddressSanitizer: heap-use-after-free /some/path.cpp:169:33 in UObjectBase::GetFName() const
		// TSAN: SUMMARY: ThreadSanitizer: data race /some/path.cpp:169:33 in MyType::Foo()
		static readonly Regex s_SummaryPattern = new Regex(
			@"^SUMMARY:\s+(?<SanitizerName>\w+)Sanitizer:\s*" +
			@"(?<SummaryReason>.+)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?\s+?in\s+?" +
			@"(?<Symbol>.+)$");

		EventId ConvertSanitizerNameToEventId(string sanitizerName)
		{
			switch (sanitizerName)
			{
				case "Thread": return KnownLogEvents.Sanitizer_Thread;
				case "Address": return KnownLogEvents.Sanitizer_Address;
			}

			return KnownLogEvents.Sanitizer;
		}

		LogLevel ConvertReportLevel(string reportLevel)
		{
			switch (reportLevel)
			{
				case "WARNING": return LogLevel.Warning;
				case "ERROR": return LogLevel.Error;
			}
			return LogLevel.Error;
		}

		public LogEventMatch? Match(ILogCursor cursor)
		{
			Match? match;
			if (cursor.TryMatch(s_ReportStartPattern, out match))
			{
				EventId sanitizerId = KnownLogEvents.Sanitizer;
				LogLevel reportLevel = LogLevel.Information;

				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.MoveNext();

				while (builder.Current.CurrentLine != null)
				{

					if (builder.Current.TryMatch(s_StackTracePattern, out match))
					{
						do
						{
							builder.AnnotateSymbol(match!.Groups["Symbol"]);
							builder.AnnotateSourceFile(match.Groups["SourceFile"], "");
							builder.TryAnnotate(match.Groups["Line"], LogEventMarkup.LineNumber);
							builder.TryAnnotate(match.Groups["Column"], LogEventMarkup.ColumnNumber);
							builder.TryAnnotate(match.Groups["Address"], "");
							builder.TryAnnotate(match.Groups["BuildId"], "");

							builder.MoveNext();
						} while (builder.Current.TryMatch(s_StackTracePattern, out match));
					}
					else
					{
						if (builder.Current.TryMatch(s_ReportLevelPattern, out match))
						{
							sanitizerId = ConvertSanitizerNameToEventId(match.Groups["SanitizerName"].Value);
							reportLevel = ConvertReportLevel(match.Groups["ReportLevel"].Value);

							// Used by IssueHandler
							builder.Annotate(match.Groups["SanitizerName"], SanitizerIssueHandler.SanitizerName);
						}

						if (builder.Current.TryMatch(s_SummaryPattern, out match))
						{
							// Used by IssueHandler
							builder.Annotate(match.Groups["SummaryReason"], SanitizerIssueHandler.SummaryReason);
							builder.Annotate("SummarySourceFile", match.Groups["SourceFile"], SanitizerIssueHandler.SummarySourceFile);

							// Annotate the source file again so that we may get UGS link resolution
							builder.AnnotateSourceFile(match.Groups["SourceFile"], "");
							builder.TryAnnotate(match.Groups["Line"], LogEventMarkup.LineNumber);
							builder.TryAnnotate(match.Groups["Column"], LogEventMarkup.ColumnNumber);
							builder.AnnotateSymbol(match.Groups["Symbol"]);
						}
						else if (builder.Current.IsMatch(s_ReportEndPattern))
						{
							return builder.ToMatch(LogEventPriority.AboveNormal, reportLevel, sanitizerId);
						}

						builder.MoveNext();
					}
				}
			}

			return null;
		}
	}
}

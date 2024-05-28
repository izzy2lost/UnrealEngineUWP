// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Issues.Handlers;
using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.Text.RegularExpressions;

#nullable enable
#pragma warning disable MA0016

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for LLVM Sanitizer reports
	/// 
	/// </summary>
	public class SanitizerEventMatcher : ILogEventMatcher
	{
		/// <summary>
		/// Regex pattern that matches Report Start
		/// </summary>
		// e.g.
		// ASAN: =================================================================
		// TSAN: ==================
		public static readonly Regex ReportStartPattern = new Regex(@"(^=================================================================$)|(^==================$)", RegexOptions.Multiline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex pattern that matches Report End
		/// </summary>
		// e.g.
		// ASAN: ==30562==ABORTING
		// TSAN: ==================
		public static readonly Regex ReportEndPattern = new Regex(@"(^==\d+==(\s*\w+)?$)|(^==================$)", RegexOptions.Multiline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex pattern that matches Report Level
		/// </summary>
		// e.g.
		// ASAN: ==30562==ERROR: AddressSanitizer: heap-use-after-free on address 0x617002aa8418 at pc 0x7f98a08bd090 bp 0x7ffc30203af0 sp 0x7ffc30203ae8
		// TSAN: WARNING: ThreadSanitizer: data race (pid=6049)
		public static readonly Regex ReportLevelPattern = new Regex(@"^[\s\t]*(==\d+==\s*)?(?<ReportLevel>WARNING|ERROR):\s+(?<SanitizerName>\w+)Sanitizer:\s*(?<Summary>.+)$", RegexOptions.Multiline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex pattern that matches Stack Trace
		/// </summary>
		// e.g. Note line number, column number and BuildId are optional
		// ASAN: #0 0x7f98a08bd08f in UObjectBase::GetFName() const /mnt/somepath/file.h:90:10
		// TSAN: #0 MyType<Type, OtherType>::Function(unsigned long*&) const /mnt/somepath/file.h:90:10 (BinaryName+0x2bc102e0) (BuildId: 8b17597a9f5444a0)
		public static readonly Regex StackTracePattern = new Regex(
			@"^[\s\t]+\#[\d]+\s+(0x[0-9a-fA-F]+\s+in\s+)?" + // The optional capture group is to handle ASAN's slightly different stack trace syntax from TSAN
			@"(?<Symbol>.+?)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?" +
			@"(\s+\(.+?\+(?<Address>0x[0-9a-fA-F]+?)\))?" +
			@"(\s+?\(BuildId:\s+?(?<BuildId>.+?)\))?$", RegexOptions.Multiline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex pattern that matches Summary
		/// </summary>
		// e.g.
		// ASAN: SUMMARY: AddressSanitizer: heap-use-after-free /some/path.cpp:169:33 in UObjectBase::GetFName() const
		// TSAN: SUMMARY: ThreadSanitizer: data race /some/path.cpp:169:33 in MyType::Foo()
		public static readonly Regex SummaryPattern = new Regex(
			@"^[\s\t]*SUMMARY:\s+(?<SanitizerName>\w+)Sanitizer:\s*" +
			@"(?<SummaryReason>.+)\s" +
			@"(?<SourceFile>(\/|\w:).+?)" +
			@"(:(?<Line>\d+?))?" +
			@"(:(?<Column>\d+?))?\s+?in\s+?" +
			@"(?<Symbol>.+)$", RegexOptions.Multiline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Convert SanitizerName to EventId
		/// </summary>
		/// <param name="sanitizerName"></param>
		/// <returns></returns>
		public static EventId ConvertSanitizerNameToEventId(string sanitizerName)
		{
			switch (sanitizerName)
			{
				case "Thread": return KnownLogEvents.Sanitizer_Thread;
				case "Address": return KnownLogEvents.Sanitizer_Address;
			}

			return KnownLogEvents.Sanitizer;
		}

		/// <summary>
		/// Convert ReportLevel string to LogLevel enum
		/// </summary>
		/// <param name="reportLevel"></param>
		/// <returns></returns>
		public static LogLevel ConvertReportLevel(string reportLevel)
		{
			switch (reportLevel)
			{
				case "WARNING": return LogLevel.Warning;
				case "ERROR": return LogLevel.Error;
			}
			return LogLevel.Error;
		}

		private static void AddProperty(LogValue Value, Group Group, Dictionary<string, object> Properties)
		{
			Value.Text = Group.Value;
			Properties.Add(Value.Type.ToString(), Value);
		}

		/// <summary>
		/// Add Sanitizer Summary information to input Key/Value Pair Properties
		/// </summary>
		/// <param name="Message"></param>
		/// <param name="Properties"></param>
		/// <returns></returns>
		public static bool AddSanitizerSummaryProperties(string Message, Dictionary<string, object> Properties)
		{
			Match Match = SummaryPattern.Match(Message);
			if (Match.Success)
			{
				AddProperty(SanitizerIssueHandler.SanitizerName, Match.Groups["SanitizerName"], Properties);
				AddProperty(SanitizerIssueHandler.SummaryReason, Match.Groups["SummaryReason"], Properties);
				AddProperty(SanitizerIssueHandler.SummarySourceFile, Match.Groups["SourceFile"], Properties);

				return true;
			}

			return false;
		}

		/// <inheritdoc/> 
		public LogEventMatch? Match(ILogCursor cursor)
		{
			Match? match;
			if (cursor.TryMatch(ReportStartPattern, out match))
			{
				EventId sanitizerId = KnownLogEvents.Sanitizer;
				LogLevel reportLevel = LogLevel.Information;

				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.MoveNext();

				while (builder.Current.CurrentLine != null)
				{

					if (builder.Current.TryMatch(StackTracePattern, out match))
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
						} while (builder.Current.TryMatch(StackTracePattern, out match));
					}
					else
					{
						if (builder.Current.TryMatch(ReportLevelPattern, out match))
						{
							sanitizerId = ConvertSanitizerNameToEventId(match.Groups["SanitizerName"].Value);
							reportLevel = ConvertReportLevel(match.Groups["ReportLevel"].Value);

							// Used by IssueHandler
							builder.Annotate(match.Groups["SanitizerName"], SanitizerIssueHandler.SanitizerName);
						}

						if (builder.Current.TryMatch(SummaryPattern, out match))
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
						else if (builder.Current.IsMatch(ReportEndPattern))
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

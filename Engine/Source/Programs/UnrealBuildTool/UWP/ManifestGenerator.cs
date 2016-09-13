// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Schema;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	/// <summary>
	///  Class to handle generating an AppxManifest.xml file
	/// </summary>
	// @ATG_CHANGE : BEGIN rename to match generic nature, but undid the file move\rename for merge convenience
	public class PackageManifestGenerator
	// @ATG_CHANGE : END
	{
		// @ATG_CHANGE : BEGIN UWP Packaging support
		public PackageManifestGenerator(string RelativeExePathParam, string ProjectPathParam, FileReference ProjectFile, UnrealTargetPlatform InPlatform, IEnumerable<string> InAdditionalNamespacePrefixes, IEnumerable<WinMDRegistrationInfo> InWinMDReferences) 
		{
			Platform = InPlatform;

			// Store values for use by the settings interpretation function
			ReletiveExePath = RelativeExePathParam;
			ProjectPath = ProjectPathParam;

			CreateAppxSchema();

			// Load up INI settings. We'll use engine settings to retrieve the manifest configuration, but these may reference
			// values in either game or engine settings, so we'll keep both.
			DirectoryReference DirRef = DirectoryReference.FromFile(ProjectFile);
			if (DirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				DirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath());
			}

			GameIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Game", DirRef);
			EngineIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", DirRef);

			// For additional namespaces, the prefixes are provided by the platform (maintains compatibility with original Xbox One manifest ini data)
			// but the associated uris come from the ini files themselves.
			AdditionalNamespaces = new Dictionary<string, string>();
			foreach (string NamespacePrefix in InAdditionalNamespacePrefixes)
			{
				string XmlNSSource;
				if (EngineIni.GetString("AppxManifest", string.Format("Package.xmlns:{0}", NamespacePrefix), out XmlNSSource))
				{
					AdditionalNamespaces.Add(XmlNSSource, NamespacePrefix);
				}
			}
			// @ATG_CHANGE : BEGIN winmd type registration support
			WinMDReferences = new List<WinMDRegistrationInfo>(InWinMDReferences);
			// @ATG_CHANGE : END		

		}
		// @ATG_CHANGE : END

		// @ATG_CHANGE : BEGIN winmd type registration support
		private List<WinMDRegistrationInfo> WinMDReferences;
		// @ATG_CHANGE : END

		// @ATG_CHANGE : BEGIN UWP Packaging support
		private UnrealTargetPlatform Platform;
		private Dictionary<string, string> AdditionalNamespaces;
		private ConfigCacheIni EngineIni;
		private ConfigCacheIni GameIni;
		private XmlSchemaSet AppxSchema;

		/// <summary>
		/// Exe path relative to image root
		/// </summary>
		private string ReletiveExePath;

		/// <summary>
		/// Path to project root (real project path if we have it, Engine path if we're building content only and don't know for what)
		/// </summary>
		private string ProjectPath;
		// @ATG_CHANGE : END

		/// <summary>
		/// Helper function that inserts a tab character for each indent level.
		/// </summary>
		private static string GetIndentString(int Indent)
		{
			string Line = null;
			for (; Indent > 0; Indent--)
			{
				Line += "\t";
			}

			return Line;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Search the root of the schema for any substitution groups that can override the TargetElement and insert them into
		/// OutputContents via the PrintElement function.
		/// </summary>
		/// <param name="TargetElement"> Element to search for substitutions of</param>
		/// <param name="SettingID">  The INI key value for the current element (full tree)</param>
		/// <param name="Indent">         Indent level of current element</param>
		/// <param name="OutputContents"> StringBuilder to contain text output of operation</param>
		/// <returns>bool    true if any elements were successfully written</returns>
		private bool FixRefForSubstitution(XmlSchemaElement TargetElement, string SettingID, int Indent, StringBuilder OutputContents)
		// @ATG_CHANGE : END
		{
			bool SubstitutionFound = false;

			foreach (XmlSchemaElement Element in AppxSchema.GlobalElements.Values)
			{
				if (Element.SubstitutionGroup != null && TargetElement.QualifiedName.Name.Equals(Element.SubstitutionGroup.Name))
				{
					SubstitutionFound |= PrintElement(Element, SettingID, Indent, OutputContents, TargetElement.MaxOccurs);
					// Note: substitution groups are often used in place of choice elements, there could be multiple substitution
					// options per TargetElement, so do not stop when one is found.
				}
			}

			return SubstitutionFound;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Retrieve a setting value from INIs. Settings are stored in AppxManifest section of Engine INIs. The setting key
		/// is the dot delimited XML tree to the element value. Array specifiers allow definition of multiple entries of the
		/// same element. Interpret any operators and fill values into the final result.
		/// Sample INI setting: Package.Capabilities.mx:Capability[0].Name=kinectAudio
		/// Corresponding XML output:
		/// <Package>
		/// <Capabilities>
		/// <mx:Capability Name="kinectAudio">
		/// Symbol Key:
		/// $Section:Key$       Look up Key in Section in INI files (Game first then fall back to Engine).
		/// Replace symbol with INI setting value.
		/// %RelativeExePath%   Replace value with path to exe from package root.
		/// %Insert:Path%       Insert contents of file at Path based off of project root path. Will indent all lines in the file
		/// by the current value of Indent. File should contain valid XML (this is not verified).
		/// </summary>
		/// <param name="LookupString"> INI key to locate</param>
		/// <param name="">Index               Optional index of setting for [n] type LookupStrings</param>
		/// <param name="Indent">      Optional current indent count (used when writing %Insert:Path% values</param>
		/// <returns>string    INI value for key LookupString post interpretation</returns>
		private string GetInterprettedSettingValue(string LookupString, int Index = 0, int Indent = 0)
		// @ATG_CHANGE : END
		{
			char[] VariableMarkers = { '$', '%' };
			string BaseSetting;
			string InterprettedSetting = "";

			// Manifest settings are only (validly) located in Engine INI files
			EngineIni.GetString("AppxManifest", LookupString, out BaseSetting);

			// Parse results for any operators
			int NextSetting = BaseSetting.IndexOfAny(VariableMarkers);
			while (NextSetting >= 0)
			{
				// This will parse multiple operator types within a single setting, but not nested operators
				if (NextSetting > 0)
				{
					// Copy any leading text (non-operator) to our output
					// @ATG_CHANGE : BEGIN UWP Packaging support
					InterprettedSetting += BaseSetting.Substring(0, NextSetting);
					// @ATG_CHANGE : END
				}
				int LenOfSetting = BaseSetting.Substring(NextSetting + 1).IndexOfAny(VariableMarkers);
				if (LenOfSetting < 0)
				{
					Log.TraceError("Could not parse setting {0}. Unmatched variable symbol '{1}'", LookupString, BaseSetting[NextSetting]);
					return InterprettedSetting + BaseSetting;
				}
				if (BaseSetting[NextSetting] != BaseSetting[NextSetting + LenOfSetting + 1])
				{
					// Probable nested operators
					Log.TraceError("Could not parse setting {0}. Mismatched variable symbols '{1}' and '{2}'", LookupString, BaseSetting[NextSetting], BaseSetting[NextSetting + LenOfSetting + 1]);
					return InterprettedSetting + BaseSetting;
				}

				// Complete contents of operator
				string VariableName = BaseSetting.Substring(NextSetting + 1, LenOfSetting);

				switch (BaseSetting[NextSetting])
				{
					case '$':
						// Look up $Section:Key$ in Game INIs
						string IniSection = VariableName.Substring(0, VariableName.IndexOf(':'));
						string IniSetting = VariableName.Substring(VariableName.IndexOf(':') + 1);
						string IniValue;
						GameIni.GetString(IniSection, IniSetting, out IniValue);
						// If not found in Game INIs, search for the same Key in Engine INIs
						if (IniValue.Length == 0)
						{
							EngineIni.GetString(IniSection, IniSetting, out IniValue);
						}
						// Replace operator with value recovered
						if (IniValue.Length == 0)
						{
							// @todo: Is there any better way to handle not finding the value? If we leave a value blank it will
							// likely produce invalid XML and be difficult to trace. At least this hardcoded string should lead
							// users back here.
							InterprettedSetting += "InvalidIniValue";
						}
						else
						{
							InterprettedSetting += IniValue;
						}
						break;
					case '%':
						if (VariableName.Equals("RelativeExePath"))
						{
							InterprettedSetting += ReletiveExePath;
						}
						// @ATG_CHANGE : BEGIN UWP Packaging support
						else if (VariableName.Equals("ExeFlavor"))
						{
							InterprettedSetting += ExeFlavor;
						}
						// @ATG_CHANGE : END
						else if (VariableName.StartsWith("Insert:"))
						{
							// Attempt to open path provided based off of the current project path
							string InsertSource = Path.Combine(ProjectPath, VariableName.Substring(VariableName.IndexOf(':') + 1));
							if (!File.Exists(InsertSource))
							{
								Log.TraceWarning("Invalid path for insertion: {0}", InsertSource);
								// @todo: Can't think of a way to insert valid XML in this case, so it's just going to be left out.
								// It would be better at least to insert something that would lead back here (as is done with
								// "InvalidIniValue" above.
								break;
							}
							string[] InsertContents = null;
							try
							{
								InsertContents = File.ReadAllLines(InsertSource);
							}
							catch (Exception)
							{
								Log.TraceWarning("Error while trying to read data for insert from {0}.", InsertSource);
								// @todo: Can't think of a way to insert valid XML in this case, so it's just going to be left out.
								// It would be better at least to insert something that would lead back here (as is done with
								// "InvalidIniValue" above.
								break;
							}
							// Insert file contents one line at a time so that we can add indentation as needed.
							foreach (string InsertLine in InsertContents)
							{
								InterprettedSetting += GetIndentString(Indent) + InsertLine + "\n";
							}
						}
						else if (VariableName.StartsWith("ResourceString:"))
						{
							string SectionKeyPair = VariableName.Substring(VariableName.IndexOf(':') + 1);
							// Look up $Section:Key$ in Game INIs
							string SettingKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
							// Replace operator with value recovered
							InterprettedSetting += "ms-resource:" + SettingKey;
						}
						else if (VariableName.StartsWith("ResourceBinary:"))
						{
							string SectionKeyPair = VariableName.Substring(VariableName.IndexOf(':') + 1);
							// Look up $Section:Key$ in Game INIs
							string SettingSection = SectionKeyPair.Substring(0, SectionKeyPair.IndexOf(':'));
							string SettingKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
							String SettingValue = null;
							GameIni.GetString(SettingSection, SettingKey, out SettingValue);
							// If not found in Game INIs, search for the same Key in Engine INIs
							if (SettingValue == null || SettingValue.Length == 0)
							{
								EngineIni.GetString(SettingSection, SettingKey, out SettingValue);
							}
							// Replace operator with value recovered
							if (SettingValue != null && SettingValue.Length > 0)
							{
								InterprettedSetting += "Resources\\" + SettingKey + ".png";
							}
						}
						else if (VariableName.StartsWith("Array:"))
						{
							string SectionKeyPair = VariableName.Substring(VariableName.IndexOf(':') + 1);
							// Look up $Section:Key$ in Game INIs
							string ArraySection = SectionKeyPair.Substring(0, SectionKeyPair.IndexOf(':'));
							string ArrayKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
							List<string> ArraySettingValue = null;
							GameIni.GetArray(ArraySection, ArrayKey, out ArraySettingValue);
							// If not found in Game INIs, search for the same Key in Engine INIs
							if (ArraySettingValue == null || ArraySettingValue.Count == 0)
							{
								EngineIni.GetArray(ArraySection, ArrayKey, out ArraySettingValue);
							}
							// Replace operator with value recovered
							if (ArraySettingValue == null || ArraySettingValue.Count == 0)
							{
								// @todo: Is there any better way to handle not finding the value? If we leave a value blank it will
								// likely produce invalid XML and be difficult to trace. At least this hardcoded string should lead
								// users back here.
								InterprettedSetting += "InvalidIniValue";
							}
							else if (ArraySettingValue.Count > Index)
							{
								InterprettedSetting += ArraySettingValue[Index];
							}
						}
						else if (VariableName.StartsWith("AlphaNumericDot:"))
						{
							string SectionKeyPair = VariableName.Substring(VariableName.IndexOf(':') + 1);
							// Look up $Section:Key$ in Game INIs
							string SettingSection = SectionKeyPair.Substring(0, SectionKeyPair.IndexOf(':'));
							string SettingKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
							string SettingValue = null;
							GameIni.GetString(SettingSection, SettingKey, out SettingValue);
							// If not found in Game INIs, search for the same Key in Engine INIs
							if (SettingValue == null || SettingValue.Length == 0)
							{
								EngineIni.GetString(SettingSection, SettingKey, out SettingValue);
							}
							// Replace operator with value recovered
							if (SettingValue == null || SettingValue.Length == 0)
							{
								// @todo: Is there any better way to handle not finding the value? If we leave a value blank it will
								// likely produce invalid XML and be difficult to trace. At least this hardcoded string should lead
								// users back here.
								InterprettedSetting += "InvalidIniValue";
							}
							else
							{
								foreach (char Character in SettingValue.ToCharArray())
								{
									if ((Character >= 'A' && Character <= 'Z') ||
									   (Character >= 'a' && Character <= 'z') ||
									   (Character >= '0' && Character <= '9') ||
									   (Character == '.'))
									{
										InterprettedSetting += Character;
									}
								}
							}
						}
						else if (VariableName.StartsWith("DefaultValue:"))
						{
							int SectionIndex = VariableName.IndexOf(':') + 1;
							int KeyIndex = VariableName.IndexOf(':', SectionIndex) + 1;
							int ValueTypeIndex = VariableName.IndexOf(':', KeyIndex) + 1;
							int DefaultValueIndex = VariableName.IndexOf(':', ValueTypeIndex) + 1;
							string DefaultValue = VariableName.Substring(DefaultValueIndex);
							string ValueType = VariableName.Substring(ValueTypeIndex, DefaultValueIndex - ValueTypeIndex - 1);
							string SettingSection = VariableName.Substring(SectionIndex, KeyIndex - SectionIndex - 1);
							string SettingKey = VariableName.Substring(KeyIndex, ValueTypeIndex - KeyIndex - 1);
							// Look up $Section:Key$ in Game INIs
							string SettingValue = null;
							if (ValueType.Equals("Int32", StringComparison.InvariantCultureIgnoreCase))
							{
								int Int32SettingValue;
								GameIni.GetInt32(SettingSection, SettingKey, out Int32SettingValue);
								SettingValue = Int32SettingValue.ToString();
							}
							else if (ValueType.Equals("GUID", StringComparison.InvariantCultureIgnoreCase))
							{
								Guid GuidSettingValue;
								GameIni.GetGUID(SettingSection, SettingKey, out GuidSettingValue);
								SettingValue = GuidSettingValue.ToString("N");
							}
							else
							{
								GameIni.GetString(SettingSection, SettingKey, out SettingValue);
							}
							// If not found in Game INIs, search for the same Key in Engine INIs
							if (SettingValue == null || SettingValue.Length == 0)
							{
								if (ValueType.Equals("Int32", StringComparison.InvariantCultureIgnoreCase))
								{
									int Int32SettingValue;
									EngineIni.GetInt32(SettingSection, SettingKey, out Int32SettingValue);
									SettingValue = Int32SettingValue.ToString();
								}
								else if (ValueType.Equals("GUID", StringComparison.InvariantCultureIgnoreCase))
								{
									Guid GuidSettingValue;
									EngineIni.GetGUID(SettingSection, SettingKey, out GuidSettingValue);
									SettingValue = GuidSettingValue.ToString("N");
								}
								else
								{
									EngineIni.GetString(SettingSection, SettingKey, out SettingValue);
								}
							}
							// Replace operator with value recovered
							if (SettingValue == null || SettingValue.Length == 0)
							{
								InterprettedSetting += DefaultValue;
							}
							else
							{
								InterprettedSetting += SettingValue;
							}
						}
						else
						{
							Log.TraceWarning("Unable to parse AppxManifest variable value for {0}.", VariableName);
							// @todo: Is there any better way to handle not finding the value? If we leave a value blank it will
							// likely produce invalid XML and be difficult to trace. At least this hardcoded string should lead
							// users back here.
							InterprettedSetting += "InvalidVariableValue";
						}
						break;
				}

				// Find next operator pair (if any)
				BaseSetting = BaseSetting.Substring(NextSetting + LenOfSetting + 2);
				NextSetting = BaseSetting.IndexOfAny(VariableMarkers);
			}

			// Insert any tail (non-operator) text from the original setting to our output
			InterprettedSetting += BaseSetting;

			return InterprettedSetting;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Print all attributes with setting data for a single XML element to OutputContents.
		/// </summary>
		/// <param name="Element">         XML element to output attributes of</param>
		/// <param name="SettingID">  The INI key for the element (base for attribute settings keys)</param>
		/// <param name="Index">      The index of an array stored parent element [n]</param>
		/// <param name="OutputContents"> Attribute string output</param>
		/// <returns>bool    true if any valid attributes were written and no invalid or missing required attributes were present</returns>
		private bool PrintAttributes(XmlSchemaElement Element, string SettingID, int Index, StringBuilder OutputContents)
		// @ATG_CHANGE : END
		{
			bool AnyAttributesSet = false;
			// @ATG_CHANGE : BEGIN UWP Packaging support
			List<string> MissingOrInvalidAttributes = new List<string>();
			// @ATG_CHANGE : END

			// Only complex types can contain attributes
			if (Element.ElementSchemaType is XmlSchemaComplexType)
			{
				XmlSchemaComplexType ComplexType = Element.ElementSchemaType as XmlSchemaComplexType;

				// Cycle through each attribute in the schema
				foreach (XmlSchemaAttribute Attribute in ComplexType.AttributeUses.Values)
				{
					// Get the INI value that would correspond with this attribute
					string AttributeLookup = SettingID + "." + Attribute.QualifiedName.Name;
					string AttributeSetting = GetInterprettedSettingValue(AttributeLookup, Index);
					if (AttributeSetting.Length > 0)
					{
						// True since if it there are no restrictions we're valid by default
						bool AttributeValid = true;
						// Check for any restrictions
						if (Attribute.AttributeSchemaType.Content is XmlSchemaSimpleTypeRestriction)
						{
							XmlSchemaSimpleTypeRestriction Restriction = Attribute.AttributeSchemaType.Content as XmlSchemaSimpleTypeRestriction;
							foreach (XmlSchemaFacet Facet in Restriction.Facets)
							{
								if (Facet is XmlSchemaEnumerationFacet)
								{
									XmlSchemaEnumerationFacet EnumerationValue = Facet as XmlSchemaEnumerationFacet;
									if (EnumerationValue.Value.Equals(AttributeSetting))
									{
										// Once we match an enumeration facet, we're valid and don't need to search any farther
										AttributeValid = true;
										break;
									}
									// There was an unmatching enumeration facet, we now know there is an enumeration that we must
									// match but this entry isn't the match. Attribute is now invalid until we find an enumeration
									// match.
									AttributeValid = false;
								}
								// @todo: handle other restriction facets
							}
						}
						if (AttributeValid)
						{
							AnyAttributesSet = true;
							OutputContents.Append(" " + Attribute.QualifiedName.Name + "=\"" + AttributeSetting + "\"");
							continue;
						}
						// Invalid attributes fall through to check if they were required
					}

					// Attribute value does not exist in INIs or was invalid, but was it required to be there?
					if (!Attribute.Use.ToString().Equals("optional", StringComparison.InvariantCultureIgnoreCase))
					{
						// @ATG_CHANGE : BEGIN UWP Packaging support
						MissingOrInvalidAttributes.Add(Attribute.Name);
						// @ATG_CHANGE : END
					}
				}
			}

			// @ATG_CHANGE : BEGIN UWP Packaging support
			if (AnyAttributesSet && MissingOrInvalidAttributes.Count > 0)
			{
				foreach (var BadAttribute in MissingOrInvalidAttributes)
				{
					Log.TraceError("Empty or invalid value provided for required attribute {0} on partially specified element {1}", BadAttribute, SettingID);
				}
				return false;
			}
			// @ATG_CHANGE : END

			return AnyAttributesSet;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Process an XML particle, following the tree structure down and forwarding to other functions as needed to print out elements.
		/// </summary>
		/// <param name="Particle">     XML particle to parse</param>
		/// <param name="SettingID">  The INI key for the most recent parent element (base for any child settings keys)</param>
		/// <param name="Indent">      Indent count for any child elements or output</param>
		/// <param name="OutputContents"> Output for entire tree down from this particle</param>
		/// <param name="">MaxOccurs           Optional value of parent MaxOccurs to allow cases where an intermediate particle governs</param>
		/// the MaxOccurs value of child elements.
		/// <returns>bool    true if any valid output was written from this particle and any descendants</returns>
		private bool ProcessSchemaTreeElement(XmlSchemaParticle Particle, string SettingID, int Indent, StringBuilder OutputContents, decimal MaxOccurs = -1)
		// @ATG_CHANGE : END
		{
			bool AnyChildrenDefined = false;

			if (Particle is XmlSchemaGroupBase)
			{
				// For group types, iterate through every sub particle and recursively process
				XmlSchemaGroupBase ElementGroup = Particle as XmlSchemaGroupBase;
				foreach (XmlSchemaParticle SubParticle in ElementGroup.Items)
				{
					// Make sure to pass on MaxOccurs if needed. Everything else stays the same for particles.
					AnyChildrenDefined |= ProcessSchemaTreeElement(SubParticle, SettingID, Indent, OutputContents, (MaxOccurs < Particle.MaxOccurs) ? Particle.MaxOccurs : MaxOccurs);
				}
			}
			else if (Particle is XmlSchemaElement)
			{
				// Elements will be processed into tags, attempt to print them and any descendants.
				XmlSchemaElement Element = Particle as XmlSchemaElement;
				AnyChildrenDefined = PrintElement(Element, SettingID, Indent, OutputContents, (MaxOccurs < Particle.MaxOccurs) ? Particle.MaxOccurs : MaxOccurs);
			}
			else if (Particle is XmlSchemaAny)
			{
				// Any particles are a special case and may require array processing.
				XmlSchemaAny Any = Particle as XmlSchemaAny;
				AnyChildrenDefined = PrintAny(Any, SettingID, Indent, OutputContents, (MaxOccurs < Particle.MaxOccurs) ? Particle.MaxOccurs : MaxOccurs);
			}

			return AnyChildrenDefined;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Process an XML any particle, printing any related settings.
		/// </summary>
		/// <param name="Any">             XML any particle to parse</param>
		/// <param name="SettingID">  The INI key for the any particle (minus any array indexing)</param>
		/// <param name="Indent">      Indent count for any element</param>
		/// <param name="OutputContents"> Output for settings based on the any particle</param>
		/// <param name="">MaxOccurs           Optional value of parent MaxOccurs to allow cases where an intermediate particle governs</param>
		/// the MaxOccurs value of child elements.
		/// <returns>bool    true if any valid output was written from this particle</returns>
		private bool PrintAny(XmlSchemaAny Any, string SettingID, int Indent, StringBuilder OutputContents, decimal MaxOccurs = -1)
		// @ATG_CHANGE : END
		{
			// Use the particle MaxOccurs value if no parent value was passed in
			if (MaxOccurs < 0)
			{
				MaxOccurs = Any.MaxOccurs;
			}

			// Loop through possibly multiple instances of this particle in the settings
			bool AnySettingFound = false;
			bool SearchArraySettings = false;
			int SettingArrayIndex = -1;
			for (int SettingIndex = 0; SettingIndex < MaxOccurs; SettingIndex++)
			{
				// Add array indexing for particles that can have multiple instances
				string LocalSettingID = SettingID;
				if (MaxOccurs > 1)
				{
					if (SearchArraySettings == true)
					{
						LocalSettingID += "[n]";
						SettingArrayIndex++;
					}
					else
					{
						LocalSettingID += "[" + SettingIndex + "]";
					}
				}
				// Get INI value and append if present
				string SimpleTypeValue = GetInterprettedSettingValue(LocalSettingID, SettingArrayIndex, Indent);
				if (SimpleTypeValue.Length > 0)
				{
					AnySettingFound = true;
					OutputContents.Append(SimpleTypeValue);
				}
				else
				{
					if (SearchArraySettings == false)
					{
						// Switch to searching array values
						SearchArraySettings = true;
					}
					else
					{
						// We early out if we find an instance with no setting applied
						break;
					}
				}
			}

			return AnySettingFound;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		private string AddNamespacePrefix(string Namespace, string ElementName)
		{
			string Prefix;
			if (AdditionalNamespaces.TryGetValue(Namespace, out Prefix))
			{
				return Prefix + ":" + ElementName;
			}

			return ElementName;
		}
		// @ATG_CHANGE : END

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Process an XML element, it's attributes, and any descendant particles. Print if any valid settings are found.
		/// </summary>
		/// <param name="Element">         XML element to parse</param>
		/// <param name="SettingID">  The INI key for the element parent</param>
		/// <param name="Indent">      Indent count for element</param>
		/// <param name="OutputContents"> Output for settings based on the any particle</param>
		/// <param name="">MaxOccurs           Optional value of parent MaxOccurs to allow cases where an intermediate particle governs</param>
		/// the MaxOccurs value of child elements.
		/// <returns>bool    true if any valid output was written from this element</returns>
		private bool PrintElement(XmlSchemaElement Element, string SettingID, int Indent, StringBuilder OutputContents, decimal MaxOccurs = -1)
		// @ATG_CHANGE : END
		{
			string ElementName = Element.Name;

			// A null element name usually means it's a reference that will be substituted. If not, pull from the qualified name.
			if (ElementName == null)
			{
				if (FixRefForSubstitution(Element, SettingID, Indent, OutputContents))
				{
					return true;
				}
				ElementName = Element.QualifiedName.Name;
			}

			// @ATG_CHANGE : BEGIN UWP Packaging support
			ElementName = AddNamespacePrefix(Element.QualifiedName.Namespace, ElementName);
			// @ATG_CHANGE : END

			// If an override wasn't specified, use the MaxOccurs value from the element itself
			if (MaxOccurs < 0)
			{
				MaxOccurs = Element.MaxOccurs;
			}

			// Basically keep track of if we found reason to keep any output and should return true from this function
			bool AnyElementFound = false;
			// Loop for multiple instances if needed
			bool SearchArraySettings = false;
			int SettingArrayIndex = -1;
			for (int ElementIndex = 0; ElementIndex < MaxOccurs; ElementIndex++)
			{
				// We store generated content in a temporary buffer in case we end up needing to discard it all
				StringBuilder LocalContents = new StringBuilder();
				// Keep track of any valid output so we know if we should keep the element
				bool AnyAttributesSet = false;
				bool AnyChildrenDefined = false;
				bool IsSimpleTypeWithValue = false;

				// Add element name and indexing as needed to the INI key
				string LocalSettingID = SettingID + "." + ElementName;
				if (MaxOccurs > 1)
				{
					if (SearchArraySettings == true)
					{
						LocalSettingID += "[n]";
						SettingArrayIndex++;
					}
					else
					{
						LocalSettingID += "[" + ElementIndex + "]";
					}
				}

				// Create Element lead in
				LocalContents.Append(GetIndentString(Indent) + "<" + ElementName);

				// Print any attributes for this element that have valid settings
				AnyAttributesSet = PrintAttributes(Element, LocalSettingID, SettingArrayIndex, LocalContents);

				// Print any data or descendants that are enclosed by this element and in any case close out the element
				if (Element.ElementSchemaType is XmlSchemaSimpleType)
				{
					XmlSchemaSimpleType SimpleType = Element.ElementSchemaType as XmlSchemaSimpleType;
					string SimpleTypeValue = GetInterprettedSettingValue(LocalSettingID, SettingArrayIndex);
					if (SimpleTypeValue.Length != 0)
					{
						IsSimpleTypeWithValue = true;
						LocalContents.AppendLine(">" + SimpleTypeValue + "</" + ElementName + ">");
					}
					else
					{
						LocalContents.AppendLine(" />");
					}
				}
				else if (Element.ElementSchemaType is XmlSchemaComplexType)
				{
					// @ATG_CHANGE : BEGIN winmd type registration support
					XmlSchemaComplexType ComplexType = Element.ElementSchemaType as XmlSchemaComplexType;
					StringBuilder TreeContents = new StringBuilder();
					
					TreeContents.AppendLine(">");
					if (LocalSettingID == "Package.Extensions")
					{
						AnyChildrenDefined = AddActivatableTypesExtensions(TreeContents, Indent + 1);
					}
					AnyChildrenDefined |= ProcessSchemaTreeElement(ComplexType.ContentTypeParticle, LocalSettingID, Indent + 1, TreeContents);

					if (AnyChildrenDefined)
					{
						LocalContents.Append(TreeContents.ToString());
						LocalContents.AppendLine(GetIndentString(Indent) + "</" + ElementName + ">");
					}
					else
					{
						LocalContents.AppendLine(" />");
					}
					// @ATG_CHANGE : END		
				}
				else
				{
					LocalContents.AppendLine(" />");
				}

				// We only include an element if any of the following are true:
				//   1. it has all required attributes set and at least one attribute set (in the case of elements with no required attributes),
				//   2. it is a simple type and it has a value set,
				//   3. it has children that meet any of the above conditions.
				if (AnyAttributesSet || IsSimpleTypeWithValue || AnyChildrenDefined)
				{
					OutputContents.Append(LocalContents.ToString());
					AnyElementFound = true;
				}
				else
				{
					if (SearchArraySettings == false)
					{
						// Switch to searching array values
						SearchArraySettings = true;
					}
					else
					{
						// If any instance of an element has no valid settings, we can early out of the loop
						break;
					}
				}
			}

			return AnyElementFound;
		}

		// @ATG_CHANGE : BEGIN winmd type registration support
		private bool AddActivatableTypesExtensions(StringBuilder TreeContents, int Indent)
		{
			bool AnyAdded = false;
			foreach (var WinMD in WinMDReferences)
			{
				TreeContents.Append(GetIndentString(Indent));
				TreeContents.AppendLine(@"<Extension Category=""windows.activatableClass.inProcessServer"">");
				++Indent;
				TreeContents.Append(GetIndentString(Indent));
				TreeContents.AppendLine(@"<InProcessServer>");
				++Indent;
				TreeContents.Append(GetIndentString(Indent));
				TreeContents.AppendFormat(@"<Path>{0}</Path>", WinMD.PackageRelativeDllPath);
				TreeContents.AppendLine();
				foreach (var WinMDType in WinMD.ActivatableTypes)
				{
					TreeContents.Append(GetIndentString(Indent));
					TreeContents.AppendFormat(@"<ActivatableClass ActivatableClassId=""{0}"" ThreadingModel=""{1}"" />", WinMDType.TypeName, WinMDType.ThreadingModelName);
					TreeContents.AppendLine();
					AnyAdded = true;
				}
				--Indent;
				TreeContents.Append(GetIndentString(Indent));
				TreeContents.AppendLine(@"</InProcessServer>");
				--Indent;
				TreeContents.Append(GetIndentString(Indent));
				TreeContents.AppendLine(@"</Extension>");
			}
			return AnyAdded;
		}
		// @ATG_CHANGE : END

		/// <summary>
		/// Callback for any XML schema validation errors.
		/// </summary>
		private static void SchemaCallback(object ValidationSender, ValidationEventArgs ValidationArgs)
		{
			// @ATG_CHANGE : BEGIN UWP Packaging support
			Log.TraceError("Validation error reading XML schema from XDK. {0}", ValidationArgs.Message);
			// @ATG_CHANGE : END
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		private void CreateAppxSchema()
		{
			AppxSchema = new XmlSchemaSet();
			AppxSchema.ValidationEventHandler += SchemaCallback;

			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder("v10.0", false);
            Version SDKVersion = VCEnvironment.FindWindowsSDKExtensionLatestVersion(SDKFolder);
            string UWPSchemaFolder = Path.Combine(SDKFolder, "Include", SDKVersion.ToString(), "winrt");

			// UWP allows the PhoneIdentity element to reference a Windows Phone package for cross-store entitlement
			// @todo: I think this creates a dependency on including the (optional) phone SDK in the Windows SD install?  That seens unfortunate.
			string PhoneSchemaFolder = Path.Combine(SDKFolder, "Extension SDKs", "WindowsMobile", SDKVersion.ToString(), "Include", "WinRT");

			AppxSchema.Add(null, XmlReader.Create(Path.Combine(UWPSchemaFolder, "UapManifestSchema.xsd")));
			AppxSchema.Add(null, XmlReader.Create(Path.Combine(UWPSchemaFolder, "FoundationManifestSchema.xsd")));
			AppxSchema.Add(null, XmlReader.Create(Path.Combine(UWPSchemaFolder, "AppxManifestTypes.xsd")));
			AppxSchema.Add(null, XmlReader.Create(Path.Combine(UWPSchemaFolder, "AppxManifestSchema2010_v2.xsd")));
			AppxSchema.Add(null, XmlReader.Create(Path.Combine(UWPSchemaFolder, "AppxManifestSchema2013.xsd")));
			AppxSchema.Add(null, XmlReader.Create(Path.Combine(PhoneSchemaFolder, "AppxPhoneManifestSchema2014.xsd")));

			AppxSchema.Compile();
		}

		private string ExeFlavor
		{
			get
			{
				switch (Platform)
				{
                    case UnrealTargetPlatform.UWP32:
                        return "x86";

					case UnrealTargetPlatform.UWP64:
						return "x64";

					case UnrealTargetPlatform.XboxOne:
						return string.Empty;

					default:
						return "x64";
				}
			}
		}
		// @ATG_CHANGE : END
			
		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Kicks off manifest generation. Will always attempt to fully calculate a new manifest but will not update the output
		/// file unless there are changes (to avoid unnecessary updates).
		/// </summary>
		/// <param name="OutputPath">     Path to write AppxManifest.xml file to.</param>
		/// <param name="RelativeExePathParam">Path to the exe relative to the package root</param>
		/// <param name="ProjectPathParam">Path to the project</param>
		/// <returns>bool    true any changes have been written to disk</returns>
		public bool CreateManifest(string OutputPath)
		{
			// Attempt to correct for directory only being sent in
			if (!OutputPath.EndsWith("AppxManifest.xml", StringComparison.CurrentCultureIgnoreCase))
			{
				OutputPath = Path.Combine(OutputPath, "AppxManifest.xml");
			}

			// Final file output
			StringBuilder OutputContents = new StringBuilder();

			// Output XML header
			// @todo: header to be determined by settings as well
			string XmlVersion;
			string XmlEncoding;
			string PackageXmlNS;
			string PackageIgnorableNS;
			EngineIni.GetString("AppxManifest", "?xml.version", out XmlVersion);
			EngineIni.GetString("AppxManifest", "?xml.encoding", out XmlEncoding);
			EngineIni.GetString("AppxManifest", "Package.xmlns", out PackageXmlNS);
			EngineIni.GetString("AppxManifest", "Package.IgnorableNamespaces", out PackageIgnorableNS);
			OutputContents.AppendLine("<?xml version=\"" + XmlVersion + "\" encoding=\"" + XmlEncoding + "\"?>");
			OutputContents.Append("<Package xmlns=\"" + PackageXmlNS + "\"");
			foreach (var Namespace in AdditionalNamespaces)
			{
				OutputContents.AppendFormat(" xmlns:{0}=\"{1}\"", Namespace.Value, Namespace.Key);
			}
			if (!string.IsNullOrEmpty(PackageIgnorableNS))
			{
				OutputContents.Append(" IgnorableNamespaces =\"" + PackageIgnorableNS + "\"");
			}
			OutputContents.AppendLine(">");
			// @ATG_CHANGE : END

			foreach (XmlSchemaElement Element in AppxSchema.GlobalElements.Values)
			{
				// We basically only include a single package entry, everything else we want sits below this
				// The root of the schema though may include many other elements
				// @todo: retrieve the package element directly
				if (Element.Name.Equals("Package", StringComparison.InvariantCultureIgnoreCase))
				{
					// Kick off the export of every particle that is a child of the package element
					XmlSchemaComplexType ComplexType = Element.ElementSchemaType as XmlSchemaComplexType;
					XmlSchemaGroupBase ElementGroup = ComplexType.ContentTypeParticle as XmlSchemaGroupBase;
					foreach (XmlSchemaParticle Particle in ElementGroup.Items)
					{
						ProcessSchemaTreeElement(Particle, "Package", 1, OutputContents);
					}

					break;
				}
			}

			// Finish file output with the closing package tag
			OutputContents.AppendLine("</Package>");

			// Check if the contents was updated and write out new contents if needed
			bool FileUpdated = false;
			if (OutputContents.Length > 0)
			{
				// @ATG_CHANGE : BEGIN UWP Packaging support
				// Validate the XML we generated
				XmlReaderSettings ReaderSettings = new XmlReaderSettings();
				ReaderSettings.ValidationType = ValidationType.Schema;
				ReaderSettings.Schemas = AppxSchema;
				ReaderSettings.ValidationEventHandler += SchemaCallback;
				using (var ContentsStream = new StringReader(OutputContents.ToString()))
				{
					using (var Reader = XmlReader.Create(ContentsStream, ReaderSettings))
					{
						while (Reader.Read())
						{
							// No-op, just reading to end to force validation.
						}
					}
				}
				// @ATG_CHANGE : END
				
				bool bFileNeedsSave = true;

				if (File.Exists(OutputPath))
				{
					// Read in the original file completely
					string LoadedFileContent = null;
					var FileAlreadyExists = File.Exists(OutputPath);
					if (FileAlreadyExists)
					{
						try
						{
							LoadedFileContent = File.ReadAllText(OutputPath);
						}
						catch (Exception)
						{
							Log.TraceInformation("Error while trying to load existing file {0}.  Ignored.", OutputPath);
						}
					}

					// Don't bother saving anything out if the new file content is the same as the old file's content
					if (LoadedFileContent != null)
					{
						var bIgnoreProjectFileWhitespaces = true;
						if (ProjectFileComparer.CompareOrdinalIgnoreCase(LoadedFileContent, OutputPath.ToString(), bIgnoreProjectFileWhitespaces) == 0)
						{
							// Exact match!
							bFileNeedsSave = false;
						}

						if (!bFileNeedsSave)
						{
							Log.TraceVerbose("Skipped saving {0} because contents haven't changed.", Path.GetFileName(OutputPath));
						}
					}
				}

				if (bFileNeedsSave)
				{
					// Save the file
					try
					{
						Directory.CreateDirectory(Path.GetDirectoryName(OutputPath));
						File.WriteAllText(OutputPath, OutputContents.ToString(), Encoding.UTF8);
						Log.TraceVerbose("Saved {0}", Path.GetFileName(OutputPath));
						FileUpdated = true;
					}
					catch (Exception)
					{
						// Unable to write to the project file.
						Log.TraceInformation("Error while trying to write file {0}.  The file is probably read-only.", OutputPath);
					}
				}
			}

			return FileUpdated;
		}
	};
}

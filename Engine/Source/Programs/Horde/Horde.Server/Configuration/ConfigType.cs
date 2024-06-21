// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace Horde.Server.Configuration
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that include other config files
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigIncludeAttribute : Attribute
	{
	}

	/// <summary>
	/// Specifies that a class is the root for including other files
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigIncludeRootAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that are relative to their containing file
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigRelativePathAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that are relative to their containing file
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigMacroScopeAttribute : Attribute
	{
	}

	/// <summary>
	/// Declares a config macro
	/// </summary>
	public class ConfigMacro
	{
		/// <summary>
		/// Name of the macro property
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// Value for the macro property
		/// </summary>
		public string Value { get; set; } = String.Empty;
	}

	/// <summary>
	/// Exception thrown when reading config files
	/// </summary>
	public sealed class ConfigException : Exception
	{
		readonly ConfigContext _context;

		/// <summary>
		/// Stack of properties
		/// </summary>
		public IEnumerable<string> ScopeStack => _context.ScopeStack;

		/// <summary>
		/// Stack of objects
		/// </summary>
		public IEnumerable<object> IncludeContextStack => _context.IncludeContextStack;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="context">Current parse context for the error</param>
		/// <param name="message">Description of the error</param>
		/// <param name="innerException">Inner exception details</param>
		internal ConfigException(ConfigContext context, string message, Exception? innerException = null)
			: base(message, innerException)
		{
			_context = context;
		}

		/// <summary>
		/// Gets the parser context when this exception was thrown. This is not exposed as a public property to avoid serializing the whole thing to Serilog.
		/// </summary>
		internal ConfigContext GetContext() => _context;
	}

	/// <summary>
	/// Base class for types that can be read from config files
	/// </summary>
	abstract class ConfigType
	{
		/// <summary>
		/// Preprocess a JSON node
		/// </summary>
		/// <param name="node">Node to preprocess</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Preprocessed node</returns>
		public async ValueTask<JsonNode?> PreprocessAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			return await PreprocessAndMergeAsync(node, null, context, cancellationToken);
		}

		static readonly ConcurrentDictionary<Type, ConfigType> s_typeToValueType = new ConcurrentDictionary<Type, ConfigType>();

		/// <summary>
		/// 
		/// </summary>
		/// <param name="type"></param>
		/// <returns></returns>
		public static ConfigType FindOrAddValueType(Type type)
		{
			ConfigType? value;
			if (!s_typeToValueType.TryGetValue(type, out value))
			{
				if (!type.IsClass || type == typeof(string))
				{
					value = new ScalarConfigType(false);
				}
				else
				{
					value = ObjectConfigType.FindOrAdd(type);
				}

				lock (s_typeToValueType)
				{
					value = s_typeToValueType.GetOrAdd(type, value);
				}
			}
			return value;
		}

		/// <summary>
		/// Reads an object from a particular URL
		/// </summary>
		/// <typeparam name="T">Type of object to read</typeparam>
		/// <param name="uri">Location of the file to read</param>
		/// <param name="context">Context for reading</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<T> ReadAsync<T>(Uri uri, ConfigContext context, CancellationToken cancellationToken) where T : class, new()
		{
			IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);
			context.IncludeStack.Push(file);

			ObjectConfigType type = ObjectConfigType.FindOrAdd(typeof(T));

			JsonObject obj = await file.ParseFileAsync(context, cancellationToken);
			obj = (JsonObject)(await type.PreprocessAndMergeAsync(obj, null, context, cancellationToken))!;

			context.IncludeStack.Pop();
			return JsonSerializer.Deserialize<T>(obj, context.JsonOptions) ?? new T();
		}

		/// <summary>
		/// Combine a relative path with a base URI to produce a new URI
		/// </summary>
		/// <param name="baseUri">Base uri to rebase relative to</param>
		/// <param name="path">Relative path</param>
		/// <returns>Absolute URI</returns>
		public static Uri CombinePaths(Uri baseUri, string path)
		{
			if (path.StartsWith("//", StringComparison.Ordinal))
			{
				if (baseUri.Scheme == PerforceConfigSource.Scheme)
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{baseUri.Host}{path}");
				}
				else
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{PerforceConnectionSettings.Default}{path}");
				}
			}
			return new Uri(baseUri, path);
		}

		/// <summary>
		/// Helper method to expand all macros within a node without performing any other processing on it
		/// </summary>
		protected static JsonNode? ExpandMacros(JsonNode? node, ConfigContext context)
		{
			if (node == null)
			{
				return node;
			}
			else if (node is JsonObject obj)
			{
				JsonObject result = new JsonObject();
				foreach ((string propertyName, JsonNode? propertyNode) in obj)
				{
					result[propertyName] = ExpandMacros(propertyNode, context);
				}
				return result;
			}
			else if (node is JsonArray arr)
			{
				JsonArray result = new JsonArray();
				foreach (JsonNode? elementNode in arr)
				{
					result.Add(ExpandMacros(elementNode, context));
				}
				return result;
			}
			else if (node is JsonValue val && val.GetValueKind() == JsonValueKind.String)
			{
				string strValue = ((string?)val)!;
				string expandedStrValue = context.ExpandMacros(strValue);
				return JsonValue.Create(expandedStrValue);
			}
			else
			{
				return node.DeepClone();
			}
		}

		/// <summary>
		/// Determines whether this type contains macro definitions
		/// </summary>
		internal abstract bool HasMacroDefinitions();

		/// <summary>
		/// Parses macro definitions from this object
		/// </summary>
		/// <param name="node">Node to parse macros from</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="macros">Macros parsed from the boject</param>
		internal abstract void ParseMacroDefinitions(JsonNode node, ConfigContext context, Dictionary<string, string> macros);

		/// <summary>
		/// Whether this type contains include directives
		/// </summary>
		internal abstract bool HasIncludes();

		/// <summary>
		/// Reads a node into a target object (possibly merging with an existing property)
		/// </summary>
		/// <param name="node">Node to preprocess</param>
		/// <param name="existingNode">Optional existing node to merge with. Can be modified.</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The preprocessed node. May be existingNode if changes are merged with it.</returns>
		internal abstract Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Traverse the property tree starting with 'node' and process any include directives, merging the results into the given target object.
		/// </summary>
		/// <param name="node"></param>
		/// <param name="target"></param>
		/// <param name="targetType"></param>
		/// <param name="context"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		internal abstract Task<JsonObject> ParseIncludesAsync(JsonNode node, JsonObject target, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> for scalar types
	/// </summary>
	class ScalarConfigType : ConfigType
	{
		readonly bool _relativePath;

		public ScalarConfigType(bool relativePath)
			=> _relativePath = relativePath;

		/// <inheritdoc/>
		internal override bool HasMacroDefinitions() => false;

		/// <inheritdoc/>
		internal override void ParseMacroDefinitions(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros) { }

		/// <inheritdoc/>
		internal override bool HasIncludes() => false;

		/// <inheritdoc/>
		internal override Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonNode? result;
			if (node == null)
			{
				result = null;
			}
			else if (_relativePath && node is JsonValue value && value.GetValueKind() == JsonValueKind.String)
			{
				result = JsonValue.Create(CombinePaths(context.CurrentFile, value.ToString()).AbsoluteUri);
			}
			else
			{
				result = ExpandMacros(node, context);
			}

			return Task.FromResult(result);
		}

		/// <inheritdoc/>
		internal override Task<JsonObject> ParseIncludesAsync(JsonNode node, JsonObject targetObject, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			=> Task.FromResult(targetObject);
	}

	/// <summary>
	/// Property which specifies the path of another config file to include 
	/// </summary>
	class IncludeConfigType : ScalarConfigType
	{
		public IncludeConfigType(bool relativePath)
			: base(relativePath)
		{ }

		/// <inheritdoc/>
		internal override bool HasIncludes() => true;

		/// <inheritdoc/>
		internal override async Task<JsonObject> ParseIncludesAsync(JsonNode jsonNode, JsonObject targetObject, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			string? path = (string?)jsonNode;

			Uri uri = ConfigType.CombinePaths(context.CurrentFile, context.ExpandMacros(path!));
			IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);

			context.IncludeStack.Push(file);

			JsonObject includedJsonObject = await file.ParseFileAsync(context, cancellationToken);
			JsonNode? result = await targetType.PreprocessAndMergeAsync(includedJsonObject, targetObject, context, cancellationToken);

			context.IncludeStack.Pop();

			return (JsonObject)result!;
		}
	}

	/// <summary>
	/// Property containing a binary resource
	/// </summary>
	class ResourceConfigType : ScalarConfigType
	{
		public ResourceConfigType()
			: base(false)
		{ }

		/// <inheritdoc/>
		internal override async Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			Uri uri = CombinePaths(context.CurrentFile, JsonSerializer.Deserialize<string>(node, context.JsonOptions) ?? String.Empty);
			IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);

			ConfigResource resource = new ConfigResource();
			resource.Path = uri.AbsoluteUri;
			resource.Data = await file.ReadAsync(cancellationToken);

			return JsonSerializer.SerializeToNode(resource, context.JsonOptions);
		}
	}

	/// <summary>
	/// Array of Json values
	/// </summary>
	class ArrayConfigType : ConfigType
	{
		readonly ConfigType _elementType;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArrayConfigType(ConfigType elementType)
		{
			_elementType = elementType;
		}

		/// <inheritdoc/>
		internal override bool HasMacroDefinitions() => _elementType is ObjectConfigType elementType && elementType.HasMacroDefinitions();

		/// <inheritdoc/>
		internal override void ParseMacroDefinitions(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
		{
			if (jsonNode is JsonArray jsonArrayValue)
			{
				ObjectConfigType classElementType = (ObjectConfigType)_elementType;
				foreach (JsonObject jsonObjectElement in jsonArrayValue.OfType<JsonObject>())
				{
					classElementType.ParseMacroDefinitions(jsonObjectElement, context, macros);
				}
			}
		}

		/// <inheritdoc/>
		internal override bool HasIncludes() => _elementType is ObjectConfigType elementType && elementType.HasIncludes();

		/// <inheritdoc/>
		internal override async Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonArray targetArray = ((JsonArray?)existingNode) ?? new JsonArray();
			foreach (JsonNode? element in (JsonArray)node!)
			{
				context.EnterScope($"[{targetArray.Count}]");

				JsonNode? elementValue = await _elementType.PreprocessAsync(element, context, cancellationToken);
				targetArray.Add(elementValue);

				context.LeaveScope();
			}
			return targetArray;
		}

		/// <inheritdoc/>
		internal override async Task<JsonObject> ParseIncludesAsync(JsonNode node, JsonObject targetObj, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is JsonArray arrayNode)
			{
				ObjectConfigType classElementType = (ObjectConfigType)_elementType;
				foreach (JsonObject jsonObjectElement in arrayNode.OfType<JsonObject>())
				{
					targetObj = await classElementType.ParseIncludesAsync(jsonObjectElement, targetObj, targetType, context, cancellationToken);
				}
			}
			return targetObj;
		}
	}

	/// <summary>
	/// Arbitary mapping of string values to keys
	/// </summary>
	class DictionaryConfigType : ConfigType
	{
		readonly ConfigType _elementType;

		public DictionaryConfigType(ConfigType elementType)
		{
			_elementType = elementType;
		}

		/// <inheritdoc/>
		internal override bool HasMacroDefinitions() => false;

		/// <inheritdoc/>
		internal override void ParseMacroDefinitions(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros) { }

		/// <inheritdoc/>
		internal override bool HasIncludes() => _elementType is ObjectConfigType elementType && elementType.HasIncludes();

		/// <inheritdoc/>
		internal override async Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonObject? targetObject = ((JsonObject?)existingNode) ?? new JsonObject();
			foreach ((string key, JsonNode? element) in (JsonObject)node!)
			{
				context.EnterScope($"[{key}]");

				JsonNode? elementValue = await _elementType.PreprocessAsync(element, context, cancellationToken);
				targetObject[key] = elementValue;

				context.LeaveScope();
			}
			return targetObject;
		}

		/// <inheritdoc/>
		internal override async Task<JsonObject> ParseIncludesAsync(JsonNode node, JsonObject targetObj, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is JsonObject obj && _elementType is ObjectConfigType classElementType)
			{
				foreach (JsonObject jsonObjectElement in obj.Select(x => x.Value).OfType<JsonObject>())
				{
					targetObj = await classElementType.ParseIncludesAsync(jsonObjectElement, targetObj, targetType, context, cancellationToken);
				}
			}
			return targetObj;
		}
	}

	/// <summary>
	/// Special config type for storing a json node
	/// </summary>
	class JsonNodeConfigType : ConfigType
	{
		/// <inheritdoc/>
		internal override bool HasMacroDefinitions() => false;

		/// <inheritdoc/>
		internal override void ParseMacroDefinitions(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
		{
		}

		/// <inheritdoc/>
		internal override bool HasIncludes() => false;

		/// <inheritdoc/>
		internal override Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			return Task.FromResult(node?.DeepClone());
		}

		static JsonNode? MergeNodes(JsonNode? first, JsonNode? second)
		{
			if (second == null)
			{
				return first?.DeepClone();
			}
			else if (first is JsonObject firstObj && second is JsonObject secondObj)
			{
				JsonObject mergedObj = new JsonObject(firstObj);
				foreach ((string childName, JsonNode? childNode) in secondObj)
				{
					mergedObj[childName] = MergeNodes(firstObj[childName], childNode);
				}
				return mergedObj;
			}
			else
			{
				return second.DeepClone();
			}
		}

		/// <inheritdoc/>
		internal override Task<JsonObject> ParseIncludesAsync(JsonNode jsonNode, JsonObject targetObj, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			=> Task.FromResult(targetObj);
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> to handle class types
	/// </summary>
	class ObjectConfigType : ConfigType
	{
		readonly bool _isIncludeRoot;
		readonly bool _isMacro;
		readonly bool _isMacroScope;
		readonly Dictionary<string, ConfigType> _nameToProperty;
		readonly Dictionary<string, ConfigType> _nameToMacroProperty = new Dictionary<string, ConfigType>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, ConfigType> _nameToIncludeProperty = new Dictionary<string, ConfigType>(StringComparer.OrdinalIgnoreCase);

		static readonly ConcurrentDictionary<Type, ObjectConfigType> s_typeToObjectValueType = new ConcurrentDictionary<Type, ObjectConfigType>();

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectConfigType(bool isIncludeRoot, bool isMacro, bool isMacroScope, IEnumerable<KeyValuePair<string, ConfigType>> properties)
		{
			_isIncludeRoot = isIncludeRoot;
			_isMacro = isMacro;
			_isMacroScope = isMacroScope;

			_nameToProperty = new Dictionary<string, ConfigType>(properties, StringComparer.OrdinalIgnoreCase);

			// Build a map of all the properties which can contain macros or include other files
			foreach ((string name, ConfigType property) in _nameToProperty)
			{
				if (property.HasMacroDefinitions())
				{
					_nameToMacroProperty.Add(name, property);
				}
				if (property.HasIncludes())
				{
					_nameToIncludeProperty.Add(name, property);
				}
			}
		}

		static ObjectConfigType FromType(Type type)
		{
			bool isIncludeRoot = type.GetCustomAttribute<ConfigIncludeRootAttribute>() != null;
			bool isMacro = type == typeof(ConfigMacro);
			bool isMacroScope = type.GetCustomAttribute<ConfigMacroScopeAttribute>() != null;

			// Find all the direct include properties
			Dictionary<string, ConfigType> nameToProperty = new Dictionary<string, ConfigType>(StringComparer.OrdinalIgnoreCase);

			PropertyInfo[] propertyInfos = type.GetProperties(BindingFlags.Instance | BindingFlags.Public | BindingFlags.GetProperty);
			foreach (PropertyInfo propertyInfo in propertyInfos)
			{
				if (propertyInfo.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					string name = propertyInfo.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ?? propertyInfo.Name;
					nameToProperty.Add(name, CreateProperty(propertyInfo));
				}
			}

			// Create the type
			return new ObjectConfigType(isIncludeRoot, isMacro, isMacroScope, nameToProperty);
		}

		/// <inheritdoc/>
		internal override bool HasMacroDefinitions() => !_isMacroScope && (_isMacro || _nameToMacroProperty.Count > 0);

		/// <inheritdoc/>
		internal override bool HasIncludes() => !_isIncludeRoot && _nameToIncludeProperty.Count > 0;

		public static ObjectConfigType FindOrAdd(Type type)
		{
			ObjectConfigType? value;
			if (!s_typeToObjectValueType.TryGetValue(type, out value))
			{
				lock (s_typeToObjectValueType)
				{
					if (!s_typeToObjectValueType.TryGetValue(type, out value))
					{
						value = ObjectConfigType.FromType(type);
						s_typeToObjectValueType.TryAdd(type, value);
					}
				}
			}
			return value;
		}

		static ConfigType CreateProperty(PropertyInfo propertyInfo)
		{
			Type propertyType = propertyInfo.PropertyType;
			if (!propertyType.IsClass || propertyType == typeof(string))
			{
				bool relativePath = propertyInfo.GetCustomAttribute<ConfigRelativePathAttribute>() != null;
				if (propertyInfo.GetCustomAttribute<ConfigIncludeAttribute>() != null)
				{
					return new IncludeConfigType(relativePath);
				}
				else
				{
					return new ScalarConfigType(relativePath);
				}
			}
			else
			{
				if (propertyType.IsAssignableTo(typeof(ConfigResource)))
				{
					return new ResourceConfigType();
				}
				else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(List<>))
				{
					Type elementType = propertyType.GetGenericArguments()[0];
					return new ArrayConfigType(FindOrAddValueType(elementType));
				}
				else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(Dictionary<,>))
				{
					Type elementType = propertyType.GetGenericArguments()[1];
					return new DictionaryConfigType(FindOrAddValueType(elementType));
				}
				else if (propertyType.IsAssignableTo(typeof(JsonNode)))
				{
					return new JsonNodeConfigType();
				}
				else
				{
					return FindOrAdd(propertyType);
				}
			}
		}

		public async ValueTask<JsonObject> ReadAsync(JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			return (JsonObject)(await PreprocessAndMergeAsync(obj, null, context, cancellationToken))!;
		}

		/// <inheritdoc/>
		internal override void ParseMacroDefinitions(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
		{
			JsonObject jsonObject = (JsonObject)jsonNode;
			if (_isMacro)
			{
				ConfigMacro? macro = JsonSerializer.Deserialize<ConfigMacro>(jsonObject, context.JsonOptions);
				if (macro != null)
				{
					macros.Add(macro.Name, macro.Value);
				}
			}
			else
			{
				foreach ((string name, JsonNode? node) in jsonObject)
				{
					if (node != null && _nameToMacroProperty.TryGetValue(name, out ConfigType? property))
					{
						property.ParseMacroDefinitions(node, context, macros);
					}
				}
			}
		}

		/// <inheritdoc/>
		internal override async Task<JsonNode?> PreprocessAndMergeAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is not JsonObject newObject)
			{
				return node?.DeepClone();
			}

			JsonObject? target = (JsonObject?)existingNode ?? new JsonObject();

			// Before parsing properties into this object, read all the includes recursively
			if (_isIncludeRoot)
			{
				await ParseIncludesAsync(newObject, target, this, context, cancellationToken);
			}

			// Parse all the macros for this scope
			if (_isMacroScope)
			{
				Dictionary<string, string> macros = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
				ParseMacroDefinitions(newObject, context, macros);
				context.MacroScopes.Add(macros);
			}

			// Parse all the properties into this object
			foreach ((string name, JsonNode? newNode) in newObject)
			{
				if (newNode is JsonValue)
				{
					context.AddProperty(name);
				}

				if (_nameToProperty.TryGetValue(name, out ConfigType? property))
				{
					context.EnterScope(name);
					target[name] = await property.PreprocessAndMergeAsync(newNode, target[name], context, cancellationToken);
					context.LeaveScope();
				}
				else
				{
					target[name] = ExpandMacros(newNode, context);
				}
			}

			// Parse all the macros for this scope
			if (_isMacroScope)
			{
				context.MacroScopes.RemoveAt(context.MacroScopes.Count - 1);
			}

			return target;
		}

		/// <inheritdoc/>
		internal override async Task<JsonObject> ParseIncludesAsync(JsonNode node, JsonObject targetObj, ObjectConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is JsonObject obj)
			{
				foreach ((string name, JsonNode? propertyNode) in obj)
				{
					if (_nameToIncludeProperty.TryGetValue(name, out ConfigType? property) && propertyNode != null)
					{
						targetObj = await property.ParseIncludesAsync(propertyNode, targetObj, targetType, context, cancellationToken);
					}
				}
			}
			return targetObj;
		}
	}
}

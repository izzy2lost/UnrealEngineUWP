// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
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
		public abstract ValueTask<JsonNode?> PreprocessAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken);

		static readonly ConcurrentDictionary<Type, ConfigType> s_typeToValueType = new ConcurrentDictionary<Type, ConfigType>();

		public static ConfigType FindOrAddValueType(Type type)
		{
			ConfigType? value;
			if (!s_typeToValueType.TryGetValue(type, out value))
			{
				if (!type.IsClass || type == typeof(string))
				{
					value = new ScalarConfigType(type);
				}
				else
				{
					value = ClassConfigType.FindOrAdd(type);
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
			ClassConfigType type = ClassConfigType.FindOrAdd(typeof(T));

			JsonObject target = new JsonObject();
			await type.MergeObjectAsync(target, uri, context, cancellationToken);

			return JsonSerializer.Deserialize<T>(target, context.JsonOptions) ?? new T();
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
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> for scalar types
	/// </summary>
	class ScalarConfigType : ConfigType
	{
		readonly Type _type;

		public ScalarConfigType(Type type)
		{
			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
			{
				type = type.GetGenericArguments()[0];
			}
			_type = type;
		}

		public override ValueTask<JsonNode?> PreprocessAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonNode? result;
			if (node == null)
			{
				result = null;
			}
			else
			{
				result = ExpandMacros(node, context);
			}

			return new ValueTask<JsonNode?>(result);
		}
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> to handle class types
	/// </summary>
	class ClassConfigType : ConfigType
	{
		abstract class Property
		{
			public string Name { get; }

			protected Property(string name)
				=> Name = name;

			public abstract bool HasMacros();

			public abstract void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros);

			public abstract bool HasIncludes();

			// Reads a node into a target object (possibly merging with an existing property)
			public abstract Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken);

			// Traverse the property tree starting with 'node' and process any include directives, merging the results into the given target object.
			public abstract Task ParseIncludesAsync(JsonNode node, JsonObject target, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken);
		}

		class ScalarProperty : Property
		{
			readonly bool _relativePath;

			public ScalarProperty(string name, bool relativePath)
				: base(name)
			{
				_relativePath = relativePath;
			}

			public override bool HasMacros() => false;

			public override void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros) { }

			public override bool HasIncludes() => false;

			public override Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				context.AddProperty(Name);

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
				target[Name] = result;

				return Task.CompletedTask;
			}

			public override Task ParseIncludesAsync(JsonNode node, JsonObject targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
				=> Task.CompletedTask;
		}

		class IncludeProperty : ScalarProperty
		{
			public IncludeProperty(string name, bool relativePath)
				: base(name, relativePath)
			{ }

			public override bool HasIncludes() => true;

			public override async Task ParseIncludesAsync(JsonNode jsonNode, JsonObject targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				string? path = (string?)jsonNode;

				Uri uri = ConfigType.CombinePaths(context.CurrentFile, context.ExpandMacros(path!));
				IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

				context.IncludeStack.Push(file);

				JsonObject includedJsonObject = await ParseFileAsync(file, context, cancellationToken);
				await targetType.MergeIntoObjectAsync(includedJsonObject, targetObject, context, cancellationToken);

				context.IncludeStack.Pop();
			}
		}

		class ResourceProperty : ScalarProperty
		{
			public ResourceProperty(string name)
				: base(name, false)
			{
			}

			public override async Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				context.AddProperty(Name);

				Uri uri = CombinePaths(context.CurrentFile, JsonSerializer.Deserialize<string>(node, context.JsonOptions) ?? String.Empty);
				IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

				ConfigResource resource = new ConfigResource();
				resource.Path = uri.AbsoluteUri;
				resource.Data = await file.ReadAsync(cancellationToken);

				target[Name] = JsonSerializer.SerializeToNode(resource, context.JsonOptions);
			}
		}

		class ListProperty : Property
		{
			readonly ConfigType _elementType;

			public ListProperty(string name, ConfigType elementType)
				: base(name)
			{
				_elementType = elementType;
			}

			public override bool HasMacros() => _elementType is ClassConfigType elementType && elementType.HasMacros();

			public override void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
			{
				if (jsonNode is JsonArray jsonArrayValue)
				{
					ClassConfigType classElementType = (ClassConfigType)_elementType;
					foreach (JsonObject jsonObjectElement in jsonArrayValue.OfType<JsonObject>())
					{
						classElementType.ParseMacros(jsonObjectElement, context, macros);
					}
				}
			}

			public override bool HasIncludes() => _elementType is ClassConfigType elementType && elementType.HasIncludes();

			public override async Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				JsonArray? targetArray = target[Name]?.AsArray();
				if (targetArray == null)
				{
					targetArray = new JsonArray();
					target[Name] = targetArray;
				}

				foreach (JsonNode? element in (JsonArray)node!)
				{
					context.EnterScope($"{Name}[{targetArray.Count}]");

					JsonNode? elementValue = await _elementType.PreprocessAsync(element, context, cancellationToken);
					targetArray.Add(elementValue);

					context.LeaveScope();
				}
			}

			public override async Task ParseIncludesAsync(JsonNode node, JsonObject targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (node is JsonArray arrayNode)
				{
					ClassConfigType classElementType = (ClassConfigType)_elementType;
					foreach (JsonObject jsonObjectElement in arrayNode.OfType<JsonObject>())
					{
						await classElementType.ParseIncludesAsync(jsonObjectElement, targetObject, targetType, context, cancellationToken);
					}
				}
			}
		}

		class DictionaryProperty : Property
		{
			readonly ConfigType _elementType;

			public DictionaryProperty(string name, ConfigType elementType)
				: base(name)
			{
				_elementType = elementType;
			}

			public override bool HasMacros() => false;

			public override void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros) { }

			public override bool HasIncludes() => _elementType is ClassConfigType elementType && elementType.HasIncludes();

			public override async Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				JsonObject? targetObject = target[Name]?.AsObject();
				if (targetObject == null)
				{
					targetObject = new JsonObject();
					target[Name] = targetObject;
				}

				foreach ((string key, JsonNode? element) in (JsonObject)node!)
				{
					context.EnterScope($"{Name}[{key}]");

					JsonNode? elementValue = await _elementType.PreprocessAsync(element, context, cancellationToken);
					targetObject[key] = elementValue;

					context.LeaveScope();
				}
			}

			public override async Task ParseIncludesAsync(JsonNode node, JsonObject targetObj, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (node is JsonObject obj && _elementType is ClassConfigType classElementType)
				{
					foreach (JsonObject jsonObjectElement in obj.Select(x => x.Value).OfType<JsonObject>())
					{
						await classElementType.ParseIncludesAsync(jsonObjectElement, targetObj, targetType, context, cancellationToken);
					}
				}
			}
		}

		class JsonNodeProperty : Property
		{
			public JsonNodeProperty(string name)
				: base(name)
			{
			}

			public override bool HasMacros() => false;

			public override void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
			{
			}

			public override bool HasIncludes() => false;

			public override Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				target[Name] = node?.DeepClone();
				return Task.CompletedTask;
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

			public override Task ParseIncludesAsync(JsonNode jsonNode, JsonObject targetObj, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
				=> Task.CompletedTask;
		}

		class ObjectProperty : Property
		{
			readonly ClassConfigType _classConfigType;

			public ObjectProperty(string name, ClassConfigType classConfigType)
				: base(name)
			{
				_classConfigType = classConfigType;
			}

			public override bool HasMacros() => _classConfigType.HasMacros();

			public override void ParseMacros(JsonNode jsonNode, ConfigContext context, Dictionary<string, string> macros)
			{
				if (jsonNode is JsonObject obj)
				{
					_classConfigType.ParseMacros(obj, context, macros);
				}
			}

			public override bool HasIncludes() => _classConfigType.HasIncludes();

			public override async Task MergeIntoObjectAsync(JsonNode? node, JsonObject target, ConfigContext context, CancellationToken cancellationToken)
			{
				if (node is JsonObject obj)
				{
					Uri? otherFile;
					context.TryAddProperty(Name, out otherFile);

					context.EnterScope(Name);

					JsonObject? childTarget = target[Name] as JsonObject;
					if (childTarget == null)
					{
						if (otherFile != null)
						{
							throw new ConfigException(context, $"Property {context.CurrentScope}.{Name} conflicts with value in {otherFile}.");
						}

						target[Name] = await _classConfigType.ReadAsync(obj, context, cancellationToken);
					}
					else
					{
						await _classConfigType.MergeIntoObjectAsync(obj, childTarget, context, cancellationToken);
					}

					context.LeaveScope();
				}
				else
				{
					context.AddProperty(Name);

					target[Name] = node?.DeepClone();
				}
			}

			public override async Task ParseIncludesAsync(JsonNode node, JsonObject targetObj, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (node is JsonObject obj)
				{
					await _classConfigType.ParseIncludesAsync(obj, targetObj, targetType, context, cancellationToken);
				}
			}
		}

		readonly bool _isIncludeRoot;
		readonly bool _isMacro;
		readonly bool _isMacroScope;
		readonly Dictionary<string, Property> _nameToProperty = new Dictionary<string, Property>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, Property> _nameToMacroProperty = new Dictionary<string, Property>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, Property> _nameToIncludeProperty = new Dictionary<string, Property>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, ClassConfigType>? _knownTypes;

		static readonly ConcurrentDictionary<Type, ClassConfigType> s_typeToObjectValueType = new ConcurrentDictionary<Type, ClassConfigType>();

		public ClassConfigType(Type type)
		{
			s_typeToObjectValueType.TryAdd(type, this);

			_isIncludeRoot = type.GetCustomAttribute<ConfigIncludeRootAttribute>() != null;
			_isMacro = type == typeof(ConfigMacro);
			_isMacroScope = type.GetCustomAttribute<ConfigMacroScopeAttribute>() != null;

			// Find all the direct include properties
			PropertyInfo[] propertyInfos = type.GetProperties(BindingFlags.Instance | BindingFlags.Public | BindingFlags.GetProperty);
			foreach (PropertyInfo propertyInfo in propertyInfos)
			{
				if (propertyInfo.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					string name = propertyInfo.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ?? propertyInfo.Name;
					_nameToProperty.Add(name, CreateProperty(name, propertyInfo));
				}
			}

			// Build a map of all the properties which can contain macros or include other files
			foreach (Property property in _nameToProperty.Values)
			{
				if (property.HasMacros())
				{
					_nameToMacroProperty.Add(property.Name, property);
				}
				if (property.HasIncludes())
				{
					_nameToIncludeProperty.Add(property.Name, property);
				}
			}

			// Build up a list of possible types for this object
			JsonKnownTypesAttribute? knownTypes = type.GetCustomAttribute<JsonKnownTypesAttribute>();
			if (knownTypes != null)
			{
				_knownTypes = new Dictionary<string, ClassConfigType>(StringComparer.Ordinal);
				foreach (Type knownType in knownTypes.Types)
				{
					ClassConfigType knownConfigType = FindOrAdd(knownType);
					foreach (JsonDiscriminatorAttribute discriminatorAttribute in knownType.GetCustomAttributes(typeof(JsonDiscriminatorAttribute), true))
					{
						_knownTypes.Add(discriminatorAttribute.Name, knownConfigType);
					}
				}
			}
		}

		bool HasMacros() => !_isMacroScope && (_isMacro || _nameToMacroProperty.Count > 0);

		bool HasIncludes() => !_isIncludeRoot && _nameToIncludeProperty.Count > 0;

		public static ClassConfigType FindOrAdd(Type type)
		{
			ClassConfigType? value;
			if (!s_typeToObjectValueType.TryGetValue(type, out value))
			{
				lock (s_typeToObjectValueType)
				{
					if (!s_typeToObjectValueType.TryGetValue(type, out value))
					{
						value = new ClassConfigType(type);
					}
				}
			}
			return value;
		}

		static Property CreateProperty(string name, PropertyInfo propertyInfo)
		{
			Type propertyType = propertyInfo.PropertyType;
			if (!propertyType.IsClass || propertyType == typeof(string))
			{
				bool relativePath = propertyInfo.GetCustomAttribute<ConfigRelativePathAttribute>() != null;
				if (propertyInfo.GetCustomAttribute<ConfigIncludeAttribute>() != null)
				{
					return new IncludeProperty(name, relativePath);
				}
				else
				{
					return new ScalarProperty(name, relativePath);
				}
			}
			else
			{
				if (propertyType.IsAssignableTo(typeof(ConfigResource)))
				{
					return new ResourceProperty(name);
				}
				else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(List<>))
				{
					Type elementType = propertyType.GetGenericArguments()[0];
					return new ListProperty(name, FindOrAddValueType(elementType));
				}
				else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(Dictionary<,>))
				{
					Type elementType = propertyType.GetGenericArguments()[1];
					return new DictionaryProperty(name, FindOrAddValueType(elementType));
				}
				else if (propertyType.IsAssignableTo(typeof(JsonNode)))
				{
					return new JsonNodeProperty(name);
				}
				else
				{
					return new ObjectProperty(name, FindOrAdd(propertyType));
				}
			}
		}

		public override async ValueTask<JsonNode?> PreprocessAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node == null)
			{
				throw new ConfigException(context, "Unable to deserialize object from null value");
			}
			else if (node is JsonObject obj)
			{
				return await ReadAsync(obj, context, cancellationToken);
			}
			else
			{
				return node;
			}
		}

		public async ValueTask<JsonObject> ReadAsync(JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			ClassConfigType targetType = this;
			if (_knownTypes != null && obj.TryGetPropertyValue("Type", out JsonNode? knownTypeNode) && knownTypeNode != null)
			{
				targetType = _knownTypes[knownTypeNode.ToString()];
			}

			JsonObject result = new JsonObject();
			await targetType.MergeIntoObjectAsync(obj, result, context, cancellationToken);

			return result;
		}

		static async ValueTask<IConfigFile> ReadFileAsync(Uri uri, ConfigContext context, CancellationToken cancellationToken)
		{
			IConfigSource? source = context.Sources[uri.Scheme];
			if (source == null)
			{
				throw new ConfigException(context, $"Invalid/unknown scheme for config file {uri}");
			}

			IConfigFile? file;
			if (!context.Files.TryGetValue(uri, out file))
			{
				file = await source.GetAsync(uri, cancellationToken);
				context.Files.Add(uri, file);
			}

			return file;
		}

		static async ValueTask<JsonObject> ParseFileAsync(IConfigFile file, ConfigContext context, CancellationToken cancellationToken)
		{
			ReadOnlyMemory<byte> data = await file.ReadAsync(cancellationToken);

			JsonObject? obj = JsonSerializer.Deserialize<JsonObject>(data.Span, context.JsonOptions);
			if (obj == null)
			{
				throw new ConfigException(context, $"Config file {file.Uri} contains a null object.");
			}

			return obj;
		}

		public async Task MergeObjectAsync(JsonObject target, Uri uri, ConfigContext context, CancellationToken cancellationToken)
		{
			if (context.IncludeStack.Any(x => x.Uri == uri))
			{
				throw new ConfigException(context, $"Recursive include of file {uri}");
			}

			IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

			context.IncludeStack.Push(file);

			JsonObject obj = await ParseFileAsync(file, context, cancellationToken);
			await MergeIntoObjectAsync(obj, target, context, cancellationToken);

			context.IncludeStack.Pop();
		}

		async Task MergeIntoObjectAsync(JsonObject newObject, JsonObject targetObject, ConfigContext context, CancellationToken cancellationToken)
		{
			// Before parsing properties into this object, read all the includes recursively
			if (_isIncludeRoot)
			{
				await ParseIncludesAsync(newObject, targetObject, this, context, cancellationToken);
			}

			// Parse all the macros for this scope
			if (_isMacroScope)
			{
				Dictionary<string, string> macros = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
				ParseMacros(newObject, context, macros);
				context.MacroScopes.Add(macros);
			}

			// Parse all the properties into this object
			foreach ((string name, JsonNode? newNode) in newObject)
			{
				if (_nameToProperty.TryGetValue(name, out Property? property))
				{
					await property.MergeIntoObjectAsync(newNode, targetObject, context, cancellationToken);
				}
				else
				{
					targetObject[name] = ExpandMacros(newNode, context);
				}
			}

			// Parse all the macros for this scope
			if (_isMacroScope)
			{
				context.MacroScopes.RemoveAt(context.MacroScopes.Count - 1);
			}
		}

		void ParseMacros(JsonObject jsonObject, ConfigContext context, Dictionary<string, string> macros)
		{
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
					if (node != null && _nameToMacroProperty.TryGetValue(name, out Property? property))
					{
						property.ParseMacros(node, context, macros);
					}
				}
			}
		}

		async Task ParseIncludesAsync(JsonObject obj, JsonObject targetObj, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			foreach ((string name, JsonNode? node) in obj)
			{
				if (_nameToIncludeProperty.TryGetValue(name, out Property? property) && node != null)
				{
					await property.ParseIncludesAsync(node, targetObj, targetType, context, cancellationToken);
				}
			}
		}
	}
}

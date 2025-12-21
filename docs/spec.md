# The Jaguar Specification

```{topic} Specification Licensing
The Jaguar specification and supporting documents are provided and licensed under Creative Commons Attribution-ShareAlike 4.0 International. To view a copy of this license, visit [https://creativecommons.org/licenses/by-sa/4.0/](https://creativecommons.org/licenses/by-sa/4.0/).
```

The Jaguar stream format is a stream-oriented binary data exchange format with an emphasis on simplicity and efficiency. Jaguar streams are self-descriptive and statically typed, feature built-in vector and matrix math types, and allow for the embedding of substreams and byte buffers.

This is the definitive specification for the Jaguar stream format. This is mainly for people looking to more deeply understand the format itself, or those wishing to write their own encoder/decoder (we recommend taking a look at `libjaguar`, the reference library, to understand how this might be accomplished).  

All conforming implementations must follow this specification. Please note: Jaguar is not designed to be an extensible format; as such, any custom extensions or features **will not** be described here.

## 0. Before We Begin - A Few Simple Notes
Before the more complicated parts of the specification, here are a few small rules that are important but don't require a whole section themselves:

* Jaguar streams use little-endian byte order.

* All strings are stored using Pascal format - a size in bytes followed by the actual string data, which has no terminator.

* All strings are encoded using UTF-8.

* Jaguar is byte-packed, but does not contain an explicit synchronization system. This is because Jaguar is not designed to be used in a lossy environment (such as networking).

* All floating-point values are encoded in accordance with the IEEE 754 LE standard.

* While Jaguar can be used to encode hierarchical data, it is worth noting that **Jaguar is not an object tree**; it is fundamentally a data stream system and has no concept of a top-level tree or DOM.

## 1. Values and Data
A Jaguar stream is fairly simple, conceptually. It is simply a continued stream of multiple `Value`s.  

A `Value` consists of the following information:
| Field | Size (bytes) |
| ----- | ---- |
| `TypeTag` | 1 |
| Name string size | 1 |
| Name string | (size above) |
| Data | (size determined by `TypeTag`) |  

The data itself is determined by what type the value belongs to, identified by the `TypeTag`.  

All data is split into a header and a body. The header contains the data required to identify the size of and correctly interpret the body data (such as the element count for lists). This allows decoders to skip over the body data and continue reading headers to identify the structure of the stream without needing to parse the data immediately.  

The name string is case-sensitive and must be unique across all `Value`s in its scope (stream root or within the same parent object). If a field with a duplicate name is encountered, decoders **may** either declare the stream invalid and terminate decoding **or** continue, ignoring the invalid object.  

If, according to this specification, a decoder is supposed to "ignore" a `Value`, that means that the decoder **must** automatically advance the stream to the next valid position and not inform the consuming application of the `Value`'s existence. If it has already done so due to the `Value` being valid before that point, the decoder **must** invalidate the returned data as it is able.

## 2. Typing
Jaguar is a statically-typed format. All data must have a corresponding type. This is useful for decoding because it allows for easier validation of the intended structure and also helps with maintaining internal consistency.  

All `Value`s in a Jaguar stream have a `TypeTag`, a one-byte sequence which identifies the start of a new `Value` and determines its type (except in cases of value embedding where the enclosing type provides a method for clarifying this).  

`TypeTag`s are grouped using their upper nibble (4 bits) by category, those categories being:  

0. Buffers, floating-point numbers, and booleans
1. Signed integers
2. Unsigned integers
3. Element containers
4. Math types

Below is the list of types in Jaguar and their `TypeTag`s:
| Type | `TypeTag` (hex) |
| ---- | --------------- |
| String | `0x0A` |
| Byte Buffer | `0x0B` |
| Substream | `0x0C` |
| Boolean | `0x0D` |
| Floating-Point (single-precision, 32 bits) | `0x0E` |
| Floating-Point (double-precision, 64 bits) | `0x0F` |
| Signed Integer (8-bit) | `0x1A` |
| Signed Integer (16-bit) | `0x1B` |
| Signed Integer (32-bit) | `0x1C` |
| Signed Integer (64-bit) | `0x1D` |
| Unsigned Integer (8-bit) | `0x2A` |
| Unsigned Integer (16-bit) | `0x2B` |
| Unsigned Integer (32-bit) | `0x2C` |
| Unsigned Integer (64-bit) | `0x2D` |
| List | `0x3A` |
| Unstructured Object (Dictionary) | `0x3B` |
| Structured Object | `0x3C` |
| Structured Object Type Declaration | `0x3D` |
| Object Scope Boundary | `0x3E` |
| Vector | `0x4A` |
| Matrix | `0x4B` |  

More details about complex object types will be provided in later sections.  

The object scope boundary `TypeTag` does not function as a traditional `Value`; it serves to delimit object boundaries, has no header or body, and may only appear in certain locations. More information will be provided in section 7.

## 3. Containers
In order to support storing a Jaguar stream on disk in an identifiable format, the stream can be wrapped in a Jaguar container.  

Unlike a Jaguar stream, Jaguar containers cannot be directly concatenated and still be valid, since the container is essentially a header followed by stream data. Since the header is not valid Jaguar bytes, this would cause the stream to become invalid when the header is encountered.  

The container header takes the following form:
| Field | Size (bytes) |
| ----- | ---- |
| Magic data | 6 |
| File intent byte | 1 |
| Null separator for alignment | 1 |
| Integrity hash | 16 |

The magic data string is `JAGUAR` in ASCII bytes (or `4A 41 47 55 41 52` in hex bytes).  

The file intent byte is application-defined and is used to identify what the stream is supposed to be to the application. Decoders **must not** rely on this byte to determine how to parse the stream, as this value has no formal definition. The only exception is that a null byte here (`00`) is reserved to mean a freeform stream (i.e., have no expectations for what you get). This is primarily for higher-level consumers of parsed Jaguar data as opposed to the decoder itself.  

The integrity hash is a system meant to ensure that the data stream has not been corrupted. It is an MD5 hash computed from the contents of the stream. Decoders are advised to incrementally read the stream and generate the hash before rewinding and parsing, but they may implement this however they wish. As Jaguar streams may be large, an incremental approach to hash generation is recommended.  

## 4. Numerical Types
Numerical types are very simple in Jaguar. They are the signed and unsigned integers, floating-point numbers, and booleans (while not technically numbers, they are stored as `0` for false and `1` for true; any other value is **invalid**). All necessary information to read the data is contained in the `TypeTag`.  

As such, they do not have a header _per se_, and the body size is negligible enough that decoders are advised to immediately parse them as opposed to delaying parsing as may be done for more complex types.

## 5. Lists
Lists are a fairly simple construct in Jaguar. The header for a list `Value` is as follows:
| Field | Size (bytes) |
| ----- | ------------ |
| Element `TypeTag` | 1 |
| Element Count (unsigned int) | 4 |

The body is then the list elements in sequential order, with zero-based indexing.  

All elements in the list **must** be of the type specified by the element `TypeTag` field, and there **must** be exactly the number of elements specified by the element count field.  

Elements within a list do not have the typical `TypeTag` followed by the name string prefix before their headers, as this is not needed to identify them.  

Do note that a list overrun could be misinterpreted and break the decoder, especially if the bytes in the list overlap with real `TypeTag`s. For this reason, decoders are advised to always read and validate in its entirety the next presumed `Value` header after a list to ensure that this is not the case.  

## 6. Math Types
Vector and matrix types are built into Jaguar. There are limitations on what data these math types may contain:  

* **ONLY** floating-point numbers and (un)signed integers are permitted. 

* Vectors may have a size in the range of 2-4. 

* Matrices may have a size in the range of 2x2-4x4

These limitations are enforced to ensure easier parsing behavior and provide separation between vectors and lists. Any violation of these rules means the `Value` is **invalid**. Vectors are very similar to lists; their primary difference is the presence of these limitations. The main benefit comes from context; the decoder is able to provide a more intuitive access interface for vectors to consuming applications.  

Vector headers follow the below format:
| Field | Size (bytes) |
| ----- | ------------ |
| Element `TypeTag` | 1 |
| Element Count (unsigned int) | 1 |

In cases of invalid vectors or matrices, decoders **may** either declare the stream invalid and terminate decoding **or** continue decoding, ignoring the broken value. It is **illegal** for the decoder to allow an invalid vector or matrix to be presented to the consuming application.  

Matrices are stored in column-major format, like with OpenGL. This means that the matrix below:
```
0 1 2 3
4 5 6 7
8 9 0 1
2 3 4 5
```  
will be stored with all values in the first column sequentially, then the next, and so on.  

Matrix headers follow the below format:
| Field | Size (bytes) |
| ----- | ------------ |
| Element `TypeTag` | 1 |
| # of Columns (unsigned int) | 1 |
| # of Rows (unsigned int) | 1 |  

## 7. Objects
Objects allow for the subdivision of a Jaguar stream into multiple groups of fields. Objects are organized as a list with a defined number of key-value pairs. There are two primary types of objects:  

**Unstructured objects** (like dictionaries) provide a generic key-value mapping. Decoders **must not** attempt to interpret the structure of unstructured objects.  

**Structured objects** provide a type-consistent method of encoding and coordinating information together at a deeper level than the primary stream, using a system of stream-defined typenames to identify the appropriate structure. All structured objects with the same typename must have the same structure; a structured object that does not conform to the declared structure is **invalid**, and decoders must ignore it. Decoders **may** either declare the stream invalid and terminate decoding **or** continue without the invalid object in this case.

Unstructured object headers are fairly simple; they consist of a 16-bit unsigned integer field count.

Structured object headers consist of an 8-bit unsigned integer typename string length, followed by the typename string data of that length.  

### Structured Object Type Declarations
Before a structured object typename may be used, it must appear as part of a structured object type declaration. A structured object type declaration follows this header format:  
| Field | Size (bytes) |
| ----- | ------------ |
| Typename string size (unsigned int) | 1 |
| Typename string | (size above) |
| Field count | 2 |  

If a structured object type declaration is encountered with a typename that has already been declared, decoders **may** either declare the stream invalid and terminate decoding **or** continue until the end of the declaration and ignore it.

The body of a declaration is very similar to an unstructured object, with the exception that only value `TypeTag`s and names are kept. The one exception to this rule is that generic data types (lists, structured objects, vectors, and matrices) maintain their headers to ensure conformity. Lists do not have a defined size in a declaration; that is decided by the actual structured object.  

Structured object type declarations **may not** contain other type declarations.

### Object Body Data

If the typename string of a structured object references a typename that has not yet been declared, decoders **must** declare the stream invalid and terminate decoding, as there is no way to accurately know the bounds of the broken object scope (since it could contain subobjects, using the scope boundary system does not work here). However, decoders **must** allow for the consuming application to reposition the stream reader head and continue decoding, provided the new position is valid data.  

Within a structured object, fields do not need to appear in the exact order that they are declared in the type declaration, provided that all fields are given a value.  

The body structure is shared between object types. It is identical to the regular `Value` stream layout, with the exception that at the end of the scope, a scope boundary `Value` with the Object Scope Boundary `TypeTag` (`0x3E`) must be added. This type has no header or body, and serves only to delimit object boundaries. Decoders **must** ensure that they accurately track nesting depth using object headers and scope boundaries in order to ensure that the parsed layout is correct.  

Objects **may** be nested up to a maximum depth of 64. If the maximum nesting depth is reached, different rules apply depending on context. If the maximum nesting depth for objects is reached, decoders **may** either declare the stream invalid and terminate decoding **or** attempt recovery using the following rules:  

* If within a structured object type declaration, the entire declaration is invalid. Decoders **must** continue until back to the root level, ignoring all data. The typename is still reserved, and should it be referenced by a structured object, decoders **may** either declare the stream invalid and terminate decoding **or** ignore the broken object.  

* Otherwise, decoders **must** continue unwinding scopes, ignoring all data, until either at the root stream level or within an unstructured object. Any `Value`s passed through during the unwinding are invalid and **must** be ignored.

If an object does not contain the amount of fields specified (which may be detected using scope boundary `Values` if they are found earlier than expected or not present where they are expected), decoders **may** either declare the stream invalid and terminate decoding **or**, if the boundary object was found early attempt recovery, **may** attempt to recover following the same procedure as the maximum nesting depth reached.

## 8. Buffer Values
Buffer-type `Value`s are fairly straightforward. Their header consists of an unsigned integer size (32 bits for strings, 64 for byte buffers and substreams), and the body is simply the data.  

Per the rules from section 0, all strings must be encoded in UTF-8. This does not apply to data within byte buffers, of course.  

Byte buffers are a blob of raw bytes embedded in the Jaguar stream. They can contain whatever binary data you like. Decoders **must not** attempt to parse the contents of byte buffers themselves; this is reserved for the consuming application.

### Substreams
Substreams are a unique feature of Jaguar that allows one stream to exist within another in an independent manner. Though they may initially appear similar to objects, they differ in a number of ways:  

1. Objects require metadata about how many fields they contain, whereas substreams do not.

2. There is no equivalent of structured objects for substreams.

3. The contents of substreams are not considered a part of the value tree of the containing stream; that is, they must be accessed separately.

4. Skipping an object during parsing requires traversing the tree and skipping each field individually, as there is no set size. By contrast, substreams are embedded identically to byte buffers, allowing them to be skipped in O(1) time.

It is advised that decoders do not automatically parse substreams during the parsing of the main stream; they should wait until the substream is requested. However, this is not a hard requirement.  

Substreams **may not** contain other substreams.  

If, during the parsing of a substream, a decoder makes the determination that the substream is invalid and decoding should be terminated, this **must not** affect the containing stream, and that stream must continue to remain valid for parsing **unless it is also invalid for a separate reason**.
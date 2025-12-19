# The Jaguar Specification

This is the definitive specification for the Jaguar stream format. This is mainly for people looking to more deeply understand the format itself, or those wishing to write their own encoder/decoder (we recommend taking a look at `libjaguar`, the reference library, to understand how this might be accomplished).  

All conforming implementations must follow this specification. Please note: Jaguar is not designed to be an extensible format; as such any custom extensions or features **will not** be described here.

```{note}
While this specification is designed to be fairly readable, what follows is still highly technical information.  

You should be familiar with the basics of streaming, hexadecimal, and matrix layouts before proceeding.
```

## 0. Before We Begin - A Few Simple Notes
Before the more complicated parts of the specification, here are a few small rules that are important but don't require a whole section themselves:

* Jaguar streams use little-endian byte order

* All strings are stored using Pascal format - a size in bytes followed by the actual string data, which has no terminator

* All strings are encoded using UTF-8

* Jaguar is byte-packed, but does not contain an explicit synchronization system. This is because Jaguar is not designed to be used in a lossy environment (such as networking)

* All floating-point values are encoded in accordance with the IEEE 754 LE standard

## 1. Values and Data
A Jaguar stream is fairly simple conceptually. It is simply a continued stream of multiple `Value`s.  

A `Value` consists of the following information:
| Field | Size (bytes) |
| ----- | ---- |
| `TypeTag` | 1 |
| Name string size | 2 |
| Name string | (size above) |
| Data | (size determined by `TypeTag`) |  

The data itself is determined by what type the value belongs to, identified by the `TypeTag`.  

All data is split into a header and a body. The header contains the data required to identify the size of and correctly interpret the body data (such as the element count for lists). This allows decoders to skip over the body data and continue reading headers to identify the structure of the stream without needing to parse the data immediately.  

## 2. Typing
Jaguar is a statically-typed format. All data must have a corresponding type. This is useful for decoding because it allows for easier validation of the intended structure and also helps with maintaining internal consistency.  

All `Value`s in a Jaguar stream have a `TypeTag`, a one-byte sequence which identifies the start of a new `Value` and determines its type.  

`TypeTag`s are grouped using their upper nibble (4 bits) by category, those categories being:
0. Buffers, floating-point numbers, and booleans
1. Signed integers
2. Unsigned integers
3. Containers
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
| Vector | `0x4A` |
| Matrix | `0x4B` |  

More details about complex object types will be provided in later sections.

## 3. Containers
In order to support storing a Jaguar stream on disk in an identifiable format, the stream can be wrapped in a Jaguar container.  

Unlike a Jaguar stream, Jaguar containers cannot be directly concatenated and still be valid, since the container is essentially a header followed by stream data. Since the header is not valid Jaguar bytes, this would cause the stream to become invalid when the header is encountered.  

The container header takes the following form:
| Field | Size (bytes) |
| ----- | ---- |
| Magic data | 6 |
| File intent byte | 1 |
| Null separator for alignment | 1 |

The magic data string is `JAGUAR` in ASCII bytes (or `4A 41 47 55 41 52` in hex bytes).  

The file intent byte is application-defined, and is used to identify what the stream is supposed to be to the application. Decoders **must not** rely on this byte to determine how to parse the stream, as this value has no formal definition. The only exception is that a null byte here (`00`) is reserved to mean a freeform stream (i.e. have no expectations for what you get). This is primarily for higher-level consumers of parsed Jaguar data as opposed to the decoder itself.

## 4. Buffer Values
Buffer-type `Value`s are fairly straightforward. Their header consists of an unsigned integer size (32 bits for strings, 64 for byte buffers and substreams), and the body is simply the data.  

Per the rules from section 0, all strings must be encoded in UTF-8. This does not apply to data within byte buffers, of course.  

Byte buffers are a blob of raw bytes embedded in the Jaguar stream. They can contain whatever binary data you like. Decoders **must not** attempt to parse the contents of byte buffers themselves; this is reserved for the consuming application.

#### Substreams
Substreams are a unique feature of Jaguar that allows one stream to exist within another in an independent manner. Though they may initially appear similar to objects, they differ in a number of ways:  

1. Objects require metadata about how many fields they contain, whereas substreams do not

2. There is no equivalent of structured objects for substreams

3. The contents of substreams are not considered a part of the value tree of the containing stream; that is, they must be accessed separately

4. Skipping an object during parsing requires traversing the tree and skipping each field individually, as there is no set size. By contrasts, substreams are embedded identically to byte buffers, allowing them to be skipped in O(1) time.

It is advised that decoders do not automatically parse substreams during parsing of the main stream; they should wait until the substream is requested. However, this is not a hard requirement.  

Substreams **may not** contain other substreams.

## 5. Lists
Lists are a fairly simple construct in Jaguar. The header for a list `Value` is as follows:
| Field | Size (bytes) |
| ----- | ------------ |
| Element `TypeTag` | 1 |
| Element Count (unsigned) | 4 |

The body is then the list elements in sequential order, with zero-based indexing.  

All elements in the list **must** be of the type specified by the element `TypeTag` field, and there **must** be exactly the amount of elements specified by the element count field.  

Elements within a list do not have the typical `TypeTag` followed by name string prefix before their headers, as this is not needed to identify them.  

Do note that a list overrun could be misinterpreted and break the decoder, especially if the bytes in the list overlap with real `TypeTag`s. For this reason, decoders are advised to always read and validate in its entirety the next presumed `Value` header after a list to ensure that this is not the case.
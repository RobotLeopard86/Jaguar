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

More details about non-self-explanatory object types will be provided later.

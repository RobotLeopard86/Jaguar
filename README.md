# Jaguar
#### A stream-oriented binary data exchange format

## About
Jaguar is a stream-oriented binary data exchange format with an emphasis on simplicity and efficiency. Here, you can find the Jaguar spec, `libjaguar` stream encoding/decoding library, and `jaguartool` CLI tool for creating Jaguar streams via a text format.  

## Features
* Byte-packed encoding
* Object support
	* Unstructured objects (dictionaries)
	* Structured objects (provides type consistency at decoding)
* Static typing system
* Self-describing streams
* Built-in support for vectors and matrices (up to 4x4)
* Use of skipping and rewinding to support gleaning information quickly and obtaining further details later
* Arbitrary data buffer embedding
* Substreams - allows for embedding other streams to allow late parsing of data

## Building
You will need:  
* Git
* Meson
* Ninja  

Configure the build directory with `meson setup build --native-file native.ini`, then run `meson compile -C build` to build `libjaguar` and `jaguartool`. You do not have to use the native file (which sets the compiler to Clang and the linker to LLD), but it is recommended.

## Licensing
The Jaguar spec and supporting documents are provided and licensed under Creative Commons Attribution-ShareAlike 4.0 International. To view a copy of this license, visit [https://creativecommons.org/licenses/by-sa/4.0/](https://creativecommons.org/licenses/by-sa/4.0/).  
`libjaguar` and `jaguartool` are licensed under the Apache License 2.0, which can be found in the root directory.

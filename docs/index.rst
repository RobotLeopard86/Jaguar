Jaguar Documentation
==============================

Welcome to the Jaguar documentation!

Jaguar is a stream-oriented binary data exchange format with an emphasis on simplicity and efficiency. Some key features include:

* Byte-packed encoding
* Object support, with both unstructured dictionaries and type-consistent structured objects
* Static typing system
* Self-describing streams
* Built-in support for vectors and matrices (up to 4x4)
* Use of skipping and rewinding to support gleaning information quickly and obtaining further details later
* Arbitrary data buffer embedding
* Substreams - allows for embedding other streams to allow late parsing of data

.. toctree::
    :maxdepth: 1

    spec
    api/index
# Spidir WASM

This is a simple WASM32 runtime based on spidir jit, made as an example and benchmark of Spidir.

The repo can either be built as a binary or as a static library.

The API is fairly low-high, only giving the ability to load a module and then jit it. Any other management and linking
needs to be done by the user of the API.

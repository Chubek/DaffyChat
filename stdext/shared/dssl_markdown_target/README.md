# `dssl_markdown_target`

This directory contains a tiny shared-library example for the public DSSL plugin API.

Why it exists:

- DSSL claims that custom codegen targets can be provided by shared libraries.
- The real plugin loader/runtime wiring is still incomplete in-tree.
- Future agents still need an example of what a plugin is supposed to look like.

What this sample does:

- exposes `dssl_plugin_create` and `dssl_plugin_destroy`
- registers a target named `markdown_index`
- writes a very small markdown file describing which top-level DSSL callbacks fired

What it deliberately does *not* do:

- inspect internal C++ AST structures that are not part of the installed C ABI
- pretend that production-grade plugin loading is already complete
- claim compatibility guarantees that the current tree does not yet provide

Suggested future direction:

- stabilize a plugin-safe AST visitor C ABI
- add a real loader to `dssl-bindgen`
- teach the plugin to emit useful per-service docs or skeleton assets

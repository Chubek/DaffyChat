# DaffyChat Extensions

This directory contains extension manifests and WASM binaries for DaffyChat extensions.

## Extension Structure

Each extension consists of:
- A JSON manifest describing the extension
- A compiled WASM binary (`.wasm` file)

## Manifest Format

```json
{
  "id": "unique-extension-id",
  "name": "Human Readable Name",
  "version": "1.0.0",
  "description": "What this extension does",
  "author": "Extension Author",
  "wasmUrl": "/path/to/extension.wasm",
  "permissions": [
    "rooms.read",
    "messages.write"
  ],
  "hooks": [
    "room:participant-joined",
    "message:sent"
  ],
  "metadata": {
    "category": "utility",
    "tags": ["tag1", "tag2"]
  }
}
```

## Available Permissions

- `rooms.read` - Read room information
- `rooms.write` - Modify room settings
- `messages.read` - Read messages
- `messages.write` - Send messages
- `events.read` - Subscribe to room events
- `events.write` - Emit custom events
- `storage.read` - Read from local storage
- `storage.write` - Write to local storage
- `network.fetch` - Make HTTP requests

## Available Hooks

Extensions can register hooks for the following events:

- `room:created` - When a room is created
- `room:destroyed` - When a room is destroyed
- `room:participant-joined` - When a user joins
- `room:participant-left` - When a user leaves
- `message:sent` - When a message is sent
- `message:received` - When a message is received
- `message:before-send` - Before a message is sent (can modify)
- `voice:state-changed` - When voice state changes
- `extension:loaded` - When an extension is loaded
- `extension:error` - When an extension encounters an error

## Creating Extensions

Extensions are written in Daffyscript and compiled to WASM. See the main documentation for details on:

1. Writing Daffyscript code
2. Compiling to WASM
3. Testing extensions locally
4. Publishing to a catalog

## Example Extensions

- `example-greeter.json` - Simple greeting extension
- `catalog.json` - Example extension catalog

## Loading Extensions

Extensions can be loaded in the room UI:

1. Go to the Extensions tab
2. Enter a catalog URL or upload a manifest
3. Click "Load" on the desired extension
4. The extension will be instantiated and hooks will be registered

## Security

Extensions run in a sandboxed WASM environment with limited access to:
- Only the permissions they declare
- No direct DOM access
- No direct network access (except through approved APIs)
- Memory isolation from other extensions

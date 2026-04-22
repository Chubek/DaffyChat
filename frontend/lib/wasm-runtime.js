/**
 * DaffyChat WASM Runtime Loader
 * 
 * Loads and manages Daffyscript-compiled WASM extensions in the browser.
 * Provides lifecycle management, sandboxing, and bridge integration.
 */
(function (global) {
  'use strict';

  const registry = new Map();
  const loadedModules = new Map();
  let nextExtensionId = 1;

  const ExtensionState = {
    PENDING: 'pending',
    LOADING: 'loading',
    LOADED: 'loaded',
    RUNNING: 'running',
    ERROR: 'error',
    UNLOADED: 'unloaded'
  };

  /**
   * Extension metadata and runtime state
   */
  class Extension {
    constructor(config) {
      this.id = config.id || 'ext-' + nextExtensionId++;
      this.name = config.name || this.id;
      this.url = config.url;
      this.version = config.version || '1.0.0';
      this.state = ExtensionState.PENDING;
      this.module = null;
      this.instance = null;
      this.memory = null;
      this.exports = {};
      this.imports = {};
      this.error = null;
      this.loadedAt = null;
      this.metadata = config.metadata || {};
    }

    toJSON() {
      return {
        id: this.id,
        name: this.name,
        url: this.url,
        version: this.version,
        state: this.state,
        error: this.error,
        loadedAt: this.loadedAt,
        metadata: this.metadata,
        exports: Object.keys(this.exports)
      };
    }
  }

  /**
   * WASM import object factory - provides host functions to WASM modules
   */
  function createImportObject(extension) {
    return {
      env: {
        // Memory management
        memory: new WebAssembly.Memory({ initial: 256, maximum: 512 }),
        
        // Logging functions
        log: function(ptr, len) {
          const message = readString(extension.memory, ptr, len);
          console.log('[WASM:' + extension.name + ']', message);
          emitExtensionEvent(extension, 'log', { message });
        },
        
        error: function(ptr, len) {
          const message = readString(extension.memory, ptr, len);
          console.error('[WASM:' + extension.name + ']', message);
          emitExtensionEvent(extension, 'error', { message });
        },
        
        // Bridge integration
        emit_event: function(topicPtr, topicLen, payloadPtr, payloadLen) {
          const topic = readString(extension.memory, topicPtr, topicLen);
          const payload = readString(extension.memory, payloadPtr, payloadLen);
          
          try {
            const data = JSON.parse(payload);
            if (global.DaffyBridge) {
              global.DaffyBridge.emit(topic, data);
            }
          } catch (err) {
            console.error('[WASM:' + extension.name + '] Failed to emit event:', err);
          }
        },
        
        // Time functions
        now: function() {
          return Date.now();
        },
        
        // Math functions (commonly needed)
        sin: Math.sin,
        cos: Math.cos,
        tan: Math.tan,
        sqrt: Math.sqrt,
        pow: Math.pow,
        floor: Math.floor,
        ceil: Math.ceil,
        round: Math.round,
        abs: Math.abs
      },
      
      daffy: {
        // DaffyChat-specific APIs
        get_room_id: function(bufPtr, bufLen) {
          const roomId = getCurrentRoomId();
          return writeString(extension.memory, bufPtr, bufLen, roomId);
        },
        
        get_user_id: function(bufPtr, bufLen) {
          const userId = getCurrentUserId();
          return writeString(extension.memory, bufPtr, bufLen, userId);
        },
        
        send_message: function(textPtr, textLen) {
          const text = readString(extension.memory, textPtr, textLen);
          sendRoomMessage(text);
        },
        
        register_hook: function(namePtr, nameLen) {
          const hookName = readString(extension.memory, namePtr, nameLen);
          registerExtensionHook(extension, hookName);
        }
      }
    };
  }

  /**
   * Read a UTF-8 string from WASM memory
   */
  function readString(memory, ptr, len) {
    if (!memory || !memory.buffer) return '';
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder('utf-8').decode(bytes);
  }

  /**
   * Write a UTF-8 string to WASM memory
   */
  function writeString(memory, ptr, maxLen, str) {
    if (!memory || !memory.buffer) return 0;
    const encoded = new TextEncoder().encode(str);
    const len = Math.min(encoded.length, maxLen);
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    bytes.set(encoded.slice(0, len));
    return len;
  }

  /**
   * Get current room ID from global state
   */
  function getCurrentRoomId() {
    if (global.DaffyStore && global.DaffyStore.room) {
      const state = global.DaffyStore.room.getState();
      return state.roomId || '';
    }
    return '';
  }

  /**
   * Get current user ID from global state
   */
  function getCurrentUserId() {
    if (global.DaffyStore && global.DaffyStore.user) {
      const state = global.DaffyStore.user.getState();
      return state.userId || '';
    }
    return '';
  }

  /**
   * Send a message to the current room
   */
  function sendRoomMessage(text) {
    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:send-message', { text });
    }
  }

  /**
   * Register a hook for an extension
   */
  function registerExtensionHook(extension, hookName) {
    if (global.DaffyBridge) {
      global.DaffyBridge.registerHook(hookName, function(event) {
        if (extension.instance && extension.exports.on_event) {
          try {
            // Call the WASM module's event handler
            const eventJson = JSON.stringify(event);
            const ptr = allocateString(extension, eventJson);
            extension.exports.on_event(ptr, eventJson.length);
          } catch (err) {
            console.error('[WASM:' + extension.name + '] Hook error:', err);
          }
        }
      });
    }
  }

  /**
   * Allocate a string in WASM memory (simplified)
   */
  function allocateString(extension, str) {
    // This is a simplified version - real implementation would need proper memory management
    return 0;
  }

  /**
   * Emit an extension-specific event
   */
  function emitExtensionEvent(extension, eventType, data) {
    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:' + eventType, {
        extensionId: extension.id,
        extensionName: extension.name,
        ...data
      });
    }
  }

  /**
   * Load a WASM extension from a URL
   */
  async function loadExtension(config) {
    const extension = new Extension(config);
    registry.set(extension.id, extension);
    
    extension.state = ExtensionState.LOADING;
    emitExtensionEvent(extension, 'loading', {});

    try {
      // Fetch the WASM binary
      const response = await fetch(extension.url);
      if (!response.ok) {
        throw new Error('Failed to fetch WASM: ' + response.statusText);
      }

      const wasmBytes = await response.arrayBuffer();
      
      // Create import object
      const importObject = createImportObject(extension);
      extension.imports = importObject;
      extension.memory = importObject.env.memory;

      // Compile and instantiate
      const result = await WebAssembly.instantiate(wasmBytes, importObject);
      extension.module = result.module;
      extension.instance = result.instance;
      extension.exports = result.instance.exports;

      extension.state = ExtensionState.LOADED;
      extension.loadedAt = new Date().toISOString();
      loadedModules.set(extension.id, extension);

      emitExtensionEvent(extension, 'loaded', {});

      // Call initialization function if it exists
      if (extension.exports.init) {
        extension.exports.init();
        extension.state = ExtensionState.RUNNING;
        emitExtensionEvent(extension, 'running', {});
      }

      return extension;
    } catch (err) {
      extension.state = ExtensionState.ERROR;
      extension.error = err.message;
      emitExtensionEvent(extension, 'error', { error: err.message });
      throw err;
    }
  }

  /**
   * Unload an extension
   */
  function unloadExtension(extensionId) {
    const extension = registry.get(extensionId);
    if (!extension) {
      throw new Error('Extension not found: ' + extensionId);
    }

    // Call cleanup function if it exists
    if (extension.instance && extension.exports.cleanup) {
      try {
        extension.exports.cleanup();
      } catch (err) {
        console.error('[WASM:' + extension.name + '] Cleanup error:', err);
      }
    }

    extension.state = ExtensionState.UNLOADED;
    extension.instance = null;
    extension.module = null;
    extension.memory = null;
    
    loadedModules.delete(extensionId);
    emitExtensionEvent(extension, 'unloaded', {});

    return true;
  }

  /**
   * Get extension by ID
   */
  function getExtension(extensionId) {
    return registry.get(extensionId);
  }

  /**
   * List all extensions
   */
  function listExtensions() {
    return Array.from(registry.values()).map(ext => ext.toJSON());
  }

  /**
   * Call an exported function from an extension
   */
  function callExtensionFunction(extensionId, functionName, ...args) {
    const extension = loadedModules.get(extensionId);
    if (!extension) {
      throw new Error('Extension not loaded: ' + extensionId);
    }

    if (!extension.exports[functionName]) {
      throw new Error('Function not found: ' + functionName);
    }

    try {
      return extension.exports[functionName](...args);
    } catch (err) {
      console.error('[WASM:' + extension.name + '] Function call error:', err);
      throw err;
    }
  }

  /**
   * Check if WASM is supported
   */
  function isSupported() {
    return typeof WebAssembly !== 'undefined' && 
           typeof WebAssembly.instantiate === 'function';
  }

  /**
   * Get runtime statistics
   */
  function getStats() {
    return {
      supported: isSupported(),
      totalExtensions: registry.size,
      loadedExtensions: loadedModules.size,
      extensions: listExtensions()
    };
  }

  // Export public API
  global.DaffyWASM = {
    loadExtension,
    unloadExtension,
    getExtension,
    listExtensions,
    callExtensionFunction,
    isSupported,
    getStats,
    ExtensionState
  };

  // Emit bootstrap event
  if (global.DaffyBridge) {
    global.DaffyBridge.emit('wasm:bootstrap', { supported: isSupported() });
  }

})(window);

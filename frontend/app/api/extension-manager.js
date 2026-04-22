/**
 * DaffyChat Extension Manager
 * 
 * Handles extension discovery, validation, and lifecycle management.
 * Integrates with the WASM runtime and provides UI-friendly APIs.
 */
(function (global) {
  'use strict';

  const extensionCatalog = new Map();
  const extensionManifests = new Map();

  /**
   * Extension manifest schema
   */
  const ManifestSchema = {
    id: 'string',
    name: 'string',
    version: 'string',
    description: 'string',
    author: 'string',
    wasmUrl: 'string',
    permissions: 'array',
    hooks: 'array',
    metadata: 'object'
  };

  /**
   * Validate extension manifest
   */
  function validateManifest(manifest) {
    const errors = [];

    if (!manifest || typeof manifest !== 'object') {
      errors.push('Manifest must be an object');
      return { valid: false, errors };
    }

    // Required fields
    if (!manifest.id || typeof manifest.id !== 'string') {
      errors.push('Missing or invalid field: id');
    }
    if (!manifest.name || typeof manifest.name !== 'string') {
      errors.push('Missing or invalid field: name');
    }
    if (!manifest.version || typeof manifest.version !== 'string') {
      errors.push('Missing or invalid field: version');
    }
    if (!manifest.wasmUrl || typeof manifest.wasmUrl !== 'string') {
      errors.push('Missing or invalid field: wasmUrl');
    }

    // Optional but typed fields
    if (manifest.permissions && !Array.isArray(manifest.permissions)) {
      errors.push('Field permissions must be an array');
    }
    if (manifest.hooks && !Array.isArray(manifest.hooks)) {
      errors.push('Field hooks must be an array');
    }

    // Validate permissions
    if (manifest.permissions) {
      const validPermissions = [
        'rooms.read',
        'rooms.write',
        'messages.read',
        'messages.write',
        'events.read',
        'events.write',
        'storage.read',
        'storage.write',
        'network.fetch'
      ];
      
      for (const perm of manifest.permissions) {
        if (!validPermissions.includes(perm)) {
          errors.push('Unknown permission: ' + perm);
        }
      }
    }

    return {
      valid: errors.length === 0,
      errors
    };
  }

  /**
   * Register an extension manifest
   */
  function registerExtension(manifest) {
    const validation = validateManifest(manifest);
    if (!validation.valid) {
      throw new Error('Invalid manifest: ' + validation.errors.join(', '));
    }

    if (extensionManifests.has(manifest.id)) {
      throw new Error('Extension already registered: ' + manifest.id);
    }

    extensionManifests.set(manifest.id, manifest);
    extensionCatalog.set(manifest.id, {
      manifest,
      loaded: false,
      enabled: false,
      error: null
    });

    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:registered', { id: manifest.id, name: manifest.name });
    }

    return manifest.id;
  }

  /**
   * Discover extensions from a catalog URL
   */
  async function discoverExtensions(catalogUrl) {
    try {
      const response = await fetch(catalogUrl);
      if (!response.ok) {
        throw new Error('Failed to fetch catalog: ' + response.statusText);
      }

      const catalog = await response.json();
      if (!Array.isArray(catalog.extensions)) {
        throw new Error('Invalid catalog format');
      }

      const discovered = [];
      for (const manifest of catalog.extensions) {
        try {
          registerExtension(manifest);
          discovered.push(manifest.id);
        } catch (err) {
          console.error('[ExtensionManager] Failed to register:', manifest.id, err);
        }
      }

      return discovered;
    } catch (err) {
      console.error('[ExtensionManager] Discovery failed:', err);
      throw err;
    }
  }

  /**
   * Load an extension by ID
   */
  async function loadExtension(extensionId) {
    const entry = extensionCatalog.get(extensionId);
    if (!entry) {
      throw new Error('Extension not found: ' + extensionId);
    }

    if (entry.loaded) {
      return entry;
    }

    if (!global.DaffyWASM) {
      throw new Error('WASM runtime not available');
    }

    try {
      const manifest = entry.manifest;
      
      // Check permissions
      if (manifest.permissions && manifest.permissions.length > 0) {
        const granted = await requestPermissions(extensionId, manifest.permissions);
        if (!granted) {
          throw new Error('Permissions denied');
        }
      }

      // Load WASM module
      const extension = await global.DaffyWASM.loadExtension({
        id: manifest.id,
        name: manifest.name,
        url: manifest.wasmUrl,
        version: manifest.version,
        metadata: manifest.metadata || {}
      });

      entry.loaded = true;
      entry.enabled = true;
      entry.error = null;

      if (global.DaffyBridge) {
        global.DaffyBridge.emit('extension:loaded', {
          id: manifest.id,
          name: manifest.name,
          version: manifest.version
        });
      }

      return entry;
    } catch (err) {
      entry.error = err.message;
      if (global.DaffyBridge) {
        global.DaffyBridge.emit('extension:error', {
          id: extensionId,
          error: err.message
        });
      }
      throw err;
    }
  }

  /**
   * Unload an extension by ID
   */
  function unloadExtension(extensionId) {
    const entry = extensionCatalog.get(extensionId);
    if (!entry) {
      throw new Error('Extension not found: ' + extensionId);
    }

    if (!entry.loaded) {
      return true;
    }

    if (global.DaffyWASM) {
      global.DaffyWASM.unloadExtension(extensionId);
    }

    entry.loaded = false;
    entry.enabled = false;

    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:unloaded', { id: extensionId });
    }

    return true;
  }

  /**
   * Request permissions from user
   */
  async function requestPermissions(extensionId, permissions) {
    // In a real implementation, this would show a UI dialog
    // For now, we'll auto-grant for development
    console.log('[ExtensionManager] Requesting permissions for', extensionId, permissions);
    return true;
  }

  /**
   * Get extension info
   */
  function getExtension(extensionId) {
    const entry = extensionCatalog.get(extensionId);
    if (!entry) {
      return null;
    }

    return {
      id: entry.manifest.id,
      name: entry.manifest.name,
      version: entry.manifest.version,
      description: entry.manifest.description,
      author: entry.manifest.author,
      loaded: entry.loaded,
      enabled: entry.enabled,
      error: entry.error,
      permissions: entry.manifest.permissions || [],
      hooks: entry.manifest.hooks || []
    };
  }

  /**
   * List all extensions
   */
  function listExtensions(filter) {
    const extensions = [];
    
    for (const [id, entry] of extensionCatalog) {
      if (filter === 'loaded' && !entry.loaded) continue;
      if (filter === 'enabled' && !entry.enabled) continue;
      
      extensions.push(getExtension(id));
    }

    return extensions;
  }

  /**
   * Enable an extension
   */
  function enableExtension(extensionId) {
    const entry = extensionCatalog.get(extensionId);
    if (!entry) {
      throw new Error('Extension not found: ' + extensionId);
    }

    if (!entry.loaded) {
      throw new Error('Extension must be loaded first');
    }

    entry.enabled = true;
    
    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:enabled', { id: extensionId });
    }

    return true;
  }

  /**
   * Disable an extension
   */
  function disableExtension(extensionId) {
    const entry = extensionCatalog.get(extensionId);
    if (!entry) {
      throw new Error('Extension not found: ' + extensionId);
    }

    entry.enabled = false;
    
    if (global.DaffyBridge) {
      global.DaffyBridge.emit('extension:disabled', { id: extensionId });
    }

    return true;
  }

  /**
   * Get manager statistics
   */
  function getStats() {
    const stats = {
      total: extensionCatalog.size,
      loaded: 0,
      enabled: 0,
      errors: 0
    };

    for (const entry of extensionCatalog.values()) {
      if (entry.loaded) stats.loaded++;
      if (entry.enabled) stats.enabled++;
      if (entry.error) stats.errors++;
    }

    return stats;
  }

  /**
   * Load extensions from localStorage
   */
  function loadFromStorage() {
    try {
      const stored = localStorage.getItem('daffy-extensions');
      if (!stored) return;

      const data = JSON.parse(stored);
      if (Array.isArray(data.manifests)) {
        for (const manifest of data.manifests) {
          try {
            registerExtension(manifest);
          } catch (err) {
            console.error('[ExtensionManager] Failed to restore:', manifest.id, err);
          }
        }
      }
    } catch (err) {
      console.error('[ExtensionManager] Failed to load from storage:', err);
    }
  }

  /**
   * Save extensions to localStorage
   */
  function saveToStorage() {
    try {
      const manifests = Array.from(extensionManifests.values());
      localStorage.setItem('daffy-extensions', JSON.stringify({ manifests }));
    } catch (err) {
      console.error('[ExtensionManager] Failed to save to storage:', err);
    }
  }

  // Auto-load from storage on init
  loadFromStorage();

  // Export public API
  global.DaffyExtensions = {
    registerExtension,
    discoverExtensions,
    loadExtension,
    unloadExtension,
    enableExtension,
    disableExtension,
    getExtension,
    listExtensions,
    getStats,
    saveToStorage
  };

  // Emit bootstrap event
  if (global.DaffyBridge) {
    global.DaffyBridge.emit('extensions:bootstrap', getStats());
  }

})(window);

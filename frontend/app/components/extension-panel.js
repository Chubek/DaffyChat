/**
 * DaffyChat Extension Panel Component
 * 
 * UI component for managing extensions in a room.
 * Provides extension loading, unloading, and status display.
 */
(function (global) {
  'use strict';

  /**
   * Create extension panel Alpine.js component
   */
  function extensionPanel() {
    return {
      extensions: [],
      loading: false,
      error: null,
      catalogUrl: '',
      uploadedFile: null,

      init() {
        this.refreshExtensions();
        
        // Listen for extension events
        if (global.DaffyBridge) {
          global.DaffyBridge.registerHook('extension:loaded', () => this.refreshExtensions());
          global.DaffyBridge.registerHook('extension:unloaded', () => this.refreshExtensions());
          global.DaffyBridge.registerHook('extension:error', (event) => {
            this.error = event.payload.error;
          });
        }
      },

      refreshExtensions() {
        if (global.DaffyExtensions) {
          this.extensions = global.DaffyExtensions.listExtensions();
        }
      },

      async loadExtension(extensionId) {
        this.loading = true;
        this.error = null;

        try {
          await global.DaffyExtensions.loadExtension(extensionId);
          this.refreshExtensions();
        } catch (err) {
          this.error = 'Failed to load extension: ' + err.message;
          console.error('[ExtensionPanel]', err);
        } finally {
          this.loading = false;
        }
      },

      async unloadExtension(extensionId) {
        this.loading = true;
        this.error = null;

        try {
          global.DaffyExtensions.unloadExtension(extensionId);
          this.refreshExtensions();
        } catch (err) {
          this.error = 'Failed to unload extension: ' + err.message;
          console.error('[ExtensionPanel]', err);
        } finally {
          this.loading = false;
        }
      },

      async discoverFromCatalog() {
        if (!this.catalogUrl) {
          this.error = 'Please enter a catalog URL';
          return;
        }

        this.loading = true;
        this.error = null;

        try {
          const discovered = await global.DaffyExtensions.discoverExtensions(this.catalogUrl);
          this.refreshExtensions();
          this.catalogUrl = '';
          
          if (global.DaffyBridge) {
            global.DaffyBridge.emit('extension:discovered', { count: discovered.length });
          }
        } catch (err) {
          this.error = 'Failed to discover extensions: ' + err.message;
          console.error('[ExtensionPanel]', err);
        } finally {
          this.loading = false;
        }
      },

      async uploadExtension(event) {
        const file = event.target.files[0];
        if (!file) return;

        this.loading = true;
        this.error = null;

        try {
          // Read manifest from file
          const text = await file.text();
          const manifest = JSON.parse(text);

          // Register extension
          global.DaffyExtensions.registerExtension(manifest);
          global.DaffyExtensions.saveToStorage();
          
          this.refreshExtensions();
          event.target.value = '';
        } catch (err) {
          this.error = 'Failed to upload extension: ' + err.message;
          console.error('[ExtensionPanel]', err);
        } finally {
          this.loading = false;
        }
      },

      getExtensionStatus(ext) {
        if (ext.error) return 'error';
        if (ext.loaded && ext.enabled) return 'running';
        if (ext.loaded) return 'loaded';
        return 'available';
      },

      getExtensionStatusClass(ext) {
        const status = this.getExtensionStatus(ext);
        return 'daffy-extension-status-' + status;
      },

      getExtensionStatusLabel(ext) {
        const status = this.getExtensionStatus(ext);
        const labels = {
          error: '⚠️ Error',
          running: '✓ Running',
          loaded: '○ Loaded',
          available: '○ Available'
        };
        return labels[status] || status;
      },

      formatPermissions(permissions) {
        if (!permissions || permissions.length === 0) {
          return 'None';
        }
        return permissions.join(', ');
      },

      getWasmStats() {
        if (global.DaffyWASM) {
          return global.DaffyWASM.getStats();
        }
        return { supported: false, totalExtensions: 0, loadedExtensions: 0 };
      },

      isWasmSupported() {
        return global.DaffyWASM && global.DaffyWASM.isSupported();
      }
    };
  }

  // Export component factory
  global.DaffyComponents = global.DaffyComponents || {};
  global.DaffyComponents.extensionPanel = extensionPanel;

})(window);

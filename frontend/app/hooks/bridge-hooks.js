(function (global) {
  'use strict';

  function installDefaultHooks() {
    if (!global.DaffyBridge) return;

    global.DaffyBridge.registerHook('room:created', function (event) {
      console.log('[DaffyHook] Room created:', event.payload);
    });

    global.DaffyBridge.registerHook('room:destroyed', function (event) {
      console.log('[DaffyHook] Room destroyed:', event.payload);
    });

    global.DaffyBridge.registerHook('voice:state-changed', function (event) {
      console.log('[DaffyHook] Voice state changed:', event.payload);
      if (global.DaffyStore && event.payload) {
        global.DaffyStore.room.setState({
          voiceTransport: event.payload.transport || global.DaffyStore.room.getState().voiceTransport
        });
      }
    });

    global.DaffyBridge.registerHook('extension:loaded', function (event) {
      console.log('[DaffyHook] Extension loaded:', event.payload);
    });

    global.DaffyBridge.registerHook('extension:error', function (event) {
      console.error('[DaffyHook] Extension error:', event.payload);
    });
  }

  global.DaffyHooks = {
    installDefaultHooks: installDefaultHooks
  };
})(window);

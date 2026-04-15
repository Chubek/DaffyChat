(function (global) {
  'use strict';

  function createStore(initial) {
    var state = Object.assign({}, initial);
    var listeners = [];

    function getState() {
      return Object.assign({}, state);
    }

    function setState(partial) {
      Object.assign(state, partial);
      for (var i = 0; i < listeners.length; i++) {
        try { listeners[i](getState()); } catch (e) { console.error('[Store]', e); }
      }
    }

    function subscribe(fn) {
      listeners.push(fn);
      return function unsubscribe() {
        listeners = listeners.filter(function (l) { return l !== fn; });
      };
    }

    return { getState: getState, setState: setState, subscribe: subscribe };
  }

  var roomStore = createStore({
    roomId: '',
    peerId: '',
    participants: [],
    messages: [],
    muted: false,
    deafened: false,
    voiceTransport: 'native-client-only',
    signalingState: 'closed',
    iceState: 'new',
    lastError: ''
  });

  global.DaffyStore = {
    createStore: createStore,
    room: roomStore
  };
})(window);

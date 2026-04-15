(function (global) {
  'use strict';

  function fetchHealth(baseUrl) {
    return fetch(baseUrl + '/healthz')
      .then(function (res) { return res.json(); })
      .catch(function () { return { status: 'unreachable' }; });
  }

  function fetchRooms(baseUrl) {
    return fetch(baseUrl + '/debug/rooms')
      .then(function (res) { return res.json(); })
      .catch(function () { return { rooms: [] }; });
  }

  function fetchTurnCredentials(baseUrl, room, peerId) {
    var url = baseUrl + '/debug/turn-credentials?room=' + encodeURIComponent(room) + '&peer_id=' + encodeURIComponent(peerId);
    return fetch(url)
      .then(function (res) { return res.json(); })
      .catch(function () { return null; });
  }

  global.DaffyAPI = {
    signaling: {
      fetchHealth: fetchHealth,
      fetchRooms: fetchRooms,
      fetchTurnCredentials: fetchTurnCredentials
    }
  };
})(window);

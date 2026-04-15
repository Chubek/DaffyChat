const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function createLocalStorage(seed) {
  const data = new Map(Object.entries(seed || {}));
  return {
    getItem(key) {
      return data.has(key) ? data.get(key) : null;
    },
    setItem(key, value) {
      data.set(key, String(value));
    },
    removeItem(key) {
      data.delete(key);
    },
    dump() {
      return Object.fromEntries(data.entries());
    }
  };
}

function loadBridge(search, storageSeed) {
  const localStorage = createLocalStorage(storageSeed);
  const context = {
    window: null,
    location: { search: search || '' },
    localStorage,
    URLSearchParams,
    Map,
    Array,
    Date,
    JSON,
    console,
    WebSocket: function WebSocketStub() {}
  };
  context.window = context;

  const source = fs.readFileSync(path.join(__dirname, '..', '..', 'frontend', 'bridge.js'), 'utf8');
  vm.runInNewContext(source, context, { filename: 'frontend/bridge.js' });

  return {
    bridge: context.DaffyBridge,
    localStorage
  };
}

function snapshotConfig(config) {
  return JSON.parse(JSON.stringify(config));
}

function testDefaults() {
  const { bridge } = loadBridge('', {});
  assert.deepStrictEqual(snapshotConfig(bridge.getBootstrapConfig()), {
    signalingAdminUrl: '',
    voiceDiagnosticsUrl: 'http://127.0.0.1:7000/bridge/events'
  });

  const eventLog = bridge.getEventLog();
  assert.ok(eventLog.length > 0);
  assert.strictEqual(eventLog[0].name, 'bridge:bootstrap');
  assert.deepStrictEqual(snapshotConfig(eventLog[0].payload), snapshotConfig(bridge.getBootstrapConfig()));
}

function testLocalStorageFallback() {
  const { bridge } = loadBridge('', {
    'daffy-signaling-admin-url': 'http://127.0.0.1:7001',
    'daffy-voice-admin-url': 'http://127.0.0.1:7000/bridge/events?cached=1'
  });

  assert.deepStrictEqual(snapshotConfig(bridge.getBootstrapConfig()), {
    signalingAdminUrl: 'http://127.0.0.1:7001',
    voiceDiagnosticsUrl: 'http://127.0.0.1:7000/bridge/events?cached=1'
  });
}

function testQueryWinsOverLocalStorage() {
  const { bridge } = loadBridge(
    '?signaling_admin=http%3A%2F%2Fquery.example%3A7001&voice_admin=http%3A%2F%2Fquery.example%3A7000%2Fbridge%2Fevents',
    {
      'daffy-signaling-admin-url': 'http://127.0.0.1:7001',
      'daffy-voice-admin-url': 'http://127.0.0.1:7000/bridge/events?cached=1'
    }
  );

  assert.deepStrictEqual(snapshotConfig(bridge.getBootstrapConfig()), {
    signalingAdminUrl: 'http://query.example:7001',
    voiceDiagnosticsUrl: 'http://query.example:7000/bridge/events'
  });
}

function testPersistStoresAndClearsOverrides() {
  const { bridge, localStorage } = loadBridge('', {});
  let resolved = bridge.persistBootstrapConfig({
    signalingAdminUrl: 'http://saved.example:7001',
    voiceDiagnosticsUrl: 'http://saved.example:7000/bridge/events'
  });
  assert.deepStrictEqual(snapshotConfig(resolved), {
    signalingAdminUrl: 'http://saved.example:7001',
    voiceDiagnosticsUrl: 'http://saved.example:7000/bridge/events'
  });
  assert.deepStrictEqual(localStorage.dump(), {
    'daffy-signaling-admin-url': 'http://saved.example:7001',
    'daffy-voice-admin-url': 'http://saved.example:7000/bridge/events'
  });

  resolved = bridge.persistBootstrapConfig({
    signalingAdminUrl: '',
    voiceDiagnosticsUrl: ''
  });
  assert.deepStrictEqual(snapshotConfig(resolved), {
    signalingAdminUrl: '',
    voiceDiagnosticsUrl: 'http://127.0.0.1:7000/bridge/events'
  });
  assert.deepStrictEqual(localStorage.dump(), {});
}

testDefaults();
testLocalStorageFallback();
testQueryWinsOverLocalStorage();
testPersistStoresAndClearsOverrides();

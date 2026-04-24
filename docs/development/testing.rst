Testing
=======

Guide to testing DaffyChat.

Test Suite
----------

DaffyChat includes comprehensive tests:

* **Unit tests** - Test individual components
* **Integration tests** - Test service interactions
* **End-to-end tests** - Test complete workflows

Running Tests
-------------

All Tests
~~~~~~~~~

.. code-block:: bash

   cd build
   ctest --output-on-failure

Specific Tests
~~~~~~~~~~~~~~

.. code-block:: bash

   # Run specific test
   ctest -R test_echo_service

   # Run tests matching pattern
   ctest -R "test_.*_service"

Verbose Output
~~~~~~~~~~~~~~

.. code-block:: bash

   ctest -V

Parallel Execution
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ctest -j$(nproc)

Unit Tests
----------

Location: ``tests/unit/``

Run unit tests:

.. code-block:: bash

   ./build/tests/unit/unit_tests

Example unit test:

.. code-block:: cpp

   TEST(EchoService, Echo) {
       EchoServiceImpl service;
       auto reply = service.Echo("hello", "alice");
       
       EXPECT_EQ(reply.message, "hello");
       EXPECT_EQ(reply.sender, "alice");
       EXPECT_TRUE(reply.echoed);
   }

Integration Tests
-----------------

Location: ``tests/integration/``

Run integration tests:

.. code-block:: bash

   ./build/tests/integration/integration_tests

Example integration test:

.. code-block:: cpp

   TEST(RoomLifecycle, CreateAndJoin) {
       // Create room
       auto room = create_room("test-room");
       ASSERT_NE(room, nullptr);
       
       // Join room
       auto session = join_room(room->id, "alice");
       ASSERT_NE(session, nullptr);
       
       // Verify participant
       auto participants = list_participants(room->id);
       EXPECT_EQ(participants.size(), 1);
       EXPECT_EQ(participants[0].user_id, "alice");
   }

End-to-End Tests
----------------

Location: ``tests/e2e/``

Run E2E tests:

.. code-block:: bash

   ./build/tests/e2e/e2e_tests

Example E2E test:

.. code-block:: cpp

   TEST(VoiceE2E, TwoParticipants) {
       // Start signaling server
       auto signaling = start_signaling_server();
       
       // Create two clients
       auto alice = create_voice_client("alice");
       auto bob = create_voice_client("bob");
       
       // Join room
       alice->join("test-room");
       bob->join("test-room");
       
       // Wait for connection
       ASSERT_TRUE(wait_for_connection(alice, bob, 5s));
       
       // Verify audio flow
       EXPECT_TRUE(alice->is_receiving_audio());
       EXPECT_TRUE(bob->is_receiving_audio());
   }

Test Fixtures
-------------

Common test fixtures in ``tests/fixtures/``:

.. code-block:: cpp

   class ServiceTestFixture : public ::testing::Test {
   protected:
       void SetUp() override {
           // Start daemon manager
           daemon_manager_ = std::make_unique<DaemonManager>();
           daemon_manager_->start();
           
           // Start test services
           daemon_manager_->start_service("echo");
       }
       
       void TearDown() override {
           daemon_manager_->stop();
       }
       
       std::unique_ptr<DaemonManager> daemon_manager_;
   };

Mocking
-------

Use Google Mock for mocking:

.. code-block:: cpp

   class MockTransport : public ITransport {
   public:
       MOCK_METHOD(bool, connect, (const std::string&), (override));
       MOCK_METHOD(std::string, send, (const std::string&), (override));
   };

   TEST(ServiceClient, SendRequest) {
       MockTransport transport;
       EXPECT_CALL(transport, send(_))
           .WillOnce(Return("{\"result\": \"ok\"}"));
       
       ServiceClient client(&transport);
       auto result = client.send_request("test");
       EXPECT_EQ(result, "ok");
   }

Test Coverage
-------------

Generate coverage report:

.. code-block:: bash

   # Build with coverage
   cmake .. -DENABLE_COVERAGE=ON
   cmake --build .
   
   # Run tests
   ctest
   
   # Generate report
   gcovr -r .. --html --html-details -o coverage.html
   
   # View report
   firefox coverage.html

Target coverage: 80%+

Performance Tests
-----------------

Benchmark tests in ``tests/benchmarks/``:

.. code-block:: cpp

   static void BM_OpusEncode(benchmark::State& state) {
       OpusCodec codec;
       std::vector<float> samples(480);
       
       for (auto _ : state) {
           codec.encode(samples.data(), samples.size());
       }
   }
   BENCHMARK(BM_OpusEncode);

Run benchmarks:

.. code-block:: bash

   ./build/tests/benchmarks/benchmarks

Stress Tests
------------

Load testing with multiple clients:

.. code-block:: bash

   # 100 concurrent clients
   ./build/tests/stress/stress_test --clients 100 --duration 60

Memory Tests
------------

Valgrind
~~~~~~~~

.. code-block:: bash

   valgrind --leak-check=full \
            --show-leak-kinds=all \
            ./build/daffychat

AddressSanitizer
~~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DENABLE_ASAN=ON
   cmake --build .
   ./build/daffychat

ThreadSanitizer
~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DENABLE_TSAN=ON
   cmake --build .
   ./build/daffychat

Continuous Integration
----------------------

GitHub Actions
~~~~~~~~~~~~~~

``.github/workflows/ci.yml``:

.. code-block:: yaml

   name: CI
   
   on: [push, pull_request]
   
   jobs:
     build:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v2
           with:
             submodules: recursive
         
         - name: Install dependencies
           run: |
             sudo apt-get update
             sudo apt-get install -y build-essential cmake
         
         - name: Build
           run: |
             mkdir build && cd build
             cmake ..
             cmake --build .
         
         - name: Test
           run: |
             cd build
             ctest --output-on-failure

Test Data
---------

Golden files in ``tests/golden/``:

.. code-block:: cpp

   TEST(Parser, ParseDSSL) {
       auto spec = parse_dssl_file("tests/golden/echo.dssl");
       ASSERT_NE(spec, nullptr);
       EXPECT_EQ(spec->service_name, "echo");
       EXPECT_EQ(spec->version, "1.0.0");
   }

Test Utilities
--------------

Helper functions in ``tests/util/``:

.. code-block:: cpp

   // Create test room
   RoomPtr create_test_room(const std::string& name);
   
   // Wait for condition
   bool wait_for(std::function<bool()> condition, 
                 std::chrono::seconds timeout);
   
   // Generate test data
   std::vector<float> generate_sine_wave(size_t samples, float freq);

Manual Testing
--------------

Test Harnesses
~~~~~~~~~~~~~~

Voice loopback test:

.. code-block:: bash

   ./build/voice-loopback-test

Echo test:

.. code-block:: bash

   ./build/voice-echo-test --room test-room

Interactive Testing
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Start services
   ./build/daffydmd start
   
   # Start frontend
   cd frontend && npm run dev
   
   # Open browser
   firefox http://localhost:3000

Test Checklist
--------------

Before Release
~~~~~~~~~~~~~~

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] All E2E tests pass
- [ ] Code coverage > 80%
- [ ] No memory leaks (Valgrind)
- [ ] No data races (ThreadSanitizer)
- [ ] Performance benchmarks meet targets
- [ ] Manual testing completed
- [ ] Documentation updated

See Also
--------

* :doc:`building` - Building from source
* :doc:`contributing` - Contributing guide

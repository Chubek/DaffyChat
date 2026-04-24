Contributing
============

Thank you for your interest in contributing to DaffyChat!

Getting Started
---------------

1. Fork the repository
2. Clone your fork
3. Create a feature branch
4. Make your changes
5. Submit a pull request

Development Setup
-----------------

.. code-block:: bash

   # Clone repository
   git clone https://github.com/yourusername/daffychat.git
   cd daffychat
   git submodule update --init --recursive

   # Create branch
   git checkout -b feature/my-feature

   # Build
   mkdir build && cd build
   cmake ..
   cmake --build .

   # Run tests
   ctest

Code Style
----------

C++
~~~

Follow Google C++ Style Guide with these modifications:

* Use 4 spaces for indentation
* Max line length: 100 characters
* Use ``snake_case`` for functions and variables
* Use ``PascalCase`` for classes and structs

Example:

.. code-block:: cpp

   namespace daffy {

   class EchoService {
   public:
       EchoReply echo(const std::string& message, 
                      const std::string& sender) {
           EchoReply reply;
           reply.message = message;
           reply.sender = sender;
           return reply;
       }
   };

   }  // namespace daffy

Python
~~~~~~

Follow PEP 8:

.. code-block:: python

   def generate_service(name: str, version: str) -> None:
       """Generate service from template.
       
       Args:
           name: Service name in snake_case
           version: Semantic version string
       """
       # Implementation

JavaScript/TypeScript
~~~~~~~~~~~~~~~~~~~~~

Follow Airbnb JavaScript Style Guide:

.. code-block:: javascript

   export function onMessage(event) {
       if (!event.text) {
           return null;
       }
       
       return {
           type: 'message',
           text: `Echo: ${event.text}`
       };
   }

Commit Messages
---------------

Format
~~~~~~

.. code-block:: text

   <type>(<scope>): <subject>

   <body>

   <footer>

Types
~~~~~

* ``feat`` - New feature
* ``fix`` - Bug fix
* ``docs`` - Documentation changes
* ``style`` - Code style changes
* ``refactor`` - Code refactoring
* ``test`` - Test changes
* ``chore`` - Build/tooling changes

Examples
~~~~~~~~

.. code-block:: text

   feat(dssl): add enum type support

   Implement enum types in DSSL parser and code generator.
   Enums are generated as C++ enum classes.

   Closes #123

.. code-block:: text

   fix(voice): resolve audio glitches in opus decoder

   Fix buffer underrun in opus decoder that caused audio glitches
   when network latency exceeded 100ms.

   Fixes #456

Pull Requests
-------------

Before Submitting
~~~~~~~~~~~~~~~~~

1. Ensure all tests pass
2. Add tests for new features
3. Update documentation
4. Run code formatter
5. Check for linting errors

PR Template
~~~~~~~~~~~

.. code-block:: markdown

   ## Description
   Brief description of changes

   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Breaking change
   - [ ] Documentation update

   ## Testing
   - [ ] Unit tests added/updated
   - [ ] Integration tests added/updated
   - [ ] Manual testing completed

   ## Checklist
   - [ ] Code follows style guidelines
   - [ ] Self-review completed
   - [ ] Documentation updated
   - [ ] No new warnings

Review Process
~~~~~~~~~~~~~~

1. Automated checks run (CI)
2. Code review by maintainers
3. Address feedback
4. Approval and merge

Testing
-------

All contributions must include tests:

.. code-block:: cpp

   TEST(NewFeature, BasicFunctionality) {
       // Arrange
       auto service = create_service();
       
       // Act
       auto result = service.new_feature();
       
       // Assert
       EXPECT_TRUE(result.success);
   }

See :doc:`testing` for detailed testing guide.

Documentation
-------------

Update documentation for:

* New features
* API changes
* Configuration options
* Breaking changes

Documentation is in ``docs/`` using reStructuredText:

.. code-block:: rst

   New Feature
   ===========

   Description of the new feature.

   Usage
   -----

   .. code-block:: cpp

      auto result = use_new_feature();

Code Review
-----------

As a Reviewer
~~~~~~~~~~~~~

* Be respectful and constructive
* Focus on code quality and correctness
* Suggest improvements, don't demand
* Approve when ready

As an Author
~~~~~~~~~~~~

* Respond to all comments
* Be open to feedback
* Make requested changes
* Ask for clarification if needed

Issue Reporting
---------------

Bug Reports
~~~~~~~~~~~

Include:

* DaffyChat version
* Operating system
* Steps to reproduce
* Expected behavior
* Actual behavior
* Logs/screenshots

Template:

.. code-block:: markdown

   **Version:** 1.0.0
   **OS:** Ubuntu 22.04

   **Steps to Reproduce:**
   1. Start service
   2. Join room
   3. Send message

   **Expected:** Message delivered
   **Actual:** Error: Connection timeout

   **Logs:**
   ```
   [ERROR] Failed to send message
   ```

Feature Requests
~~~~~~~~~~~~~~~~

Include:

* Use case
* Proposed solution
* Alternatives considered
* Additional context

Template:

.. code-block:: markdown

   **Use Case:**
   As a user, I want to mute participants so that I can moderate the room.

   **Proposed Solution:**
   Add a mute button in the participant list.

   **Alternatives:**
   - Kick participant
   - Temporary ban

Development Workflow
--------------------

1. Pick an issue or create one
2. Comment that you're working on it
3. Create a branch
4. Make changes
5. Write tests
6. Update docs
7. Submit PR
8. Address review feedback
9. Merge

Branch Naming
~~~~~~~~~~~~~

* ``feature/description`` - New features
* ``fix/description`` - Bug fixes
* ``docs/description`` - Documentation
* ``refactor/description`` - Refactoring

Release Process
---------------

Versioning
~~~~~~~~~~

Follow Semantic Versioning (semver):

* ``MAJOR.MINOR.PATCH``
* ``MAJOR`` - Breaking changes
* ``MINOR`` - New features (backward compatible)
* ``PATCH`` - Bug fixes

Changelog
~~~~~~~~~

Update ``CHANGELOG.md`` for each release:

.. code-block:: markdown

   ## [1.1.0] - 2025-01-15

   ### Added
   - New feature X
   - New feature Y

   ### Fixed
   - Bug fix A
   - Bug fix B

   ### Changed
   - Breaking change C

Community
---------

* GitHub Discussions: https://github.com/yourusername/daffychat/discussions
* Discord: https://discord.gg/daffychat
* Forum: https://forum.daffychat.io

Code of Conduct
---------------

Be respectful and inclusive:

* Use welcoming language
* Respect differing viewpoints
* Accept constructive criticism
* Focus on what's best for the community

License
-------

By contributing, you agree that your contributions will be licensed under the same license as the project.

See Also
--------

* :doc:`building` - Building from source
* :doc:`testing` - Testing guide

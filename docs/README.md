# DaffyChat Documentation

This directory contains the complete documentation for DaffyChat, built with Sphinx.

## Building Documentation

### Prerequisites

Install Sphinx and dependencies:

```bash
pip install -r requirements.txt
```

For C++ API documentation, install Doxygen:

```bash
sudo apt-get install doxygen graphviz
```

### Build HTML Documentation

```bash
make html
```

Output: `_build/html/index.html`

### Build PDF Documentation

```bash
make latexpdf
```

Output: `_build/latex/daffychat.pdf`

### Build C++ API Documentation

```bash
doxygen Doxyfile
```

Output: `../build/xml/` (consumed by Breathe/Sphinx)

### Build All

```bash
# Generate C++ API docs
doxygen Doxyfile

# Build Sphinx docs (includes C++ API via Breathe)
make html
```

## Documentation Structure

```
docs/
├── conf.py                 # Sphinx configuration
├── index.rst              # Main documentation index
├── introduction.rst       # Introduction
├── installation.rst       # Installation guide
├── quickstart.rst         # Quick start guide
├── architecture/          # Architecture documentation
│   ├── overview.md       # Existing architecture doc
│   ├── control-plane.rst
│   ├── media-plane.rst
│   └── extension-plane.rst
├── dssl/                  # DSSL documentation
│   ├── index.rst
│   ├── README.md         # Existing DSSL overview
│   ├── language-reference.md
│   ├── getting-started.md
│   ├── code-generation.md
│   ├── best-practices.md
│   ├── toolchain.md
│   └── examples/
├── daffyscript/          # Daffyscript documentation
│   ├── index.rst
│   ├── README.md         # Existing Daffyscript overview
│   ├── language-reference.md
│   ├── modules.md
│   ├── programs.md
│   ├── recipes.md
│   ├── compiler.md
│   └── examples.md
├── api/                  # API reference
│   ├── cpp/             # C++ API (via Breathe/Doxygen)
│   ├── python/          # Python toolchain API
│   └── rest/            # REST API reference
├── operations/          # Operations documentation
│   ├── deployment.rst
│   ├── configuration.rst
│   ├── monitoring.rst
│   └── troubleshooting.rst
└── development/         # Development documentation
    ├── building.rst
    ├── testing.rst
    └── contributing.rst
```

## Viewing Documentation

### Local Development Server

```bash
# Install sphinx-autobuild
pip install sphinx-autobuild

# Start live-reload server
sphinx-autobuild . _build/html
```

Open http://localhost:8000

### Static Files

```bash
# Build HTML
make html

# Open in browser
firefox _build/html/index.html
```

## Documentation Formats

### reStructuredText (.rst)

Used for Sphinx-native documentation:

```rst
Section Title
=============

Subsection
----------

* Bullet point
* Another point

.. code-block:: cpp

   int main() {
       return 0;
   }
```

### Markdown (.md)

Existing documentation in Markdown is included via MyST parser:

```rst
.. include:: README.md
   :parser: myst_parser.sphinx_
```

## Contributing to Documentation

1. Edit relevant `.rst` or `.md` files
2. Build documentation locally to verify
3. Submit pull request

See `development/contributing.rst` for details.

## Deployment

Documentation is automatically built and deployed on:

* **Read the Docs:** https://daffychat.readthedocs.io
* **GitHub Pages:** https://yourusername.github.io/daffychat

## Troubleshooting

### Missing Dependencies

```bash
pip install -r requirements.txt
```

### Doxygen Not Found

```bash
sudo apt-get install doxygen graphviz
```

### Build Errors

```bash
# Clean build
make clean
make html
```

### MyST Parser Errors

Ensure MyST parser is installed:

```bash
pip install myst-parser
```

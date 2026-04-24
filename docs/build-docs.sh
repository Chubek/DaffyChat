#!/bin/bash
# Build DaffyChat documentation

set -e

echo "=== Building DaffyChat Documentation ==="

# Check for Sphinx
if ! command -v sphinx-build &> /dev/null; then
    echo "Error: sphinx-build not found. Install with: pip install -r requirements.txt"
    exit 1
fi

# Install Python dependencies
echo "Installing Python dependencies..."
pip install -q -r requirements.txt

# Generate C++ API documentation with Doxygen (optional)
if command -v doxygen &> /dev/null; then
    echo "Generating C++ API documentation with Doxygen..."
    doxygen Doxyfile
else
    echo "Warning: Doxygen not found. Skipping C++ API documentation."
    echo "Install with: sudo apt-get install doxygen graphviz"
fi

# Build HTML documentation
echo "Building HTML documentation..."
make html

echo ""
echo "=== Documentation built successfully! ==="
echo ""
echo "View documentation:"
echo "  HTML: file://$(pwd)/_build/html/index.html"
echo ""
echo "Or start a local server:"
echo "  cd _build/html && python3 -m http.server 8000"
echo "  Then open: http://localhost:8000"

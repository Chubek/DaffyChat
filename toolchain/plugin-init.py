#!/usr/bin/env python3
"""
Plugin Init - Scaffold a new DaffyChat shared library plugin

Creates a template plugin project with build configuration.
"""

import argparse
import sys
from pathlib import Path


PLUGIN_HEADER_TEMPLATE = """#pragma once

#include <string>

// DaffyChat plugin API
extern "C" {{
    const char* plugin_name();
    const char* plugin_version();
    const char* plugin_description();
    int plugin_init();
    void plugin_shutdown();
}}
"""


PLUGIN_SOURCE_TEMPLATE = """#include "{header_name}"

extern "C" {{
    const char* plugin_name() {{
        return "{plugin_name}";
    }}
    
    const char* plugin_version() {{
        return "1.0.0";
    }}
    
    const char* plugin_description() {{
        return "{description}";
    }}
    
    int plugin_init() {{
        // Initialize your plugin here
        return 0;
    }}
    
    void plugin_shutdown() {{
        // Clean up your plugin here
    }}
}}
"""


CMAKE_TEMPLATE = """cmake_minimum_required(VERSION 3.20)
project({plugin_name}_plugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library({plugin_name}_plugin SHARED
    src/{plugin_name}_plugin.cpp
)

target_include_directories({plugin_name}_plugin PUBLIC
    include
)

install(TARGETS {plugin_name}_plugin
    LIBRARY DESTINATION lib/daffychat/plugins
)
"""


README_TEMPLATE = """# {plugin_name} Plugin

{description}

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Installing

```bash
cmake --install build
```

## Usage

The plugin will be automatically loaded by DaffyChat when placed in the plugins directory.
"""


def main():
    parser = argparse.ArgumentParser(
        description="Scaffold a new DaffyChat shared library plugin",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Create a new plugin
  %(prog)s --name myplugin --output plugins/myplugin
  
  # Create with description
  %(prog)s -n auth -d "Authentication plugin" -o plugins/auth
        """
    )
    
    parser.add_argument("--name", "-n", required=True,
                       help="Plugin name (lowercase, no spaces)")
    parser.add_argument("--output", "-o", required=True,
                       help="Output directory path")
    parser.add_argument("--description", "-d",
                       default="DaffyChat plugin",
                       help="Plugin description")
    parser.add_argument("--force", "-f", action="store_true",
                       help="Overwrite existing directory")
    
    args = parser.parse_args()
    
    # Validate plugin name
    plugin_name = args.name.lower().strip()
    if not plugin_name.isidentifier():
        print(f"Error: Invalid plugin name '{plugin_name}'", file=sys.stderr)
        return 1
    
    # Check output directory
    output_dir = Path(args.output)
    if output_dir.exists() and not args.force:
        print(f"Error: Directory already exists: {output_dir}", file=sys.stderr)
        print("Use --force to overwrite", file=sys.stderr)
        return 1
    
    # Create directory structure
    (output_dir / "src").mkdir(parents=True, exist_ok=True)
    (output_dir / "include").mkdir(parents=True, exist_ok=True)
    
    # Generate files
    header_name = f"{plugin_name}_plugin.hpp"
    source_name = f"{plugin_name}_plugin.cpp"
    
    # Write header
    header_path = output_dir / "include" / header_name
    header_path.write_text(PLUGIN_HEADER_TEMPLATE)
    
    # Write source
    source_path = output_dir / "src" / source_name
    source_content = PLUGIN_SOURCE_TEMPLATE.format(
        header_name=header_name,
        plugin_name=plugin_name,
        description=args.description
    )
    source_path.write_text(source_content)
    
    # Write CMakeLists.txt
    cmake_path = output_dir / "CMakeLists.txt"
    cmake_content = CMAKE_TEMPLATE.format(plugin_name=plugin_name)
    cmake_path.write_text(cmake_content)
    
    # Write README
    readme_path = output_dir / "README.md"
    readme_content = README_TEMPLATE.format(
        plugin_name=plugin_name,
        description=args.description
    )
    readme_path.write_text(readme_content)
    
    print(f"Created plugin project: {output_dir}")
    print(f"\nProject structure:")
    print(f"  {output_dir}/")
    print(f"    CMakeLists.txt")
    print(f"    README.md")
    print(f"    include/{header_name}")
    print(f"    src/{source_name}")
    print(f"\nNext steps:")
    print(f"  1. cd {output_dir}")
    print(f"  2. mkdir build && cd build")
    print(f"  3. cmake ..")
    print(f"  4. cmake --build .")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())

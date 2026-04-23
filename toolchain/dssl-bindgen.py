#!/usr/bin/env python3
"""
DSSL Bindgen - Generate service code from DSSL specifications

This is a wrapper around the C++ dssl-bindgen tool that provides
a more user-friendly CLI interface.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def find_dssl_bindgen():
    """Find the dssl-bindgen binary in the build directory."""
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    
    # Check common build locations
    candidates = [
        repo_root / "build" / "dssl-bindgen",
        repo_root / "build" / "Debug" / "dssl-bindgen",
        repo_root / "build" / "Release" / "dssl-bindgen",
    ]
    
    for candidate in candidates:
        if candidate.exists():
            return candidate
    
    # Try to find in PATH
    try:
        result = subprocess.run(["which", "dssl-bindgen"], 
                              capture_output=True, text=True, check=True)
        return Path(result.stdout.strip())
    except subprocess.CalledProcessError:
        pass
    
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Generate service code from DSSL specifications",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate C++ service from DSSL spec
  %(prog)s --target cpp --out-dir ./generated services/specs/echo.dssl
  
  # Generate with custom namespace
  %(prog)s --target cpp --namespace myapp --out-dir ./gen spec.dssl
  
  # Validate DSSL spec without generating code
  %(prog)s --validate-only spec.dssl
        """
    )
    
    parser.add_argument("spec", help="DSSL specification file")
    parser.add_argument("--target", choices=["cpp"], default="cpp",
                       help="Target language (default: cpp)")
    parser.add_argument("--out-dir", required=True,
                       help="Output directory for generated files")
    parser.add_argument("--namespace", 
                       help="Custom namespace for generated code")
    parser.add_argument("--validate-only", action="store_true",
                       help="Only validate the spec, don't generate code")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Enable verbose output")
    
    args = parser.parse_args()
    
    # Find the binary
    bindgen = find_dssl_bindgen()
    if not bindgen:
        print("Error: dssl-bindgen binary not found", file=sys.stderr)
        print("Please build the project first: cmake --build build", file=sys.stderr)
        return 1
    
    # Validate input file
    spec_path = Path(args.spec)
    if not spec_path.exists():
        print(f"Error: Spec file not found: {args.spec}", file=sys.stderr)
        return 1
    
    # Create output directory
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    
    # Build command
    cmd = [str(bindgen), "--target", args.target, "--out-dir", str(out_dir)]
    
    if args.namespace:
        cmd.extend(["--namespace", args.namespace])
    
    if args.validate_only:
        cmd.append("--validate-only")
    
    cmd.append(str(spec_path))
    
    # Execute
    if args.verbose:
        print(f"Executing: {' '.join(cmd)}", file=sys.stderr)
    
    try:
        result = subprocess.run(cmd, check=True)
        return result.returncode
    except subprocess.CalledProcessError as e:
        return e.returncode
    except FileNotFoundError:
        print(f"Error: Failed to execute {bindgen}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

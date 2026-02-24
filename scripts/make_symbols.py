#!/usr/bin/env python3
"""
Generate kernel/symbols.c from kernel/kernel.sym for backtrace symbol lookup.
Usage: python3 scripts/make_symbols.py kernel/kernel.sym kernel/symbols.c
"""

import sys
import re

def is_valid_symbol(addr_str, name):
    """Filter to keep only kernel symbols (non-zero addresses in kernel range)."""
    # Skip sections, debug info, source files, object files, and other metadata
    if not name or name.startswith('.'):
        return False
    if name.endswith('.c') or name.endswith('.o') or name.endswith('.S'):
        return False
    if name[0].isdigit():  # Labels like first.1
        return False
    
    try:
        addr = int(addr_str, 16)
        # Keep only kernel-space symbols (>= 0x80000000)
        return addr >= 0x80000000
    except:
        return False

def parse_symbols(sym_file):
    """Read kernel.sym, extract valid symbols, return list of (address, name) tuples."""
    symbols = []
    with open(sym_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 2:
                addr_str = parts[0]
                name = parts[1]
                if is_valid_symbol(addr_str, name):
                    symbols.append((int(addr_str, 16), name))
    
    # Sort by address for efficient lookup
    symbols.sort(key=lambda x: x[0])
    return symbols

def generate_c_code(symbols):
    """Generate C code for symbols array."""
    code = '''#include "kernel/types.h"

// Auto-generated kernel symbols from kernel/kernel.sym
// Used by backtrace() to print function names

struct {
  uint64 addr;
  const char *name;
} kernel_symbols[] = {
'''
    
    for addr, name in symbols:
        code += f'  {{{addr:#018x}ul, "{name}"}},\n'
    
    code += '''};

int kernel_symbols_len = sizeof(kernel_symbols) / sizeof(kernel_symbols[0]);

// Find symbol name for address, or NULL if not found
const char*
address_to_symbol(uint64 addr)
{
  int lo = 0, hi = kernel_symbols_len - 1;
  int best = -1;
  
  // Binary search for closest symbol <= addr
  while(lo <= hi) {
    int mid = (lo + hi) / 2;
    if(kernel_symbols[mid].addr <= addr) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  
  return best >= 0 ? kernel_symbols[best].name : 0;
}
'''
    return code

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <kernel.sym> <output.c>", file=sys.stderr)
        sys.exit(1)
    
    sym_file = sys.argv[1]
    out_file = sys.argv[2]
    
    symbols = parse_symbols(sym_file)
    code = generate_c_code(symbols)
    
    with open(out_file, 'w') as f:
        f.write(code)
    
    print(f"Generated {out_file} with {len(symbols)} symbols", file=sys.stderr)

if __name__ == '__main__':
    main()

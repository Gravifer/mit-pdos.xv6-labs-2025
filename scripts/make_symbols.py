#!/usr/bin/env python3
"""
Generate kernel/symbols.c from kernel/kernel.sym and DWARF debug info.
Provides address -> (symbol, file, line) lookup for enhanced backtrace.
Usage: python3 scripts/make_symbols.py kernel/kernel.sym kernel/symbols.c
"""

import sys
import re
import subprocess

def is_valid_symbol(addr_str, name):
    """Filter to keep only kernel symbols (non-zero addresses in kernel range)."""
    if not name or name.startswith('.'):
        return False
    if name.endswith('.c') or name.endswith('.o') or name.endswith('.S'):
        return False
    if name[0].isdigit():
        return False
    
    try:
        addr = int(addr_str, 16)
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
    
    symbols.sort(key=lambda x: x[0])
    return symbols

def extract_dwarf_lines(kernel_binary):
    """
    Extract line number information using readelf -wL.
    Returns dict: address -> (filename, line_number)
    """
    lines = {}
    try:
        result = subprocess.run(
            ['readelf', '-wL', kernel_binary],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode == 0:
            current_file = None
            for line in result.stdout.split('\n'):
                # Skip empty lines and headers
                if not line.strip() or 'File name' in line or 'Starting' in line:
                    continue
                
                # File header lines (e.g., "entry.S:", "kalloc.c:")
                if line.rstrip().endswith(':') and '/' not in line:
                    current_file = line.rstrip().rstrip(':')
                    continue
                
                # Line entries: filename at col 0, line number mid, address later
                # Format: "kalloc.c                                  48          0x8000001c"
                if current_file and '0x8' in line:
                    # Parse: filename, line number, address
                    parts = line.split()
                    if len(parts) >= 2:
                        try:
                            # Find address (starts with 0x8)
                            addr_str = None
                            linenum_str = None
                            for part in parts:
                                if part.startswith('0x8'):
                                    addr_str = part
                                elif part.isdigit() and int(part) < 100000:
                                    linenum_str = part
                            
                            if addr_str and linenum_str:
                                addr = int(addr_str, 16)
                                linenum = int(linenum_str)
                                # Simplify filename (remove path)
                                file_base = current_file.split('/')[-1]
                                lines[addr] = (file_base, linenum)
                        except (ValueError, IndexError):
                            pass
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass
    
    return lines

def generate_c_code(symbols, dwarf_lines):
    """Generate C code for enhanced symbols array with file/line info."""
    code = '''#include "kernel/types.h"

// Auto-generated kernel symbols with DWARF line info
// Used by backtrace() to print function names and source locations

struct symbol_info {
  uint64 addr;
  const char *name;
  const char *file;
  int line;
} kernel_symbols[] = {
'''
    
    for addr, name in symbols:
        if addr in dwarf_lines:
            file, linenum = dwarf_lines[addr]
            # Simplify file path to just filename
            file_base = file.split('/')[-1] if '/' in file else file
            code += f'  {{{addr:#018x}ul, "{name}", "{file_base}", {linenum}}},\n'
        else:
            # No line info, use placeholder
            code += f'  {{{addr:#018x}ul, "{name}", "?", 0}},\n'
    
    code += '''};

int kernel_symbols_len = sizeof(kernel_symbols) / sizeof(kernel_symbols[0]);

// Find symbol info for address, or NULL if not found
const struct symbol_info*
address_to_symbol_info(uint64 addr)
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
  
  return best >= 0 ? &kernel_symbols[best] : 0;
}
'''
    return code

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <kernel.sym> <output.c>", file=sys.stderr)
        sys.exit(1)
    
    sym_file = sys.argv[1]
    out_file = sys.argv[2]
    kernel_binary = 'kernel/kernel'  # Expected location
    
    symbols = parse_symbols(sym_file)
    dwarf_lines = extract_dwarf_lines(kernel_binary)
    code = generate_c_code(symbols, dwarf_lines)
    
    with open(out_file, 'w') as f:
        f.write(code)
    
    line_count = len([s for s in symbols if s[0] in dwarf_lines])
    print(f"Generated {out_file} with {len(symbols)} symbols ({line_count} with line info)", file=sys.stderr)

if __name__ == '__main__':
    main()


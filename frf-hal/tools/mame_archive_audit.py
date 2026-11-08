#!/usr/bin/env python3
"""Inventory global undefined dependencies of ARM32 static archives.
Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
Read-only analysis. Does not alter libraries, source, SD media or Git.
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess

def symbols(path, flag):
    cp=subprocess.run(["arm-none-eabi-nm","-g",flag,str(path)],
                      capture_output=True,text=True,check=True,timeout=45)
    output=set()
    for line in cp.stdout.splitlines():
        stripped=line.strip()
        if not stripped or stripped.endswith(":"): continue
        tokens=stripped.split()
        if flag == "--undefined-only":
            if len(tokens)>=2 and tokens[-2].upper()=="U":
                output.add(tokens[-1])
        else:
            if len(tokens)>=3 and tokens[-2].upper() not in ("U","?"):
                output.add(tokens[-1])
    return output

def category(sym):
    if sym.startswith(("osd_","bm_","frf_","log_")): return "FRF/MAME OSD integration"
    if sym.startswith(("_Z","__cxa","__gxx","__aeabi")): return "C++ ABI / ARM EABI"
    if sym in ("malloc","free","calloc","realloc","memcpy","memmove","memset","memcmp","strlen","strcpy","strcmp","strncmp","strncpy","strchr","strrchr","strstr","strtok","strdup","strtol","strtoul","atoi","atol","sprintf","snprintf","vsnprintf","vprintf","printf","fprintf","fopen","fclose","fread","fwrite","fseek","ftell","feof","ferror","fgets","fflush","perror","stat","opendir","readdir","closedir","mkdir","remove","rename","errno","_impure_ptr","__errno","__errno_r","_sbrk","_write","_read","_close","_fstat","_lseek","_isatty","_exit","exit","abort","assert"):
        return "C runtime / storage"
    if sym in ("sin","cos","tan","sqrt","pow","exp","log","log10","floor","ceil","fabs","fmod","atan2","asin","acos","sinf","cosf","sqrtf","powf","floorf","ceilf","fabsf","ldexp","frexp"): return "math lib"
    if sym.startswith(("z_","inflate","deflate","crc32","adler32","XML_")): return "zlib / Expat"
    return "other / inspect"

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("root",type=Path)
    args=parser.parse_args()
    p=args.root
    paths=[p/"libmamecore.a",p/"libmamecoreUNROLL.a",
           p/"zlib"/"libz.a",p/"libexpat"/"libexpat.a"]
    defined=set()
    undefined=set()
    for a in paths:
        if not a.is_file(): raise SystemExit("Archive missing: "+str(a))
        d=symbols(a,"--defined-only")
        u=symbols(a,"--undefined-only")
        print(f"{a.name}: public defined={len(d)}, undefined references={len(u)}")
        defined|=d;undefined|=u
    missing=sorted(undefined-defined)
    c=Counter(category(s) for s in missing)
    print(f"Across 4 libraries: defined={len(defined)} referenced={len(undefined)}")
    print("Unresolved relative to these 4 libraries ONLY:",len(missing))
    for cat in sorted(c):
        names=[s for s in missing if category(s)==cat]
        print(f"\n[{cat}] {len(names)} symbols")
        print("  "+", ".join(names[:75]))
        if len(names)>75:print(f"  ... and {len(names)-75} more")
    print("\nIMPORTANT: Some symbols are satisfied by newlib/libm/libgcc/C++ runtime;")
    print("others may only be reached from selected driver objects. This is")
    print("an inventory, NOT a full executable link failure report.")
if __name__=="__main__":main()

#!/usr/bin/env python3

# This should be ran after compilation (perhaps before linking)

import os
import sys
import glob
import json
import struct

type_name_map = {}  # type name -> type data
global_string_volume = {} # string literal -> relative offset in reflection.dat
arch = -1           # 8 or 4

class BinWriter:
    def __init__(self, size, arch):
        self.data = bytearray(size)
        self.offset = 0
        self.arch = arch

    def ensure_capacity(self, extra):
        if self.offset + extra > len(self.data):
            raise RuntimeError("Buffer overflow; increase the buffer size.")

    def write_uint32(self, value):
        self.ensure_capacity(4)
        struct.pack_into("<I", self.data, self.offset, value)
        self.offset += 4

    def write_int64(self, value):
        self.ensure_capacity(8)
        struct.pack_into("<q", self.data, self.offset, value)
        self.offset += 8

    def write_uint8(self, value):
        self.ensure_capacity(1)
        struct.pack_into("<B", self.data, self.offset, value)
        self.offset += 1

    def write_bool(self, value):
        self.write_uint8(1 if value else 0)

    def write_c_string(self, s):
        encoded = s.encode("utf-8")
        self.ensure_capacity(len(encoded) + 1)
        self.data[self.offset:self.offset+len(encoded)] = encoded
        self.offset += len(encoded)
        self.write_uint8(0)

    def write_arch_size(self, value):
        if self.arch == 8:
            self.write_int64(value)
        else:
            self.write_uint32(value)


def parse_reflection_files(root_dir):
    global arch, type_name_map, global_string_volume, global_string_writer

    pattern = os.path.join(root_dir, "**", "*.reflection.dat")
    files = glob.glob(pattern, recursive=True)
    if not files:
        print("No reflection.dat files found.")
        return

    # Sort files by modification time (oldest first so that newer definitions override)
    files = sorted(files, key=os.path.getmtime)
    print(f"Found {len(files)} reflection.dat files.\n")

    for file in files:
        try:
            with open(file, "r") as f:
                content = f.read().splitlines()
        except Exception as e:
            print(f"Error reading file {file}: {e}")
            continue

        current_data = {}
        current_field_data = {}
        is_in_field_mode = False

        def flush_current_field_data():
            nonlocal is_in_field_mode, current_field_data, current_data
            if not is_in_field_mode:
                return
            if "name" in current_field_data:
                current_data.setdefault("fields", []).append(
                    current_field_data.copy())
            is_in_field_mode = False
            current_field_data = {}

        def flush_current_data():
            nonlocal current_data
            global type_name_map
            flush_current_field_data()
            if "type" in current_data and "name" in current_data:
                type_name_map[current_data["name"]] = current_data.copy()
            current_data = {}

        for line in content:
            parts = line.strip().split()
            if not parts:
                continue
            command = parts[0]
            arg = " ".join(parts[1:]) if len(parts) > 1 else ""
            if command == "arch":
                try:
                    if arch == -1:
                        global global_string_volume, global_string_writer
                        global_string_writer = BinWriter(1000000, int(arg))
                        global_string_volume = {}

                    arch = int(arg)
                except Exception:
                    pass
            elif command in ("base", "enum", "struct", "union"):
                flush_current_field_data()
                flush_current_data()
                if command == "base":
                    current_data = {"type": "base"}
                elif command == "enum":
                    current_data = {"type": "enum",
                                    "fields": [], "aliases": []}
                elif command == "struct":
                    current_data = {"type": "struct",
                                    "fields": [], "aliases": []}
                elif command == "union":
                    current_data = {"type": "union",
                                    "fields": [], "aliases": []}
            elif command in ("field", "enumerator"):
                flush_current_field_data()
                is_in_field_mode = True
            elif command == "ek":
                if arg not in global_string_volume:
                    global_string_volume[arg] = global_string_writer.offset + arch*2
                    global_string_writer.write_c_string(arg)
                current_field_data["name"] = arg
            elif command == "ev":
                try:
                    current_field_data["value"] = int(arg)
                except Exception:
                    current_field_data["value"] = 0
            elif command == "name":
                if arg not in global_string_volume:
                    global_string_volume[arg] = global_string_writer.offset + arch*2
                    global_string_writer.write_c_string(arg)

                if is_in_field_mode:
                    current_field_data["name"] = arg
                else:
                    current_data["name"] = arg
            elif command == "alias":
                if arg not in global_string_volume:
                    global_string_volume[arg] = global_string_writer.offset + arch*2
                    global_string_writer.write_c_string(arg)
                current_data.setdefault("aliases", []).append(arg)
            elif command == "type":
                if arg not in global_string_volume:
                    global_string_volume[arg] = global_string_writer.offset + arch*2
                    global_string_writer.write_c_string(arg)
                current_field_data["type"] = arg
            elif command == "offset":
                try:
                    current_field_data["offset"] = int(arg)
                except Exception:
                    current_field_data["offset"] = 0
            elif command == "pdepth":
                try:
                    current_field_data["pdepth"] = int(arg)
                except Exception:
                    current_field_data["pdepth"] = 0
            elif command == "arrsize":
                try:
                    current_field_data["arrsize"] = int(arg)
                except Exception:
                    current_field_data["arrsize"] = 0
            elif command == "const":
                current_field_data["const"] = (arg == "true")
            elif command == "size":
                try:
                    current_data["size"] = int(arg)
                except Exception:
                    current_data["size"] = 0
            elif command == "isstruct":
                current_field_data["struct"] = (arg == "true")
        flush_current_field_data()
        flush_current_data()


def write_reflection_dat(output_file, output_asm_file):
    global arch, type_name_map, global_string_volume, global_string_writer

    type_id = 1
    for name, data in type_name_map.items():
        data["id"] = type_id
        type_id += 1
    type_name_map.pop("", None)

    # Debug
    with open("reflectionOutput.json", "w") as f:
        json.dump(type_name_map, f, indent=4)

    # Hmm should probably not hardcode this
    writer = BinWriter(1000000, arch)
    object_types = {"base": 1, "struct": 2, "union": 3, "enum": 4}

    head_writer = BinWriter(arch*2, arch)

    # header
    head_writer.write_arch_size(arch * 2) # location of global string literal volume
    head_writer.write_arch_size(arch * 2 + global_string_writer.offset) # location of actual table data

    writer.write_arch_size(len(type_name_map))

    for type_data in type_name_map.values():
        if type_data["type"] == "base":
            writer.write_arch_size(type_data["id"])
            writer.write_uint8(1)  # base type code
            writer.write_arch_size(global_string_volume[type_data["name"]])
            writer.write_arch_size(type_data.get("size", 0))
        else:
            writer.write_arch_size(type_data["id"])
            writer.write_uint8(object_types.get(type_data["type"], 0))
            writer.write_arch_size(global_string_volume[type_data["name"]])
            writer.write_arch_size(type_data.get("size", 0))
            field_count_offset = writer.offset
            writer.write_arch_size(0)  # placeholder for field count
            field_count = 0
            for field in type_data.get("fields", []):
                if type_data["type"] != "enum":
                    if field.get("struct", False) and field.get("type", "") not in type_name_map:
                        continue
                    writer.write_arch_size(global_string_volume[field.get("name", "")])
                    writer.write_bool(field.get("const", False))
                    writer.write_uint32(field.get("pdepth", 0))
                    writer.write_arch_size(field.get("offset", 0))
                    writer.write_arch_size(field.get("arrsize", 0))
                    ftype = field.get("type", "")
                    ref_id = type_name_map[ftype]["id"] if ftype in type_name_map else 0
                    writer.write_arch_size(ref_id)
                    field_count += 1
                else:
                    writer.write_arch_size(global_string_volume[field.get("name", "")])
                    writer.write_arch_size(field.get("value", 0))
                    field_count += 1
            current_offset = writer.offset
            if arch == 8:
                struct.pack_into("<q", writer.data,
                                 field_count_offset, field_count)
            else:
                struct.pack_into("<I", writer.data,
                                 field_count_offset, field_count)
            writer.offset = current_offset

    with open(output_file, "wb") as f:
        f.write(head_writer.data[:head_writer.offset])
        f.write(global_string_writer.data[:global_string_writer.offset])
        f.write(writer.data[:writer.offset])

    # For linking reflection.dat directly with binary
    with open(output_asm_file, "w") as f:
        f.write("#ifdef __APPLE__\n")
        f.write("    .section __TEXT,__const\n")
        f.write("#elif defined(_WIN32)\n")
        f.write("    .section .rdata\n")  # Read-only data section on Windows (MinGW, MSVC)
        f.write("#else\n")
        f.write("    .section .rodata\n")  # Read-only data section on Linux
        f.write("#endif\n\n")

        f.write("    .global _reflection_dat_start\n")
        f.write("    .global _reflection_dat_end\n")
        f.write("_reflection_dat_start:\n")

        full_data = head_writer.data[:head_writer.offset] + global_string_writer.data[:global_string_writer.offset] + writer.data[:writer.offset]

        for i, byte in enumerate(full_data):
            if i % 12 == 0:
                f.write("\n    .byte ")
            f.write(f"0x{byte:02x}")
            if i % 12 != 11 and (i != len(full_data) - 1):
                f.write(", ")

        f.write("\n_reflection_dat_end:\n")

        f.write("\n#ifdef __GNUC__\n")
        f.write("\n#ifndef __APPLE__\n")
        f.write("    .section .note.GNU-stack,\"\",@progbits\n")
        f.write("#endif\n")
        f.write("#endif\n")


def main():
    global type_name_map, arch

    if len(sys.argv) > 2:
        root_dir = sys.argv[1]
        out_dir = sys.argv[2]
    else:
        raise Exception("Missing object file path or output path")

    parse_reflection_files(root_dir)
    output_file = os.path.join(out_dir, "reflection.dat")
    output_asm_file = os.path.join(out_dir, "reflection.dat.S")
    write_reflection_dat(output_file, output_asm_file)


if __name__ == "__main__":
    main()

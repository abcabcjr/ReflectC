#!/usr/bin/env python3

import argparse
import random

base_counter = 1
union_counter = 1
struct_counter = 1
enum_counter = 1
field_counter = 1
alias_counter = 1
enumerator_counter = 1

def next_base_identifier():
    global base_counter
    bid = str(base_counter)
    base_counter += 1
    return bid

def next_union_identifier():
    global union_counter
    uid = str(union_counter)
    union_counter += 1
    return uid

def next_struct_identifier():
    global struct_counter
    sid = str(struct_counter)
    struct_counter += 1
    return sid

def next_enum_identifier():
    global enum_counter
    eid = str(enum_counter)
    enum_counter += 1
    return eid

def next_field_identifier():
    global field_counter
    fid = str(field_counter)
    field_counter += 1
    return fid

def next_alias_identifier():
    global alias_counter
    aid = str(alias_counter)
    alias_counter += 1
    return aid

def next_enumerator_identifier():
    global enumerator_counter
    eid = str(enumerator_counter)
    enumerator_counter += 1
    return eid

def random_bool_str(prob_true=0.3):
    return "true" if random.random() < prob_true else "false"

def generate_base_entry(_):
    lines = []
    lines.append("base")
    bid = next_base_identifier()
    
    if random.random() < 0.2:
        ret_type = random.choice(["int", "long", "void", "unsigned int", "short"])
        num_args = random.randint(0, 3)
        args = []
        for _ in range(num_args):
            arg_type = random.choice(["void *", "char *", "const char *", "int", "long", "unsigned int"])
            args.append(arg_type)
        signature = "{}_func_{} ({})".format(ret_type, bid, ", ".join(args))
        size = 0
    else:
        base_type = random.choice([
            "int", "long long", "unsigned short", "unsigned int",
            "float", "double", "char", "bool", "size_t"
        ])
        signature = base_type + "_" + bid
        size = random.choice([1, 2, 4, 8])
    
    lines.append("name " + signature)
    lines.append("size " + str(size))
    return "\n".join(lines)

def generate_field(indent=""):
    lines = []
    lines.append(indent + "field")
    fid = next_field_identifier()
    lines.append(indent + "name " + fid)

    field_type = random.choice([
        "int", "long", "char", "unsigned int", "float", "double",
        "custom_" + next_field_identifier()
    ])
    lines.append(indent + "type " + field_type)
    
    offset = random.randint(0, 100)
    lines.append(indent + "offset " + str(offset))
    
    pdepth = random.randint(0, 2)
    lines.append(indent + "pdepth " + str(pdepth))
    
    arrsize = random.choice([0, 0, 0, random.randint(1, 10)])
    lines.append(indent + "arrsize " + str(arrsize))
    
    lines.append(indent + "const " + random_bool_str(0.3))
    lines.append(indent + "isstruct " + random_bool_str(0.2))
    return "\n".join(lines)

def generate_union_entry(_, num_fields):
    lines = []
    lines.append("union")
    uid = next_union_identifier()
    lines.append("name union_" + uid)
    
    if random.random() < 0.3:
        lines.append("alias alias_" + next_alias_identifier())
        
    size = random.choice([8, 16, 32, 64, 128])
    lines.append("size " + str(size))
    
    for _ in range(num_fields):
        lines.append(generate_field())
    return "\n".join(lines)

def generate_struct_entry(_, num_fields):
    lines = []
    lines.append("struct")
    sid = next_struct_identifier()
    lines.append("name struct_" + sid)
    
    if random.random() < 0.3:
        lines.append("alias alias_" + next_alias_identifier())
    
    size = random.choice([8, 16, 24, 32, 64, 128, 256])
    lines.append("size " + str(size))
    
    for _ in range(num_fields):
        lines.append(generate_field())
    return "\n".join(lines)

def generate_enum_entry(_, num_enumerators):
    lines = []
    lines.append("enum")
    eid = next_enum_identifier()
    lines.append("name enum_" + eid)
    lines.append("size 4")
    
    for _ in range(num_enumerators):
        lines.append("enumerator")
        lines.append("ek " + next_enumerator_identifier())
        enum_val = random.randint(0, 1000)
        lines.append("ev " + str(enum_val))
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(
        description="Generate synthetic reflection data"
    )
    parser.add_argument("--bases", type=int, default=50, help="Number of base type entries")
    parser.add_argument("--unions", type=int, default=20, help="Number of union entries")
    parser.add_argument("--structs", type=int, default=20, help="Number of struct entries")
    parser.add_argument("--enums", type=int, default=10, help="Number of enum entries")
    parser.add_argument("--fields", type=int, default=3, help="Number of fields per union/struct")
    parser.add_argument("--enumerators", type=int, default=4, help="Number of enumerators per enum")
    parser.add_argument("--output", type=str, default="synthetic_data.txt", help="Output file name")
    args = parser.parse_args()
    
    with open(args.output, "w") as f:
        f.write("arch 8\n")
        
        for i in range(args.bases):
            f.write(generate_base_entry(i) + "\n")
        
        for i in range(args.unions):
            f.write(generate_union_entry(i, args.fields) + "\n")
        
        for i in range(args.structs):
            f.write(generate_struct_entry(i, args.fields) + "\n")
        
        for i in range(args.enums):
            f.write(generate_enum_entry(i, args.enumerators) + "\n")
    
    print(f"Synthetic reflection data written to {args.output}")

if __name__ == "__main__":
    main()

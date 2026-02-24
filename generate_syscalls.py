import re

def parse_syscall_table(filepath):
    syscalls = {}
    with open(filepath, 'r') as f:
        for line in f:
            parts = line.split()
            if not parts or not parts[0].isdigit():
                continue
            num = int(parts[0])
            abi = parts[1]
            name = parts[2]
            entry = parts[3] if len(parts) > 3 else f"sys_{name}"

            if abi in ['common', '64']:
                syscalls[num] = {'name': name, 'entry': entry}
    return syscalls

def find_implemented_syscalls(filepath):
    implemented = set()
    with open(filepath, 'r') as f:
        content = f.read()
        # Regex for standard static implementation
        matches = re.findall(r'static int64_t (sys_\w+)', content)
        implemented.update(matches)

        # Check for non-standard implementations or special cases
        if 'sys_mmap' in content: implemented.add('sys_mmap')
        if 'sys_fork' in content: implemented.add('sys_fork') # If it exists

    return implemented

def generate_header(syscalls):
    lines = []

    sorted_nums = sorted(syscalls.keys())
    for num in sorted_nums:
        name = syscalls[num]['name']
        lines.append(f"#define SYS_{name:<20} {num}")

    return "\n".join(lines)

def generate_table_init(syscalls, implemented):
    lines = []
    lines.append("#define MAX_SYSCALL_NUM 512")
    lines.append("")
    lines.append("typedef int64_t (*syscall_ptr_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);")
    lines.append("")
    lines.append("static int64_t sys_ni_syscall(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {")
    lines.append("    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;")
    lines.append("    return -ENOSYS;")
    lines.append("}")
    lines.append("")
    lines.append("static syscall_ptr_t syscall_table[MAX_SYSCALL_NUM] = {")
    lines.append("    [0 ... MAX_SYSCALL_NUM - 1] = sys_ni_syscall,")

    sorted_nums = sorted(syscalls.keys())

    mapping = {
        'sys_newstat': 'sys_stat',
        'sys_newfstat': 'sys_fstat',
        'sys_newuname': 'sys_uname',
        'sys_sendfile64': 'sys_sendfile',
        # Add other mappings if needed
    }

    # Special handlers that shouldn't be in the table directly if they take regs
    # clone(56), fork(57), vfork(58), execve(59), rt_sigreturn(15)
    special_syscalls = [15, 56, 57, 58, 59]

    for num in sorted_nums:
        if num in special_syscalls:
            continue

        entry = syscalls[num]['entry']
        target = mapping.get(entry, entry)

        if target in implemented:
             lines.append(f"    [{num}] = (syscall_ptr_t){target},")

    lines.append("};")
    return "\n".join(lines)

syscalls = parse_syscall_table('syscalls.tbl')
implemented = find_implemented_syscalls('kernel/arch/x86_64/syscall.c')
header_content = generate_header(syscalls)
table_content = generate_table_init(syscalls, implemented)

with open('syscall_header_part.txt', 'w') as f:
    f.write(header_content)

with open('syscall_table_part.txt', 'w') as f:
    f.write(table_content)

print("Generated.")

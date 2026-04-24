import re

with open('kernel/arch/x86_64/syscall.c', 'r') as f:
    content = f.read()

# Fix the extern redeclarations which are causing conflicts with the actual sys_ni_syscall
content = re.sub(r'extern int sys_ni_syscall\([^;]+;\n', '', content)

# Fix missing cast for sys_ni_syscall in the arrays
content = content.replace('[0 ... MAX_SYSCALL_NUM - 1] = sys_ni_syscall,', '[0 ... MAX_SYSCALL_NUM - 1] = (syscall_ptr_t)sys_ni_syscall,')

with open('kernel/arch/x86_64/syscall.c', 'w') as f:
    f.write(content)

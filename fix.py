import sys

def main():
    with open('tests/test_elf_read_failure.c', 'r') as f:
        lines = f.readlines()

    start_idx = -1
    end_idx = -1

    for i, line in enumerate(lines):
        if line.startswith('typedef struct {') and 'e_ident[EI_NIDENT]' in lines[i+1]:
            start_idx = i
        if line.startswith('} Elf64_Phdr;'):
            end_idx = i
            break

    if start_idx != -1 and end_idx != -1:
        new_lines = lines[:start_idx] + ['#include "../include/elf.h"\n'] + lines[end_idx+1:]
        with open('tests/test_elf_read_failure.c', 'w') as f:
            f.writelines(new_lines)
        print("Replaced definitions with include.")
    else:
        print("Could not find the definitions to replace.")

if __name__ == '__main__':
    main()

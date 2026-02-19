extern int main(int argc, char **argv);
extern void exit(int status);

__attribute__((naked))
void _start() {
    /*
     * Kernel passes:
     * RSP -> argc (8 bytes)
     * RSP+8 -> argv[0] (8 bytes)
     * ...
     */
    asm volatile (
        "xor %rbp, %rbp\n\t"  // Mark end of stack frames for debuggers
        "pop %rdi\n\t"        // argc (in RDI for first argument)
        "mov %rsp, %rsi\n\t"  // argv (RSP now points to argv[0], put in RSI for second argument)

        /*
         * Stack Alignment:
         * At _start entry, RSP is 16-byte aligned (guaranteed by kernel loader).
         * 'pop %rdi' adds 8 to RSP -> RSP is 8-byte aligned (misaligned for function calls).
         * 'call main' pushes return address (8 bytes) -> RSP becomes 16-byte aligned.
         * This satisfies the System V AMD64 ABI requirement for function entry.
         */

        "call main\n\t"
        "mov %eax, %edi\n\t"  // return value of main (EAX) to exit status (EDI)
        "call exit\n\t"
        "hlt\n\t"             // Should not be reached
    );
}

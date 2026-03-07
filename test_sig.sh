#!/bin/bash
grep -n "sys_rt_sigaction" kernel/arch/x86_64/syscall.c
grep -n "sys_rt_sigreturn" kernel/arch/x86_64/syscall.c

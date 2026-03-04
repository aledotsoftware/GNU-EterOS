#ifndef CPU_MOCK_H
#define CPU_MOCK_H
#include "../include/cpu.h"
#undef get_current_cpu
#define get_current_cpu() (&cpu_mock_struct)
extern cpu_info_t cpu_mock_struct;
#endif

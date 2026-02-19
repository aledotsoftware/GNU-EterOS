#ifndef KLOG_H
#define KLOG_H

/**
 * éterOS - Kernel Logging System
 *
 * Centralized logging mechanism with severity levels and automatic routing.
 */

/* ========================================================================= */
/* Log Levels                                                                */
/* ========================================================================= */

#define KLOG_DEBUG  0   /* Debugging info (Serial only) */
#define KLOG_INFO   1   /* General information (Serial + Console) */
#define KLOG_WARN   2   /* Warnings (Serial + Console) */
#define KLOG_ERROR  3   /* Critical errors (Serial + Console) */
#define KLOG_PANIC  4   /* System halt (Serial + Console + Halt) */

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Logs a formatted message with a specific severity level.
 *
 * @param level The severity level (KLOG_DEBUG ... KLOG_PANIC).
 * @param fmt   Format string (supports %s, %d, %x, %c, %p, %u).
 * @param ...   Arguments for the format string.
 */
void klog(int level, const char* fmt, ...);

#endif /* KLOG_H */

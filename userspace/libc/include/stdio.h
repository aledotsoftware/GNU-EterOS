#ifndef _ETEROS_STDIO_H
#define _ETEROS_STDIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __ETEROS_HOST_TEST__
#include_next <stdio.h>

#undef printf
#define printf eteros_printf
#undef dprintf
#define dprintf eteros_dprintf
#undef snprintf
#define snprintf eteros_snprintf
#undef vprintf
#define vprintf eteros_vprintf
#undef vfprintf
#define vfprintf eteros_vfprintf
#undef vsnprintf
#define vsnprintf eteros_vsnprintf

#undef fopen
#define fopen eteros_fopen
#undef fclose
#define fclose eteros_fclose
#undef fread
#define fread eteros_fread
#undef fwrite
#define fwrite eteros_fwrite
#undef fseek
#define fseek eteros_fseek
#undef ftell
#define ftell eteros_ftell
#undef rewind
#define rewind eteros_rewind
#undef fflush
#define fflush eteros_fflush

#undef fputc
#define fputc eteros_fputc
#undef fgetc
#define fgetc eteros_fgetc
#undef fgets
#define fgets eteros_fgets
#undef fputs
#define fputs eteros_fputs

#undef stdin
#define stdin eteros_stdin
#undef stdout
#define stdout eteros_stdout
#undef stderr
#define stderr eteros_stderr

#undef FILE
#define FILE eteros_FILE

#undef perror
#define perror eteros_perror
#undef remove
#define remove eteros_remove
#undef rename
#define rename eteros_rename
#endif

#ifndef __ETEROS_HOST_TEST__
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct _eteros_IO_FILE FILE;
#ifndef __ETEROS_HOST_TEST__
typedef uint64_t size_t;
#endif

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Very basic printf support */
#ifdef __ETEROS_HOST_TEST__
int eteros_printf(const char *format, ...);
int eteros_dprintf(int fd, const char *format, ...);
int eteros_snprintf(char *str, size_t size, const char *format, ...);
int eteros_vprintf(const char *format, __builtin_va_list ap);
int eteros_vfprintf(FILE *stream, const char *format, __builtin_va_list ap);
int eteros_vsnprintf(char *str, size_t size, const char *format, __builtin_va_list ap);

#undef fprintf
#define fprintf eteros_fprintf
int eteros_fprintf(FILE *stream, const char *format, ...);

/* File I/O */
FILE *eteros_fopen(const char *pathname, const char *mode);
int   eteros_fclose(FILE *stream);
size_t eteros_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t eteros_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int   eteros_fseek(FILE *stream, long offset, int whence);
long  eteros_ftell(FILE *stream);
void  eteros_rewind(FILE *stream);
int   eteros_fflush(FILE *stream);

/* Character/String I/O */
int   eteros_fputc(int c, FILE *stream);
int   eteros_fgetc(FILE *stream);
char *eteros_fgets(char *s, int size, FILE *stream);
int   eteros_fputs(const char *s, FILE *stream);

/* Error */
void eteros_perror(const char *s);

/* Operations */
int eteros_remove(const char *pathname);
int eteros_rename(const char *oldpath, const char *newpath);

#else

int printf(const char *format, ...);
int dprintf(int fd, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, __builtin_va_list ap);
int vfprintf(FILE *stream, const char *format, __builtin_va_list ap);
int vsnprintf(char *str, size_t size, const char *format, __builtin_va_list ap);

int fprintf(FILE *stream, const char *format, ...);

/* File I/O */
FILE *fopen(const char *pathname, const char *mode);
int   fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int   fseek(FILE *stream, long offset, int whence);
long  ftell(FILE *stream);
void  rewind(FILE *stream);
int   fflush(FILE *stream);

/* Character/String I/O */
int   fputc(int c, FILE *stream);
int   fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int   fputs(const char *s, FILE *stream);

/* Error */
void perror(const char *s);

/* Operations */
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
#endif

#endif /* _ETEROS_STDIO_H */

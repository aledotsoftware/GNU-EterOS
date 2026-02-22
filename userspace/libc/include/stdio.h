#ifndef _STDIO_H
#define _STDIO_H

#include <stdint.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct _IO_FILE FILE;
typedef uint64_t size_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Very basic printf support */
int printf(const char *format, ...);
int dprintf(int fd, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);

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

#endif /* _STDIO_H */

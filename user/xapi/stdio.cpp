#include "./include/x3api.h"
#include "./include/libsys.h"
#include "./include/xposix/syscall_ret.h"

extern "C" ssize_t read(int fd, void *buf, size_t len)
{
    return (ssize_t)__xposix_ret(enter_syscall(SYS_READ, fd, (uint64_t)buf, len, 0, 0, 0));
}

extern "C" ssize_t write(int fd, const void *buf, size_t len)
{
    return (ssize_t)__xposix_ret(enter_syscall(SYS_WRITE, fd, (uint64_t)buf, len, 0, 0, 0));
}

#define CANREAD(flag)  ((flag) & READ || (flag) & PLUS)
#define CANWRITE(flag) ((flag) & WRITE || (flag) & PLUS || (flag) & APPEND)

int feof(FILE *stream)
{ return stream->eof ? -1 : 0; }
int ferror(FILE *stream)
{ return 0; }

int fseek(FILE *fp, int offset, int whence)
{
    if (whence == 0) { fp->p = offset; }
    else if (whence == 1) { fp->p += offset; }
    else if (whence == 2) { fp->p = fp->fileSize + offset; }
    else
    {
        return -1;
    }
    return 0;
}

long ftell(FILE *stream)
{ return stream->p; }

FILE *fopen(const char *filename, const char *mode)
{
    XFILE *xf = xapi_OpenFile((char *)filename);
    unsigned int flag = 0;
    FILE        *fp   = (FILE *)malloc(sizeof(FILE));
    fp->read_flag     = 0;
    while (*mode != '\0')
    {
        switch (*mode)
        {
        case 'a': flag |= APPEND; break;
        case 'b': break;
        case 'r': flag |= READ; break;
        case 'w': flag |= WRITE; break;
        case '+': flag |= PLUS; break;
        default: break;
        }
        mode++;
    }
    if (xf == NULL)
    {
        free(fp);
        return NULL;
    }
    if (xf->length <= 0)
    {
        if (flag & READ)
        {
            free(fp);
            xapi_CloseFile(xf);
            return NULL; // 找不到
        }
        // vfs似乎没有创建文件
        // if (flag & WRITE || flag & APPEND) {
        // 	if (!mkfile(filename)) {
        // 		free(fp);
        // 		return NULL;
        // 	}
        // 	fp->read_flag = 2;
        // }
    }
    if (flag & WRITE) { fp->fileSize = 0; }
    else
    {
        fp->fileSize = xf->length;
    }
    fp->bufferSize = 0;
    if (flag & READ || flag & PLUS || flag & APPEND) { fp->bufferSize = xf->length; }
    if (flag & WRITE || flag & PLUS || flag & APPEND) { fp->bufferSize += 100; }
    if (fp->bufferSize == 0) { fp->bufferSize = 1; }
    fp->buffer = (unsigned char *)(malloc(fp->bufferSize));
    if (flag & PLUS || flag & APPEND || flag & READ)
    {
        if (fp->read_flag != 2) fp->read_flag = 1;
    }
    fp->p   = 0;
    fp->eof = 0;
    if (flag & APPEND) { fp->p = fp->fileSize; }
    fp->name = (char *)(malloc(strlen(filename) + 1));
    strcpy(fp->name, filename);
    fp->mode = flag;
    //	printf("[fopen]BufferSize=%d\n",fp->bufferSize);
    xapi_CloseFile(xf);
    return fp;
}

int fclose(FILE *fp)
{
    if (fp == NULL) { return EOF; }
    free(fp->buffer);
    free(fp->name);
    free(fp);
    return 0;
}

int fgetc(FILE *stream)
{
    XFILE *xf = xapi_OpenFile(stream->name);
    if (!xf) { return EOF; }
    if (CANREAD(stream->mode))
    {
        if (stream->p >= stream->fileSize || stream->fileSize == -1)
        {
            stream->eof = 1;
            xapi_CloseFile(xf);
            return EOF;
        }
        else
        {
            if (stream->read_flag == 1)
            {
                memcpy(stream->buffer, xf->buffer, stream->fileSize);
                stream->read_flag = 0;
            }
            int ch = stream->buffer[stream->p++];
            xapi_CloseFile(xf);
            return ch;
        }
    }
    else
    {
        xapi_CloseFile(xf);
        return EOF;
    }
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
{
    if (CANREAD(stream->mode))
    {
        unsigned char *c_ptr = (unsigned char *)buffer;
        for (size_t i = 0; i < size * count; i++)
        {
            unsigned int ch = fgetc(stream);
            if (ch == EOF) { return i; }
            else
            {
                c_ptr[i] = ch;
            }
        }
        return count;
    }
    else
    {
        return 0;
    }
}

int ungetc(int c, FILE *fp)
{
    if (fp->p - 1 < 0) { return EOF; }
    else
    {
        fp->p             -= 1;
        fp->buffer[fp->p]  = c;
        return c;
    }
}

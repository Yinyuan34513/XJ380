#include "./stdio.h"
#include "../include/include.h"

#define CANREAD(flag)  ((flag) & READ || (flag) & PLUS)
#define CANWRITE(flag) ((flag) & WRITE || (flag) & PLUS || (flag) & APPEND)

int feof(FILE *stream) {
    return stream->eof ? -1 : 0;
}
int ferror(FILE *stream) {
    return 0;
}

int fseek(FILE *fp, int offset, int whence) {
    if (whence == 0) {
        fp->p = offset;
    } else if (whence == 1) {
        fp->p += offset;
    } else if (whence == 2) {
        fp->p = fp->fileSize + offset;
    } else {
        return -1;
    }
    return 0;
}

long ftell(FILE *stream) {
    return stream->p;
}

FILE *fopen(const char *filename, const char *mode) {
    VNode       *v    = path_walk(current_task->cwd_node, filename);
    unsigned int flag = 0;
    FILE        *fp   = (FILE *)malloc(sizeof(FILE));
    fp->read_flag     = 0;
    while (*mode != '\0') {
        switch (*mode) {
        case 'a': flag |= APPEND; break;
        case 'b': break;
        case 'r': flag |= READ; break;
        case 'w': flag |= WRITE; break;
        case '+': flag |= PLUS; break;
        default: break;
        }
        mode++;
    }
    if (v->file_size <= 0) {
        if (flag & READ) {
            free(fp);
            return nullptr; // 找不到
        }
        // vfs似乎没有创建文件
        // if (flag & WRITE || flag & APPEND) {
        // 	if (!mkfile(filename)) {
        // 		free(fp);
        // 		return nullptr;
        // 	}
        // 	fp->read_flag = 2;
        // }
    }
    if (flag & WRITE) {
        fp->fileSize = 0;
    } else {
        fp->fileSize = v->file_size;
    }
    fp->bufferSize = 0;
    if (flag & READ || flag & PLUS || flag & APPEND) { fp->bufferSize = v->file_size; }
    if (flag & WRITE || flag & PLUS || flag & APPEND) { fp->bufferSize += 100; }
    if (fp->bufferSize == 0) { fp->bufferSize = 1; }
    fp->buffer = (unsigned char *)(malloc(fp->bufferSize));
    if (flag & PLUS || flag & APPEND || flag & READ) {
        if (fp->read_flag != 2) fp->read_flag = 1;
    }
    fp->p   = 0;
    fp->eof = 0;
    if (flag & APPEND) { fp->p = fp->fileSize; }
    fp->name = (char *)(malloc(strlen(filename) + 1));
    strcpy(fp->name, filename);
    fp->mode = flag;
    //	printf("[fopen]BufferSize=%d\n",fp->bufferSize);
    return fp;
}

int fclose(FILE *fp) {
    if (fp == nullptr) { return EOF; }
    free(fp->buffer);
    free(fp->name);
    free(fp);
    return 0;
}

int fgetc(FILE *stream) {
    VNode *v = path_walk(current_task->cwd_node, stream->name);
    if (CANREAD(stream->mode)) {
        if (stream->p >= stream->fileSize || stream->fileSize == -1) {
            stream->eof = 1;
            return EOF;
        } else {
            if (stream->read_flag == 1) {
                v->ops->read(v, stream->buffer, 0, stream->fileSize);
                stream->read_flag = 0;
            }
            return stream->buffer[stream->p++];
        }
    } else {
        return EOF;
    }
}

unsigned int fread(void *buffer, unsigned int size, unsigned int count, FILE *stream) {
    if (CANREAD(stream->mode)) {
        unsigned char *c_ptr = (unsigned char *)buffer;
        for (int i = 0; i < size * count; i++) {
            unsigned int ch = fgetc(stream);
            if (ch == EOF) {
                return i;
            } else {
                c_ptr[i] = ch;
            }
        }
        return count;
    } else {
        return 0;
    }
}

int ungetc(int c, FILE *fp) {
    if (fp->p - 1 < 0) {
        return EOF;
    } else {
        fp->p             -= 1;
        fp->buffer[fp->p]  = c;
        return c;
    }
}

/*
 * FRF HAL tiny diagnostic snprintf implementation.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Limited to the formats used by FRF GUI diagnostic images.
 * Memory routines are provided by the independent usb_runtime.c.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} frf_output;
static void frf_put(frf_output *out, char c)
{
    if (out->capacity && out->length < out->capacity-1u)
        out->buffer[out->length] = c;
    ++out->length;
}
static void frf_string(frf_output *out, const char *text)
{
    if (!text) text="(null)";
    while (*text) frf_put(out,*text++);
}
static void frf_uint(frf_output *out, uint32_t value,
                     unsigned base, unsigned width, char pad, int upper)
{
    char tmp[32];
    unsigned length=0;
    const char *digits=upper?"0123456789ABCDEF":"0123456789abcdef";
    do {
        tmp[length++]=digits[value%base];
        value/=base;
    } while(value!=0u);
    while (length<width) {
        frf_put(out,pad);
        --width;
    }
    while (length) frf_put(out,tmp[--length]);
}
int snprintf(char *buffer, size_t capacity, const char *format, ...)
{
    frf_output out={buffer,capacity,0u};
    va_list ap;
    va_start(ap,format);
    while (*format) {
        if (*format!='%') { frf_put(&out,*format++);continue; }
        ++format;
        if (*format=='%') {frf_put(&out,'%');++format;continue;}
        char pad=' ';
        if (*format=='0'){pad='0';++format;}
        unsigned width=0;
        while (*format>='0' && *format<='9'){
            if(width<100u)width=width*10u+(unsigned)(*format-'0');
            ++format;
        }
        const char spec=*format?*format++:'\0';
        if(spec=='s') {
            frf_string(&out,va_arg(ap,const char *));
        } else if(spec=='u'||spec=='X'||spec=='x') {
            uint32_t value=va_arg(ap,unsigned);
            frf_uint(&out,value,spec=='u'?10u:16u,width,pad,spec=='X');
        } else if(spec=='d'||spec=='i') {
            int value=va_arg(ap,int);
            if(value<0){
                frf_put(&out,'-');
                uint32_t positive=0u-(uint32_t)value;
                frf_uint(&out,positive,10u,width?width-1u:0u,pad,0);
            }else frf_uint(&out,(uint32_t)value,10u,width,pad,0);
        } else if(spec=='c') {
            frf_put(&out,(char)va_arg(ap,int));
        } else {
            frf_put(&out,'?');
        }
    }
    va_end(ap);
    if(capacity) buffer[out.length < capacity?out.length:capacity-1u]='\0';
    return (int)out.length;
}

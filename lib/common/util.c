/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>

#include "util.h"
#include "log.h"
#include "os.h"


#define UTIL_URI_MAX_LENG           512

/**
 * Similar to snprintf(), except it appends (concatenates) the resulting string to str.
 * @p str is updated to point to the end of the string while @p size is decreased to
 * reflect the new buffer size.
 *
 * This function can be used to "write" to a memory buffer incrementally.
 */
int csnprintf(char **str, size_t *size, const char *fmt, ...)
{
    va_list va;
    int ret;
    size_t len;

    va_start(va, fmt);

    ret = vsnprintf(*str, *size, fmt, va);
    len = (size_t)ret;
    if (ret < 0) len = 0;
    if (len >= *size)
    {
        len = *size - 1;
    }

    /* Update pointers */
    *str  += len;
    *size -= len;

    va_end(va);

    return ret;
}


/**
 * tsnprintf = truncate snprintf
 * Same as snprintf() but don't warn about truncation (caused by -Wformat-truncation)
 * This can be used when the truncation of str to size is explicitly desired
 * Either when str size can't be increased or it is known to be large enough
 * Examples: dns hostname: 64, mac address: 18, ...
 */
int tsnprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list va;
    int ret;
    va_start(va, fmt);
    ret = vsnprintf(str, size, fmt, va);
    va_end(va);
    return ret;
}


/**
 * Split a string into shell-style arguments. This function works in a similar fashion
 * as the strsep() function, except it understands quotes, double quotes and
 * backslashes. This means that, for example, "1 2 3" and '1 2 3' are interpreted
 * as a single tokens.
 *
 * @p cmd should be initialized to the string that is to be split. strargv() will
 * return a pointer to the next token or NULL if end of the string is reached.
 *
 * @warn This function modifies the content of @p cmd; if the content is to be preserved,
 * it is up to to the caller to make a copy before calling this function.
 */
char* strargv(char **cmd, bool with_quotes)
{
    char *dcmd;
    char *scmd;
    char *retval;
    char quote_char;

    enum
    {
        TOK_SPACE,
        TOK_WORD,
        TOK_QUOTE,
        TOK_END,
    }
    state = TOK_SPACE;

    if (*cmd == NULL) return NULL;

    dcmd = scmd = *cmd;

    quote_char = '\0';

    while (state != TOK_END && *scmd != '\0')
    {
        switch (state)
        {
            /* Skip whitespace */
            case TOK_SPACE:
                while (isspace(*scmd)) scmd++;

                if (*scmd == '\0') return NULL;

                state = TOK_WORD;
                break;

            /* Parse non-whitespace sequence */
            case TOK_WORD:
                /* Escape backslashes */
                if (*scmd == '\\')
                {
                    scmd++;

                    if (*scmd != '\0')
                    {
                        *dcmd++ = *scmd++;
                    }
                    else
                    {
                        *scmd = '\\';
                    }
                }

                /* Switch to QUOTE mode */
                if (strchr("\"'", *scmd) != NULL)
                {
                    if (false == with_quotes)
                    {
                        state = TOK_QUOTE;
                    }
                    else
                    {
                        *dcmd++ = *scmd++;
                    }
                    break;
                }

                if (isspace(*scmd))
                {
                    state = TOK_END;
                    break;
                }

                /* Copy chars */
                *dcmd++ = *scmd++;
                break;

            case TOK_QUOTE:
                if (quote_char == '\0') quote_char = *scmd++;

                /* Un-terminated quote */
                if (*scmd == '\0')
                {
                    state = TOK_END;
                    break;
                }

                /* Escape backslashes */
                if (quote_char == '"' && *scmd == '\\')
                {
                    scmd++;

                    if (*scmd != '\0')
                    {
                        *dcmd++ = *scmd++;
                    }
                    else
                    {
                        *scmd = '\\';
                    }

                    break;
                }

                if (*scmd == quote_char)
                {
                    quote_char = '\0';
                    state = TOK_WORD;
                    scmd++;
                    break;
                }


                *dcmd++ = *scmd++;
                break;

            case TOK_END:
                break;
        }
    }

    retval = *cmd;
    if (*scmd == '\0')
    {
        *cmd = NULL;
    }
    else
    {
        *cmd = scmd + 1;
    }

    *dcmd = '\0';

    return retval;
}

/**
 * Compare two strings and their length
 */
int strcmp_len(char *a, size_t alen, char *b, size_t blen)
{
    if (alen != blen) return (int)(alen - blen);

    return strncmp(a, b, alen);
}

static char base64_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64 encode @p input_sz bytes from @p input and store the result to out
 *
 * @retval
 * If the output buffer is not big enough to store the encoded result, a number less than 0
 * is returned. Otherwise the number of bytes stored in @p out is returned.
 */
ssize_t base64_encode(char *out, ssize_t out_sz, void *input, ssize_t input_sz)
{
    uint8_t *pin = input;
    char *pout = out;
    uint8_t m[4];

    if ((((input_sz + 2) / 3) * 4) >= out_sz) return -1;

    while (input_sz > 0)
    {
        m[0] = m[1] = m[2] = m[3] = 0;
        /* pout[0] and pout[1] are never '=' */
        pout[2] = pout[3] = '=';

        switch (input_sz)
        {
            default:
                m[3] = pin[2];
                m[2] = pin[2] >> 6;

                pout[3] = base64_table[m[3] & 63];
                /* Falls through. */

            case 2:
                m[2] |= (uint8_t)(pin[1] << 2);
                m[1] |= pin[1] >> 4;
                pout[2] = base64_table[m[2] & 63];
                /* Falls through. */

            case 1:
                m[1] |= (uint8_t)(pin[0] << 4);
                m[0] |= pin[0] >> 2;

                pout[1] = base64_table[m[1] & 63];
                pout[0] = base64_table[m[0] & 63];
                /* Falls through. */
        }

        pout += 4;
        pin += 3;
        input_sz -= 3;
    }

    *pout++ = '\0';

    return pout - out;
}

/**
 * Decode a base64 buffer.
 *
 * @retval
 *
 * This function returns the number of bytes stored in the output buffer.
 * A negative number is returned if the output buffer doesn't contain enough
 * room to store the entire decoded buffer or if there's an invalid character
 * in the input.
 */
ssize_t base64_decode(void *out, ssize_t out_sz, char *input)
{
    int ii;
    ssize_t input_sz = (ssize_t)strlen(input);

    if (input_sz == 0) return 0;
    if ((input_sz % 4) != 0) return -1;

    /* Clip ending '=' characters */
    for (ii = 0; ii < 2; ii++) if (input[input_sz - 1] == '=') input_sz -= 1;

    /* Check output length */
    if (((input_sz >> 2) * 3 + (((input_sz & 3) * 3) >> 2)) > out_sz) return -1;

    /* Calculate total output size */
    char *pin = input;
    uint8_t *pout = out;

    while (input_sz > 0)
    {
        uint8_t m[4] = {0};

        /* Calculate number of bytes to process this round */
        int isz = (int)((input_sz > 4) ? 4 : input_sz);

        /* Translate a single character to it's base64 value according to base64_table */
        for (ii = 0; ii < isz; ii++)
        {
            char *p = strchr(base64_table, pin[ii]);

            /* Invalid character found -- error*/
            if (p == NULL) return -1;

            m[ii] = (uint8_t)(p - base64_table);
        }

        /* Process a 4-byte (or less) block */
        switch (isz)
        {
            default:
                pout[2] = (uint8_t)((m[2] << 6) | m[3]);
                /* Falls through. */
            case 3:
                pout[1] = (uint8_t)((m[1] << 4) | (m[2] >> 2));
                /* Falls through. */
            case 2:
                pout[0] = (uint8_t)((m[0] << 2) | (m[1] >> 4));
                /* Falls through. */
        }

        pout += isz - 1;
        input_sz -= 4;
        pin += 4;
    }

    return pout - (uint8_t *)out;
}

/**
 * Unescape \xXX sequences in @p str.
 */
char *str_unescape_hex(char *str)
{
    char *s;
    char *d;
    int n;

    for (s=str, d=str; *s; d++) {
        if (*s == '\\') {
            s++;
            switch (*s++) {
                case '\\': *d = '\\';   break;
                case '"':  *d = '"';    break;
                case 'e':  *d = '\033'; break;
                case 't':  *d = '\t';   break;
                case 'n':  *d = '\n';   break;
                case 'r':  *d = '\r';   break;
                case 'x':  n = 0; sscanf(s, "%02hhx%n", d, &n); s += n; break;
                default:   *d = 0; return str;
            }
        } else {
            *d = *s++;
        }
    }
    *d = 0;
    return str;
}

/**
 * Remove all characters in @p delim from the end of the string
 */
char *strchomp(char *str, char *delim)
{
    int len;

    if (!str)
        return NULL;

    len = (int)strlen(str);
    while (len > 0 &&
            (strchr(delim, str[len - 1]) != NULL))
    {
        str[len - 1] = '\0';
        len--;
    }
    return str;
}


/*
 * This function checks array of strings
 * In key is present in at least one of the array members, true is
 * returned
 */
bool is_inarray(const char * key, int argc, char ** argv)
{
    int i;
    bool retval = false;

    for(i = 0; i < argc; i++)
    {
        if (0 == strcmp(key, argv[i]))
        {
            LOG(TRACE, "Found is_inarray()::argc=%d|i=%d|argv[i]=%s", argc, i, argv[i]);
            retval = true;
            break;
        }
    }

    return retval;
}

// count null terminated array of pointers
int count_nt_array(char **array)
{
    int count = 0;
    if (!array) return 0;
    while (*array)
    {
        array++;
        count++;
    }
    return count;
}

char* strfmt_nt_array(char *str, size_t size, char **array)
{
    *str = 0;
    strscpy(str, "[", size);
    while (array && *array)
    {
        if (str[1]) strlcat(str, ",", size);
        strlcat(str, *array, size);
        array++;
    }
    strlcat(str, "]", size);
    return str;
}

int filter_out_nt_array(char **array, char **filter)
{
    int f_count = count_nt_array(filter);
    int count = 0;
    char **src = array;
    char **dest = array;
    while (src && *src)
    {
        if (!is_inarray(*src, f_count, filter))
        {
            *dest = *src;
            dest++;
            count++;
        }
        src++;
    }
    *dest = NULL;
    return count;
}

// are all src[] entries in dest[]?
bool is_array_in_array(char **src, char **dest)
{
    int count = count_nt_array(dest);
    if (!src && !dest) return true;
    if (!src || !dest) return false;
    while (*src)
    {
        if (!is_inarray(*src, count, dest)) return false;
        src++;
    }
    return true;
}

char* str_bool(bool a)
{
    return a ? "true" : "false";
}

char* str_success(bool a)
{
    return a ? "success" : "failure";
}


void delimiter_append(char *dest, int size, char *src, int i, char d)
{
    if (i > 0)
    {
        int len = (int)strlen(dest);
        dest += len;
        size -= len;
        if (size <= 1) return;
        *dest = d;
        dest++;
        size--;
        if (size <= 1) return;
    }
    strscpy(dest, src, (size_t)size);
}


void comma_append(char *dest, int size, char *src, int i)
{
    delimiter_append(dest, size, src, i, ',');
}

void remove_character(char *str, const char character)
{
    char* i = str;
    char* j = str;
    while(*j != 0)
    {
        *i = *j++;
        if(*i != character)
          i++;
    }
    *i = 0;
}


// fsa: fixed size array[len][size] helper functions

int fsa_find_str(const void *array, int size, int len, const char *str)
{
    for (len--; len >= 0; len--)
        if (strcmp((const char*)array + len * size, str) == 0)
            return len;
    return -1;
}

void fsa_copy(const void *array, int size, int len, int num, void *dest, int dsize, int dlen, int *dnum)
{
    int i;
    for (i=0; i<num; i++)
    {
        if (i >= dlen) {
            LOG(CRIT, "FSA copy out of bounds %d >= %d", num, dlen);
            break;
        }
        const char *s = fsa_item(array, size, len, i);
        char *d = fsa_item(dest, dsize, dlen, i);
        strscpy(d, s, (size_t)dsize);
    }
    *dnum = i;
}


char *str_tolower(char *str)
{
    unsigned char *s = (unsigned char*)str;
    while (s && *s) { *s =(unsigned char) tolower(*s); s++; }
    return str;
}

char *str_toupper(char *str)
{
    unsigned char *s = (unsigned char*)str;
    while (s && *s) { *s = (unsigned char)toupper(*s); s++; }
    return str;
}

void create_onewifi_factory_reset_flag()
{
    FILE *fp=fopen(ONEWIFI_FR_FLAG, "a");
    if (fp != NULL) {
        fclose(fp);
    }
}

void remove_onewifi_factory_reset_flag()
{
    remove(ONEWIFI_FR_FLAG);
}

void remove_onewifi_migration_flag()
{
    remove(ONEWIFI_MIGRATION_FLAG);
}

/*
 * This function creates a file flag to reset wifidb data
 * when Factory Reset with reboot was executed
 */
void create_onewifi_factory_reset_reboot_flag() {
    FILE *fp=fopen(ONEWIFI_FR_REBOOT_FLAG, "a");
    if (fp != NULL) {
        fclose(fp);
    }
}

void remove_onewifi_factory_reset_reboot_flag() {
    remove(ONEWIFI_FR_REBOOT_FLAG);
}

void create_onewifi_fr_wifidb_reset_done_flag() {
    FILE *fp=fopen(ONEWIFI_FR_WIFIDB_RESET_DONE_FLAG, "a");
    if (fp != NULL) {
        fclose(fp);
    }
}

bool str_is_mac_address(const char *mac)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        if (!isxdigit(*mac++))
            return false;
        if (!isxdigit(*mac++))
            return false;
        if (i < 5 && *mac++ != ':')
            return false;
    }

    return true;
}

/*
 * [in] uri
 * [out] proto, host, port
 */
bool parse_uri(char *uri, char *proto, size_t proto_size, char *host, size_t host_size, int *port)
{
    // split
    char *sptr, *tproto, *thost, *pstr;
    int tport = 0;
    char tmp[UTIL_URI_MAX_LENG];

    if (!uri || uri[0] == '\0')
    {
        LOGE("URI empty");
        return false;
    }
    STRSCPY(tmp, uri);

    // Split the address up into it's pieces
    tproto = strtok_r(tmp, ":", &sptr);
    thost  = strtok_r(NULL, ":", &sptr);
    pstr  = strtok_r(NULL, ":", &sptr);
    if (pstr) tport = atoi(pstr);

    if (   strcmp(tproto, "ssl")
        && strcmp(tproto, "tcp"))
    {
        LOGE("URI %s proto not supported (Only ssl and tcp)", uri);
        return false;
    }

    if (  !thost
        || thost[0] == '\0'
        || tport <= 0
        )
    {
        LOGE("URI %s malformed (Host or port)", uri);
        return false;
    }
    else
    {
        strscpy(proto, tproto, proto_size);
        strscpy(host, thost, host_size);
        *port = tport;
    }

    return true;
}

/**
 * strscpy: safe string copy (using strnlen + memcpy)
 * Safe alternative to strncpy() as it always zero terminates the string.
 * Also safer than strlcpy in case of unterminated source string.
 *
 * Returns the length of copied string or a -E2BIG error if src len is
 * too big in which case the string is copied up to size-1 length
 * and is zero terminated.
 */
ssize_t strscpy(char *dest, const char *src, size_t size)
{
    size_t len;
    if (size == 0) return -E2BIG;
    len = strnlen(src, size - 1);
    memcpy(dest, src, len);
    dest[len] = 0;
    if (src[len]) return -E2BIG;
    return (ssize_t)len;
}

/**
 * strscpy_len copies a slice of src with length up to src_len
 * but never more than what fits in dest size.
 * If src_len is negative then it is an offset from the end of the src string.
 *
 * Returns the length of copied string or a -E2BIG error if src len is
 * too big in which case the string is copied up to size-1 length
 * and is zero terminated. In case the src_len is a negative offset
 * that is larger than the actual src length, it returns -EINVAL and
 * sets the dest buffer to empty string.
 */
ssize_t strscpy_len(char *dest, const char *src, size_t size, ssize_t src_len)
{
    size_t len;
    if (size == 0) return -E2BIG;
    // get actual src_len
    if (src_len < 0) {
        // using (size - str_len) to limit to size
        src_len =(ssize_t) strnlen(src, size -(size_t) src_len) + src_len;
        // check if offset is larger than actual src len
        if (src_len < 0) {
            *dest = 0;
            return -EINVAL;
        }
    } else {
        // limit to src_len
        src_len = (ssize_t)strnlen(src, (size_t)src_len);
    }
    len = (size_t)src_len;
    // trim if too big
    if (len > size - 1) len = size - 1;
    // copy the substring
    memcpy(dest, src, len);
    // zero terminate
    dest[len] = 0;
    // return error if src len was too big
    if ((size_t)src_len > len) return -E2BIG;
    // return len on success
    return (ssize_t)len;
}

ssize_t strscat(char *dest, const char *src, size_t size)
{
    if (size == 0) return -E2BIG;
    size_t dlen = strnlen(dest, size);
    size_t free = size - dlen;
    if (free == 0) return -E2BIG;
    ssize_t slen = strscpy(dest + dlen, src, free);
    if (slen < 0) return slen;
    return (ssize_t)dlen + slen;
}

char *strschr(const char *s, int c, size_t n)
{
    size_t len = strnlen(s, n);
    return memchr(s, c, len);
}

char *strsrchr(const char *s, int c, size_t n)
{
    size_t len = strnlen(s, n);
    return memrchr(s, c, len);
}

__attribute__ ((format(printf, 1, 2)))
char *strfmt(const char *fmt, ...)
{
    va_list ap;
    char c, *p;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(&c, 1, fmt, ap);
    va_end(ap);
    if (n >= 0 && (p = malloc((size_t)++n))) {
        va_start(ap, fmt);
        vsnprintf(p, (size_t)n, fmt, ap);
        va_end(ap);
        return p;
    }

    return NULL;
}

char *vstrfmt(const char *fmt, va_list args)
{
    va_list args_copy;
    char c, *p;
    int n;

    va_copy(args_copy, args);
    n = vsnprintf(&c, 1, fmt, args_copy);
    va_end(args_copy);

    if (n >= 0 && (p = malloc((size_t)++n))) {
        vsnprintf(p, (size_t)n, fmt, args);
        return p;
    }

    return NULL;
}

char *argvstr(const char *const*argv)
{
    char *q;
    int i;
    size_t n;
    if (!argv)
        return NULL;
    for (n=1, i=0; argv[i]; i++)
        n += strlen(argv[i]) + sizeof(',');
    if (!(q = calloc(1, n)))
        return NULL;
    for (i=0; argv[i]; i++)
        if (strscat(q, argv[i], n) >= 0 && argv[i+1])
            strscat(q, ",", n);
    return q;
}

char *strexread(const char *prog, const char *const*argv)
{
    const char *ctx = strfmta("%s(%s, [%s]", __func__, prog ?: "", argvstra(argv) ?: "");
    char **args, *p, *q, c;
    int fd[2], pid, status, i, j, n;
    if (!prog || !argv) {
        LOGW("%s: invalid arguments (prog=%p, argv=%p)", ctx, prog, argv);
        return NULL;
    }
    if (pipe(fd) < 0) {
        LOGW("%s: failed to pipe(): %d (%s)", ctx, errno, strerror(errno));
        return NULL;
    }
    switch ((pid = fork())) {
        case -1:
            LOGW("%s: failed to fork(): %d (%s)", ctx, errno, strerror(errno));
            close(fd[0]);
            close(fd[1]);
            return NULL;
        case 0:
            close(0);
            close(1);
            close(2);
            open("/dev/null", O_RDONLY);
            dup2(fd[1], 1);
            close(fd[0]);
            close(fd[1]);
            for (n=0; argv[n]; n++);
            args = calloc((size_t)++n, sizeof(args[0]));
            for (n=0; argv[n]; n++) args[n] = strdup(argv[n]);
            args[n] = 0;
            execvp(prog, args);
            LOGW("%s: failed to execvp(): %d (%s)", ctx, errno, strerror(errno));
            exit(1);
        default:
            close(fd[1]);
            for (n=0,i=0,p=0;;) {
                if (i+1 >= n) {
                    if ((q = realloc(p,(size_t)(n+=4096))))
                        p = q;
                    else
                        break;
                }
                if ((j = (int)read(fd[0], p+i, (size_t)(n-i-1))) <= 0)
                    break;
                i += j;
            }
            p[i] = 0;
            while (read(fd[0], &c, 1) == 1);
            close(fd[0]);
            waitpid(pid, &status, 0);
            LOGT("%s: status=%d output='%s'", ctx, status, p);
            if ((errno = (WIFEXITED(status) ? WEXITSTATUS(status) : -1)) == 0)
                return p;
            free(p);
            return NULL;
    }
    LOGW("%s: unreachable", ctx);
    return NULL;
}

char *strdel(char *heystack, const char *needle, int (*strcmp_fun) (const char*, const char*))
{
    const size_t heystack_size = strlen(heystack) + 1;
    char *p = strdupa(heystack ?: "");
    char *q = strdupa("");
    char *i;
    while ((i = strsep(&p, " ")))
        if (strcmp_fun(i, needle))
            q = strfmta("%s %s", i, q);
    strscpy(heystack, strchomp(q, " "), heystack_size);
    return heystack;
}

int str_count_lines(char *s)
{
    int count = 0;
    if (!s) return 0;
    while (*s) {
        count++;
        s = strchr(s, '\n');
        if (!s) break;
        s++;
    }
    return count;
}

// zero terminate each line in a block of text,
// store ptr to each line in lines array
// count is actual number of lines stored
// return false if size too small
bool str_split_lines_to(char *s, char **lines, int size, int *count)
{
    int i = 0;
    while (*s) {
        if (i >= size) return false;
        lines[i] = s;
        i++;
        *count = i;
        s = strchr(s, '\n');
        if (!s) break;
        *s = 0; // zero term
        s++;
    }
    return true;
}

// zero terminate each line in a block of text,
// allocate and store ptr to each line in lines array
// return lines array or NULL if empty or error allocating
char** str_split_lines(char *s, int *count)
{
    *count = 0;
    int num = str_count_lines(s);
    if (!num) return NULL;
    char **lines = calloc((size_t)num, sizeof(char*));
    if (!lines) return NULL;
    str_split_lines_to(s, lines, num, count);
    return lines;
}

// join a list of strings using delimiter
// return false if size too small
bool str_join(char *str, int size, char **list, int num, char *delim)
{
    char *p = str;
    size_t s = (size_t)size;
    int i, r;
    for (i=0; i<num; i++) {
        r = csnprintf(&p, &s, "%s%s", list[i], i < num - 1 ? delim : "");
        if (r < 0 || r > (int)s) return false;
    }
    return true;
}

// join a list of ints using delimiter
// return false if size too small
bool str_join_int(char *str, int size, int *list, int num, char *delim)
{
    char *p = str;
    size_t s = (size_t)size;
    int i, r;
    for (i=0; i<num; i++) {
        r = csnprintf(&p, &s, "%d%s", list[i], i < num - 1 ? delim : "");
        if (r < 0 || r > (int)s) return false;
    }
    return true;
}

/**
 * Returns true if string 'str' starts with string 'start'
 */
bool str_startswith(const char *str, const char *start)
{
    return strncmp(str, start, strlen(start)) == 0;
}

/**
 * Returns true if string 'str' ends with string 'end'
 */
bool str_endswith(const char *str, const char *end)
{
    int i =(int)(strlen(str) - strlen(end));
    if (i < 0) return false;
    return strcmp(str + i, end) == 0;
}

char *ini_get(const char *buf, const char *key)
{
    char *lines = strdupa(buf);
    char *line;
    const char *k;
    const char *v;
    while ((line = strsep(&lines, "\t\r\n")))
        if ((k = strsep(&line, "=")) &&
            (v = strsep(&line, "")) &&
            (!strcmp(k, key)))
            return strdup(v);
    return NULL;
}

int file_put(const char *path, const char *buf)
{
    ssize_t len = (ssize_t)strlen(buf);
    ssize_t n;
    int fd;
    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
        return -1;
    if ((n = write(fd, buf, (size_t)len)) != len)
        goto err_close;
    close(fd);
    LOGT("%s: wrote %-100s", path, buf);
    return 0;
err_close:
    close(fd);
    LOGT("%s: failed to write %-100s: %d (%s)", path, buf, errno, strerror(errno));
    return -1;
}

char *file_get(const char *path)
{
    ssize_t n;
    ssize_t size = 0;
    ssize_t len = 0;
    char *buf = NULL;
    char *nbuf = NULL;
    char *hunk[4096];
    int fd;
    if ((fd = open(path, O_RDONLY)) < 0)
        goto err;
    while ((n = read(fd, hunk, sizeof(hunk))) > 0) {
        if (!(nbuf = realloc(buf, (size_t)(size += n) + 1)))
            goto err_free;
        buf = nbuf;
        memcpy(buf + len, hunk, (size_t)n);
        len += n;
        buf[len] = 0;
    }
    if (n < 0)
        goto err_free;
    close(fd);
    LOGT("%s: read %-100s", path, (const char *)buf);
    return buf;
err_free:
    free(buf);
    close(fd);
err:
    LOGT("%s: failed to read: %d (%s)", path, errno, strerror(errno));
    return NULL;
}

const int *unii_5g_chan2list(int chan, int width)
{
    static const int lists[] = {
        /* <width>, <chan1>, <chan2>..., 0, */
        20, 36, 0,
        20, 40, 0,
        20, 44, 0,
        20, 48, 0,
        20, 52, 0,
        20, 56, 0,
        20, 60, 0,
        20, 64, 0,
        20, 100, 0,
        20, 104, 0,
        20, 108, 0,
        20, 112, 0,
        20, 116, 0,
        20, 120, 0,
        20, 124, 0,
        20, 128, 0,
        20, 132, 0,
        20, 136, 0,
        20, 140, 0,
        20, 144, 0,
        20, 149, 0,
        20, 153, 0,
        20, 157, 0,
        20, 161, 0,
        20, 165, 0,
        40, 36, 40, 0,
        40, 44, 48, 0,
        40, 52, 56, 0,
        40, 60, 64, 0,
        40, 100, 104, 0,
        40, 108, 112, 0,
        40, 116, 120, 0,
        40, 124, 128, 0,
        40, 132, 136, 0,
        40, 140, 144, 0,
        40, 149, 153, 0,
        40, 157, 161, 0,
        80, 36, 40, 44, 48, 0,
        80, 52, 56, 60, 64, 0,
        80, 100, 104, 108, 112, 0,
        80, 116, 120, 124, 128, 0,
        80, 132, 136, 140, 144, 0,
        80, 149, 153, 157, 161, 0,
        160, 36, 40, 44, 48, 52, 56, 60, 64, 0,
        160, 100, 104, 108, 112, 116, 120, 124, 128, 0,
        -1, /* keep last */
    };
    const int *start;
    const int *p;

    for (p = lists; *p != -1; p++)
        if (*p == width)
            for (start = ++p; *p; p++)
                if (*p == chan)
                    return start;

    return NULL;
}

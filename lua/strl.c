/* Written by Kaz Kylheku, assigned to public domain. */
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
*((char *) mempcpy (dst, src, n)) = '\0';

size_t lim_strcpy(char *dst, const char *src, size_t lim)
{
  size_t src_len = strlen(src);
  size_t copy_size = MIN(lim, src_len + 1);

  if (copy_size > 0) {
    memcpy(dst, src, copy_size);
    dst[copy_size - 1] = 0;
  }

  return src_len;
}

size_t lim_strcat(char *dst, const char *src, size_t lim)
{
  size_t dst_len = strlen(dst);

  if (lim > dst_len + 1)
    return dst_len + lim_strcpy(dst + dst_len, src, lim - dst_len);

  return dst_len + strlen(src);
}

/*
The routines in this file handle encryption of the "file" abstraction.

Two on-disk formats exist:

  #ME2.00$  (current, written by encrypt_buf)
      AES-256-GCM with a per-file random salt and scrypt key derivation.
      Layout:
         0        8            24            36           52
        +--------+-------------+-------------+------------+-- ~~ --+
        |#ME2.00$|  salt (16)  | nonce (12)  |  tag (16)  |  ct... |
        +--------+-------------+-------------+------------+-- ~~ --+
      The GCM tag authenticates both the ciphertext AND the header bytes
      [0..36) (magic|salt|nonce) which are fed as additional authenticated
      data, so tampering with any of them fails decryption.  There is no
      plaintext hash stored anywhere (unlike the legacy format).

  #ME1.42$  (legacy, decrypt-only -- see decrypt_legacy)
      Blowfish-CBC, password used directly as the key, and an MD5 of the
      *plaintext* stored in the clear.  Kept readable for old files; never
      written.  Opening such a file and saving it upgrades it to #ME2.00$.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ed.h"
#include "crypt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#if CRYPT_S

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>

#define BLOCKZ 8

/* ---- #ME2.00$ format geometry ---- */
#define V2_MAGLEN   8
#define V2_SALTLEN  16
#define V2_NONCELEN 12
#define V2_TAGLEN   16
#define V2_HDRLEN   (V2_MAGLEN + V2_SALTLEN + V2_NONCELEN + V2_TAGLEN) /* 52 */
#define V2_AADLEN   (V2_MAGLEN + V2_SALTLEN + V2_NONCELEN)             /* 36 */

/* scrypt work factors.  N*r*128 bytes of memory (~16 MiB here); tunable. */
#define SCRYPT_N    16384
#define SCRYPT_R    8
#define SCRYPT_P    1
#define SCRYPT_MAXMEM (64UL * 1024 * 1024)
#define V2_KEYLEN   32   /* AES-256 */

extern BYTE *md5string(BYTE *, long);

/* Derive a V2_KEYLEN key from password + salt via scrypt. Returns 1 on ok. */
static int
v2_derive_key(BYTE *pw, const BYTE *salt, BYTE *key_out)
{
    return EVP_PBE_scrypt((const char *)pw, strlen((char *)pw),
                          salt, V2_SALTLEN,
                          SCRYPT_N, SCRYPT_R, SCRYPT_P, SCRYPT_MAXMEM,
                          key_out, V2_KEYLEN) == 1;
}

/*
 * Encrypt *bz bytes of buf into a freshly malloc'd #ME2.00$ blob.
 * On return *bz is the blob length.  Returns NULL on failure.
 */
BYTE *
encrypt_buf(BYTE *pw, BYTE *buf, int *bz)
{
    BYTE  *out = NULL;
    BYTE   key[V2_KEYLEN];
    EVP_CIPHER_CTX *ctx = NULL;
    int    len, ok = 0;
    int    ptlen = *bz;

    out = (BYTE *)malloc(V2_HDRLEN + (size_t)ptlen);
    if (!out) return NULL;

    memcpy(out, ME_MAGIC2, V2_MAGLEN);
    if (RAND_bytes(out + V2_MAGLEN, V2_SALTLEN) != 1) goto done;             /* salt  */
    if (RAND_bytes(out + V2_MAGLEN + V2_SALTLEN, V2_NONCELEN) != 1) goto done; /* nonce */

    if (!v2_derive_key(pw, out + V2_MAGLEN, key)) goto done;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) goto done;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) goto done;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, V2_NONCELEN, NULL) != 1) goto done;
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key,
                           out + V2_MAGLEN + V2_SALTLEN) != 1) goto done;
    /* authenticate the header (magic|salt|nonce) as AAD */
    if (EVP_EncryptUpdate(ctx, NULL, &len, out, V2_AADLEN) != 1) goto done;
    if (EVP_EncryptUpdate(ctx, out + V2_HDRLEN, &len, buf, ptlen) != 1) goto done;
    if (EVP_EncryptFinal_ex(ctx, out + V2_HDRLEN + len, &len) != 1) goto done; /* GCM: len==0 */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, V2_TAGLEN,
                            out + V2_MAGLEN + V2_SALTLEN + V2_NONCELEN) != 1) goto done;

    *bz = V2_HDRLEN + ptlen;
    ok = 1;

done:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    memset(key, 0, sizeof(key));
    if (!ok) { free(out); return NULL; }
    return out;
}

/*
 * Decrypt a #ME2.00$ blob.  *bz is the blob length on entry, plaintext
 * length on success.  Returns NULL on any failure (wrong password, tampering,
 * truncation) -- GCM tag verification makes a wrong password indistinguishable
 * from corruption, which is the desired behavior.
 */
static BYTE *
decrypt_v2(BYTE *pw, BYTE *buf, long *bz)
{
    BYTE  *out = NULL;
    BYTE   key[V2_KEYLEN];
    EVP_CIPHER_CTX *ctx = NULL;
    int    len, ok = 0;
    long   ctlen;

    if (*bz < V2_HDRLEN) return NULL;
    ctlen = *bz - V2_HDRLEN;

    out = (BYTE *)malloc((size_t)ctlen + 1); /* +1 so a 0-length body still mallocs */
    if (!out) return NULL;

    if (!v2_derive_key(pw, buf + V2_MAGLEN, key)) goto done;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) goto done;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) goto done;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, V2_NONCELEN, NULL) != 1) goto done;
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key,
                           buf + V2_MAGLEN + V2_SALTLEN) != 1) goto done;
    if (EVP_DecryptUpdate(ctx, NULL, &len, buf, V2_AADLEN) != 1) goto done;   /* AAD */
    if (EVP_DecryptUpdate(ctx, out, &len, buf + V2_HDRLEN, ctlen) != 1) goto done;
    /* set expected tag, then Final verifies it */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, V2_TAGLEN,
                            buf + V2_MAGLEN + V2_SALTLEN + V2_NONCELEN) != 1) goto done;
    if (EVP_DecryptFinal_ex(ctx, out + len, &len) != 1) goto done; /* tag mismatch -> fail */

    *bz = ctlen;
    ok = 1;

done:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    memset(key, 0, sizeof(key));
    if (!ok) { free(out); return NULL; }
    return out;
}

/*
 * Legacy #ME1.42$ decryptor -- Blowfish-CBC, raw password key, plaintext MD5
 * stored in the header.  Read-only; retained so old files keep opening.
 */
static BYTE *
decrypt_legacy(BYTE *pw, BYTE *buf, long *bz)
{
    BYTE *nb;
    BYTE *computed_md5;
    BYTE stored_md5[16];
    int nbx, i, j, k, pad_size;
    Blowfish_Key kk;

    if (*bz < 48 || (*bz - 40) % BLOCKZ != 0) return NULL;

    nb = (BYTE *)malloc(*bz);
    if (!nb) return NULL;

    memcpy(stored_md5, &buf[8], 16);

    Blowfish_ExpandUserKey((BYTE *)pw, strlen((char *)pw), kk);

    nbx = 0;
    for (i = 40; i < *bz; i += BLOCKZ) {
        Blowfish_Decrypt(&buf[i], &nb[nbx], kk);
        for (j = 0, k = i - 8; j < BLOCKZ; j++, k++)
            nb[nbx++] ^= buf[k];
    }

    pad_size = nb[nbx - 1];
    if (pad_size >= BLOCKZ) {   /* valid range is 0..BLOCKZ-1 */
        free(nb);
        return NULL;
    }
    *bz = (nbx - BLOCKZ - pad_size);
    if (*bz <= 0) {
        free(nb);
        return NULL;
    }

    computed_md5 = md5string(nb, *bz);
    if (memcmp(computed_md5, stored_md5, 16) != 0) {
        free(nb);
        return NULL;
    }
    return nb;
}

/*
 * Dispatch on magic.  Caller has already confirmed the buffer starts with one
 * of our magics (see is_me_encrypted / do_read in file.c).
 */
BYTE *
decrypt_buf(BYTE *pw, BYTE *buf, long *bz)
{
    if (*bz >= V2_MAGLEN && memcmp(buf, ME_MAGIC2, V2_MAGLEN) == 0)
        return decrypt_v2(pw, buf, bz);
    return decrypt_legacy(pw, buf, bz);
}

#endif

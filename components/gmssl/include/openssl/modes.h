/*
 * Copyright 2008-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_MODE_H
#define HEADER_MODE_H

#include <internal/ayconfig.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef  __cplusplus
extern "C" {
#endif
typedef void (*block128_f) (const unsigned char in[16],
                            unsigned char out[16], const void *key);

typedef void (*cbc128_f) (const unsigned char *in, unsigned char *out,
                          size_t len, const void *key,
                          unsigned char ivec[16], int enc);

typedef void (*ctr128_f) (const unsigned char *in, unsigned char *out,
                          size_t blocks, const void *key,
                          const unsigned char ivec[16]);

typedef void (*ccm128_f) (const unsigned char *in, unsigned char *out,
                          size_t blocks, const void *key,
                          const unsigned char ivec[16],
                          unsigned char cmac[16]);

void ctr128_inc(unsigned char *counter);
void ctr128_dec(unsigned char *ctr_buf);
void ctr128_init(unsigned char nonce[4], unsigned char iv[8], unsigned char ctr_buf[16]);


void CRYPTO_cbc128_encrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16], block128_f block);
void CRYPTO_cbc128_decrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16], block128_f block);

void CRYPTO_ctr128_encrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16],
                           unsigned char ecount_buf[16], unsigned int *num,
                           block128_f block);

void CRYPTO_ctr128_encrypt_ctr32(const unsigned char *in, unsigned char *out,
                                 size_t len, const void *key,
                                 unsigned char ivec[16],
                                 unsigned char ecount_buf[16],
                                 unsigned int *num, ctr128_f ctr);

void CRYPTO_ofb128_encrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16], int *num,
                           block128_f block);

void CRYPTO_cfb128_encrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16], int *num,
                           int enc, block128_f block);
void CRYPTO_cfb128_8_encrypt(const unsigned char *in, unsigned char *out,
                             size_t length, const void *key,
                             unsigned char ivec[16], int *num,
                             int enc, block128_f block);
void CRYPTO_cfb128_1_encrypt(const unsigned char *in, unsigned char *out,
                             size_t bits, const void *key,
                             unsigned char ivec[16], int *num,
                             int enc, block128_f block);

size_t CRYPTO_cts128_encrypt_block(const unsigned char *in,
                                   unsigned char *out, size_t len,
                                   const void *key, unsigned char ivec[16],
                                   block128_f block);
size_t CRYPTO_cts128_encrypt(const unsigned char *in, unsigned char *out,
                             size_t len, const void *key,
                             unsigned char ivec[16], cbc128_f cbc);
size_t CRYPTO_cts128_decrypt_block(const unsigned char *in,
                                   unsigned char *out, size_t len,
                                   const void *key, unsigned char ivec[16],
                                   block128_f block);
size_t CRYPTO_cts128_decrypt(const unsigned char *in, unsigned char *out,
                             size_t len, const void *key,
                             unsigned char ivec[16], cbc128_f cbc);

size_t CRYPTO_nistcts128_encrypt_block(const unsigned char *in,
                                       unsigned char *out, size_t len,
                                       const void *key,
                                       unsigned char ivec[16],
                                       block128_f block);
size_t CRYPTO_nistcts128_encrypt(const unsigned char *in, unsigned char *out,
                                 size_t len, const void *key,
                                 unsigned char ivec[16], cbc128_f cbc);
size_t CRYPTO_nistcts128_decrypt_block(const unsigned char *in,
                                       unsigned char *out, size_t len,
                                       const void *key,
                                       unsigned char ivec[16],
                                       block128_f block);
size_t CRYPTO_nistcts128_decrypt(const unsigned char *in, unsigned char *out,
                                 size_t len, const void *key,
                                 unsigned char ivec[16], cbc128_f cbc);

typedef struct gcm128_context GCM128_CONTEXT;

GCM128_CONTEXT *CRYPTO_gcm128_new(void *key, block128_f block);
void CRYPTO_gcm128_init(GCM128_CONTEXT *ctx, void *key, block128_f block);
void CRYPTO_gcm128_setiv(GCM128_CONTEXT *ctx, const unsigned char *iv,
                         size_t len);
int CRYPTO_gcm128_aad(GCM128_CONTEXT *ctx, const unsigned char *aad,
                      size_t len);
int CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx,
                          const unsigned char *in, unsigned char *out,
                          size_t len);
int CRYPTO_gcm128_decrypt(GCM128_CONTEXT *ctx,
                          const unsigned char *in, unsigned char *out,
                          size_t len);
int CRYPTO_gcm128_encrypt_ctr32(GCM128_CONTEXT *ctx,
                                const unsigned char *in, unsigned char *out,
                                size_t len, ctr128_f stream);
int CRYPTO_gcm128_decrypt_ctr32(GCM128_CONTEXT *ctx,
                                const unsigned char *in, unsigned char *out,
                                size_t len, ctr128_f stream);
int CRYPTO_gcm128_finish(GCM128_CONTEXT *ctx, const unsigned char *tag,
                         size_t len);
void CRYPTO_gcm128_tag(GCM128_CONTEXT *ctx, unsigned char *tag, size_t len);
void CRYPTO_gcm128_release(GCM128_CONTEXT *ctx);

/*
*  Created on: 2017.10.18
*      Author: lzj
*
*  encrypted file format:
*
*  MAGIC_TAG  : AYCF-SM4-GCM
*  VERSION	  : 1
*  TAG		    : cipher tag
*  SIZE	      : plaintext size
*  CONTENT	  : cipher text
*
*	how to use?  see the test function under below .check out!
*/
#define GCM_FILE_MAGIC_TAG "AYCF-SM4-GCM"
#define GCM_FILE_MAGIC_TAG_LEN 12
#define GCM_FILE_VERSION 1
#define GCM_FILE_TAG_LEN 16
#define GCM_FILE_MAX_BLOCK_LEN 128

typedef struct {
		GCM128_CONTEXT *gcm;
		unsigned char tag[GCM_FILE_TAG_LEN];
} gcmf_context;

int gcmf_init(gcmf_context *ctx, void * key, block128_f block);
int gcmf_free(gcmf_context *ctx);
int gcmf_set_iv(gcmf_context *ctx, const unsigned char * iv, size_t len);
int gcmf_encrypt_file(gcmf_context * ctx, char *infpath, char *outfpath);
int gcmf_decrypt_file(gcmf_context * ctx, char *infpath, char *outfpath);

typedef struct ccm128_context CCM128_CONTEXT;

void CRYPTO_ccm128_init(CCM128_CONTEXT *ctx,
                        unsigned int M, unsigned int L, void *key,
                        block128_f block);
int CRYPTO_ccm128_setiv(CCM128_CONTEXT *ctx, const unsigned char *nonce,
                        size_t nlen, size_t mlen);
void CRYPTO_ccm128_aad(CCM128_CONTEXT *ctx, const unsigned char *aad,
                       size_t alen);
int CRYPTO_ccm128_encrypt(CCM128_CONTEXT *ctx, const unsigned char *inp,
                          unsigned char *out, size_t len);
int CRYPTO_ccm128_decrypt(CCM128_CONTEXT *ctx, const unsigned char *inp,
                          unsigned char *out, size_t len);
int CRYPTO_ccm128_encrypt_ccm64(CCM128_CONTEXT *ctx, const unsigned char *inp,
                                unsigned char *out, size_t len,
                                ccm128_f stream);
int CRYPTO_ccm128_decrypt_ccm64(CCM128_CONTEXT *ctx, const unsigned char *inp,
                                unsigned char *out, size_t len,
                                ccm128_f stream);
size_t CRYPTO_ccm128_tag(CCM128_CONTEXT *ctx, unsigned char *tag, size_t len);

typedef struct xts128_context XTS128_CONTEXT;

int CRYPTO_xts128_encrypt(const XTS128_CONTEXT *ctx,
                          const unsigned char iv[16],
                          const unsigned char *inp, unsigned char *out,
                          size_t len, int enc);

size_t CRYPTO_128_wrap(void *key, const unsigned char *iv,
                       unsigned char *out,
                       const unsigned char *in, size_t inlen,
                       block128_f block);

size_t CRYPTO_128_unwrap(void *key, const unsigned char *iv,
                         unsigned char *out,
                         const unsigned char *in, size_t inlen,
                         block128_f block);
size_t CRYPTO_128_wrap_pad(void *key, const unsigned char *icv,
                           unsigned char *out, const unsigned char *in,
                           size_t inlen, block128_f block);
size_t CRYPTO_128_unwrap_pad(void *key, const unsigned char *icv,
                             unsigned char *out, const unsigned char *in,
                             size_t inlen, block128_f block);

#ifndef OPENSSL_NO_OCB
typedef struct ocb128_context OCB128_CONTEXT;

typedef void (*ocb128_f) (const unsigned char *in, unsigned char *out,
                          size_t blocks, const void *key,
                          size_t start_block_num,
                          unsigned char offset_i[16],
                          const unsigned char L_[][16],
                          unsigned char checksum[16]);

OCB128_CONTEXT *CRYPTO_ocb128_new(void *keyenc, void *keydec,
                                  block128_f encrypt, block128_f decrypt,
                                  ocb128_f stream);
int CRYPTO_ocb128_init(OCB128_CONTEXT *ctx, void *keyenc, void *keydec,
                       block128_f encrypt, block128_f decrypt,
                       ocb128_f stream);
int CRYPTO_ocb128_copy_ctx(OCB128_CONTEXT *dest, OCB128_CONTEXT *src,
                           void *keyenc, void *keydec);
int CRYPTO_ocb128_setiv(OCB128_CONTEXT *ctx, const unsigned char *iv,
                        size_t len, size_t taglen);
int CRYPTO_ocb128_aad(OCB128_CONTEXT *ctx, const unsigned char *aad,
                      size_t len);
int CRYPTO_ocb128_encrypt(OCB128_CONTEXT *ctx, const unsigned char *in,
                          unsigned char *out, size_t len);
int CRYPTO_ocb128_decrypt(OCB128_CONTEXT *ctx, const unsigned char *in,
                          unsigned char *out, size_t len);
int CRYPTO_ocb128_finish(OCB128_CONTEXT *ctx, const unsigned char *tag,
                         size_t len);
int CRYPTO_ocb128_tag(OCB128_CONTEXT *ctx, unsigned char *tag, size_t len);
void CRYPTO_ocb128_cleanup(OCB128_CONTEXT *ctx);
#endif                          /* OPENSSL_NO_OCB */

#ifdef  __cplusplus
}
#endif


#ifndef AISINOSSL_MODES_LCL
#define AISINOSSL_MODES_LCL

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
typedef __int64 i64;
typedef unsigned __int64 u64;
# define U64(C) C##UI64
#elif defined(__arch64__)
typedef long i64;
typedef unsigned long u64;
# define U64(C) C##UL
#else
typedef long long i64;
typedef unsigned long long u64;
# define U64(C) C##ULL
#endif

typedef unsigned int u32;
typedef unsigned char u8;

#define STRICT_ALIGNMENT 1
#ifndef PEDANTIC
# if defined(__i386)    || defined(__i386__)    || \
     defined(__x86_64)  || defined(__x86_64__)  || \
     defined(_M_IX86)   || defined(_M_AMD64)    || defined(_M_X64) || \
     defined(__aarch64__)                       || \
     defined(__s390__)  || defined(__s390x__)
#  undef STRICT_ALIGNMENT
# endif
#endif

#if !defined(PEDANTIC) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM)
# if defined(__GNUC__) && __GNUC__>=2
#  if defined(__x86_64) || defined(__x86_64__)
#   define BSWAP8(x) ({ u64 ret_=(x);                   \
                        __asm__ ("bswapq %0  "              \
                        : "+r"(ret_));   ret_;          })
#   define BSWAP4(x) ({ u32 ret_=(x);                   \
                        __asm__ ("bswapl %0 "               \
                        : "+r"(ret_));   ret_;          })
#  elif (defined(__i386) || defined(__i386__)) && !defined(I386_ONLY)
#   define BSWAP8(x) ({ u32 lo_=(u64)(x)>>32,hi_=(x);   \
                        asm ("bswapl %0; bswapl %1"     \
                        : "+r"(hi_),"+r"(lo_));         \
                        (u64)hi_<<32|lo_;               })
#   define BSWAP4(x) ({ u32 ret_=(x);                   \
                        asm ("bswapl %0"                \
                        : "+r"(ret_));   ret_;          })
#  elif defined(__aarch64__)
#   define BSWAP8(x) ({ u64 ret_;                       \
                        asm ("rev %0,%1"                \
                        : "=r"(ret_) : "r"(x)); ret_;   })
#   define BSWAP4(x) ({ u32 ret_;                       \
                        asm ("rev %w0,%w1"              \
                        : "=r"(ret_) : "r"(x)); ret_;   })
#  elif (defined(__arm__) || defined(__arm)) && !defined(STRICT_ALIGNMENT)
#   define BSWAP8(x) ({ u32 lo_=(u64)(x)>>32,hi_=(x);   \
                        asm ("rev %0,%0; rev %1,%1"     \
                        : "+r"(hi_),"+r"(lo_));         \
                        (u64)hi_<<32|lo_;               })
#   define BSWAP4(x) ({ u32 ret_;                       \
                        asm ("rev %0,%1"                \
                        : "=r"(ret_) : "r"((u32)(x)));  \
                        ret_;                           })
#  endif
# elif defined(_MSC_VER)
#  if _MSC_VER>=1300
#   pragma intrinsic(_byteswap_uint64,_byteswap_ulong)
#   define BSWAP8(x)    _byteswap_uint64((u64)(x))
#   define BSWAP4(x)    _byteswap_ulong((u32)(x))
#  elif defined(_M_IX86)
__inline u32 _bswap4(u32 val)
{
	_asm mov eax, val _asm bswap eax
}
#   define BSWAP4(x)    _bswap4(x)
#  endif
# endif
#endif
#if defined(BSWAP4) && !defined(STRICT_ALIGNMENT)
# define GETU32(p)       BSWAP4(*(const u32 *)(p))
# define PUTU32(p,v)     *(u32 *)(p) = BSWAP4(v)
#else
# define GETU32(p)       ((u32)(p)[0]<<24|(u32)(p)[1]<<16|(u32)(p)[2]<<8|(u32)(p)[3])
# define PUTU32(p,v)     ((p)[0]=(u8)((v)>>24),(p)[1]=(u8)((v)>>16),(p)[2]=(u8)((v)>>8),(p)[3]=(u8)(v))
#endif
/*- GCM definitions */ typedef struct {
	u64 hi, lo;
} u128;

#ifdef  TABLE_BITS
# undef  TABLE_BITS
#endif
/*
* Even though permitted values for TABLE_BITS are 8, 4 and 1, it should
* never be set to 8 [or 1]. For further information see gcm128.c.
*/
#define TABLE_BITS 4

struct gcm128_context {
	/* Following 6 names follow names in GCM specification */
	union {
		u64 u[2];
		u32 d[4];
		u8 c[16];
		size_t t[16 / sizeof(size_t)];
	} Yi, EKi, EK0, len, Xi, H;
	/*
	* Relative position of Xi, H and pre-computed Htable is used in some
	* assembler modules, i.e. don't change the order!
	*/
#if TABLE_BITS==8
	u128 Htable[256];
#else
	u128 Htable[16];
	void(*gmult) (u64 Xi[2], const u128 Htable[16]);
	void(*ghash) (u64 Xi[2], const u128 Htable[16], const u8 *inp,
		size_t len);
#endif
	unsigned int mres, ares;
	block128_f block;
	void *key;
};

struct xts128_context {
	void *key1, *key2;
	block128_f block1, block2;
};

struct ccm128_context {
	union {
		u64 u[2];
		u8 c[16];
	} nonce, cmac;
	u64 blocks;
	block128_f block;
	void *key;
};

#ifndef OPENSSL_NO_OCB

typedef union {
	u64 a[2];
	unsigned char c[16];
} OCB_BLOCK;
# define ocb_block16_xor(in1,in2,out) \
    ( (out)->a[0]=(in1)->a[0]^(in2)->a[0], \
      (out)->a[1]=(in1)->a[1]^(in2)->a[1] )
# if STRICT_ALIGNMENT
#  define ocb_block16_xor_misaligned(in1,in2,out) \
    ocb_block_xor((in1)->c,(in2)->c,16,(out)->c)
# else
#  define ocb_block16_xor_misaligned ocb_block16_xor
# endif

struct ocb128_context {
	/* Need both encrypt and decrypt key schedules for decryption */
	block128_f encrypt;
	block128_f decrypt;
	void *keyenc;
	void *keydec;
	ocb128_f stream;    /* direction dependent */
						/* Key dependent variables. Can be reused if key remains the same */
	size_t l_index;
	size_t max_l_index;
	OCB_BLOCK l_star;
	OCB_BLOCK l_dollar;
	OCB_BLOCK *l;
	/* Must be reset for each session */
	u64 blocks_hashed;
	u64 blocks_processed;
	OCB_BLOCK tag;
	OCB_BLOCK offset_aad;
	OCB_BLOCK sum;
	OCB_BLOCK offset;
	OCB_BLOCK checksum;
};
#endif                          /* OPENSSL_NO_OCB */

#endif /* AISINOSSL_MODES_LCL */

#endif

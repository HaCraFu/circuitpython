/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Paul Sokolovsky
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "py/runtime.h"
#include "py/binary.h"

#include "supervisor/shared/translate/translate.h"

#if MICROPY_PY_UBINASCII

static void check_not_unicode(const mp_obj_t arg) {
    #if MICROPY_CPYTHON_COMPAT
    if (mp_obj_is_str(arg)) {
        mp_raise_TypeError(MP_ERROR_TEXT("a bytes-like object is required"));
    }
    #endif
}
STATIC mp_obj_t mod_binascii_hexlify(size_t n_args, const mp_obj_t *args) {
    // First argument is the data to convert.
    // Second argument is an optional separator to be used between values.
    const char *sep = NULL;
    mp_buffer_info_t bufinfo;
    check_not_unicode(args[0]);
    mp_get_buffer_raise(args[0], &bufinfo, MP_BUFFER_READ);

    // Code below assumes non-zero buffer length when computing size with
    // separator, so handle the zero-length case here.
    if (bufinfo.len == 0) {
        return mp_const_empty_bytes;
    }

    vstr_t vstr;
    size_t out_len = bufinfo.len * 2;
    if (n_args > 1) {
        // 1-char separator between hex numbers
        out_len += bufinfo.len - 1;
        sep = mp_obj_str_get_str(args[1]);
    }
    vstr_init_len(&vstr, out_len);
    byte *in = bufinfo.buf, *out = (byte *)vstr.buf;
    for (mp_uint_t i = bufinfo.len; i--;) {
        byte d = (*in >> 4);
        if (d > 9) {
            d += 'a' - '9' - 1;
        }
        *out++ = d + '0';
        d = (*in++ & 0xf);
        if (d > 9) {
            d += 'a' - '9' - 1;
        }
        *out++ = d + '0';
        if (sep != NULL && i != 0) {
            *out++ = *sep;
        }
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_binascii_hexlify_obj, 1, 2, mod_binascii_hexlify);

STATIC mp_obj_t mod_binascii_unhexlify(mp_obj_t data) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);

    if ((bufinfo.len & 1) != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("odd-length string"));
    }
    vstr_t vstr;
    vstr_init_len(&vstr, bufinfo.len / 2);
    byte *in = bufinfo.buf, *out = (byte *)vstr.buf;
    byte hex_byte = 0;
    for (mp_uint_t i = bufinfo.len; i--;) {
        byte hex_ch = *in++;
        if (unichar_isxdigit(hex_ch)) {
            hex_byte += unichar_xdigit_value(hex_ch);
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("non-hex digit found"));
        }
        if (i & 1) {
            hex_byte <<= 4;
        } else {
            *out++ = hex_byte;
            hex_byte = 0;
        }
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_binascii_unhexlify_obj, mod_binascii_unhexlify);

// If ch is a character in the base64 alphabet, and is not a pad character, then
// the corresponding integer between 0 and 63, inclusively, is returned.
// Otherwise, -1 is returned.
static int mod_binascii_sextet(byte ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    } else if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 26;
    } else if (ch >= '0' && ch <= '9') {
        return ch - '0' + 52;
    } else if (ch == '+') {
        return 62;
    } else if (ch == '/') {
        return 63;
    } else {
        return -1;
    }
}

STATIC mp_obj_t mod_binascii_a2b_base64(mp_obj_t data) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    byte *in = bufinfo.buf;

    vstr_t vstr;
    vstr_init(&vstr, (bufinfo.len / 4) * 3 + 1); // Potentially over-allocate
    byte *out = (byte *)vstr.buf;

    uint shift = 0;
    int nbits = 0; // Number of meaningful bits in shift
    bool hadpad = false; // Had a pad character since last valid character
    for (size_t i = 0; i < bufinfo.len; i++) {
        if (in[i] == '=') {
            if ((nbits == 2) || ((nbits == 4) && hadpad)) {
                nbits = 0;
                break;
            }
            hadpad = true;
        }

        int sextet = mod_binascii_sextet(in[i]);
        if (sextet == -1) {
            continue;
        }
        hadpad = false;
        shift = (shift << 6) | sextet;
        nbits += 6;

        if (nbits >= 8) {
            nbits -= 8;
            out[vstr.len++] = (shift >> nbits) & 0xFF;
        }
    }

    if (nbits) {
        mp_raise_ValueError(MP_ERROR_TEXT("incorrect padding"));
    }

    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_binascii_a2b_base64_obj, mod_binascii_a2b_base64);

STATIC mp_obj_t mod_binascii_b2a_base64(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_newline };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_newline, MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    uint8_t newline = args[ARG_newline].u_bool;
    check_not_unicode(pos_args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(pos_args[0], &bufinfo, MP_BUFFER_READ);

    vstr_t vstr;
    vstr_init_len(&vstr, ((bufinfo.len != 0) ? (((bufinfo.len - 1) / 3) + 1) * 4 : 0) + newline);

    // First pass, we convert input buffer to numeric base 64 values
    byte *in = bufinfo.buf, *out = (byte *)vstr.buf;
    mp_uint_t i;
    for (i = bufinfo.len; i >= 3; i -= 3) {
        *out++ = (in[0] & 0xFC) >> 2;
        *out++ = (in[0] & 0x03) << 4 | (in[1] & 0xF0) >> 4;
        *out++ = (in[1] & 0x0F) << 2 | (in[2] & 0xC0) >> 6;
        *out++ = in[2] & 0x3F;
        in += 3;
    }
    if (i != 0) {
        *out++ = (in[0] & 0xFC) >> 2;
        if (i == 2) {
            *out++ = (in[0] & 0x03) << 4 | (in[1] & 0xF0) >> 4;
            *out++ = (in[1] & 0x0F) << 2;
        } else {
            *out++ = (in[0] & 0x03) << 4;
            *out++ = 64;
        }
        *out = 64;
    }

    // Second pass, we convert number base 64 values to actual base64 ascii encoding
    out = (byte *)vstr.buf;
    for (mp_uint_t j = vstr.len - newline; j--;) {
        if (*out < 26) {
            *out += 'A';
        } else if (*out < 52) {
            *out += 'a' - 26;
        } else if (*out < 62) {
            *out += '0' - 52;
        } else if (*out == 62) {
            *out = '+';
        } else if (*out == 63) {
            *out = '/';
        } else {
            *out = '=';
        }
        out++;
    }
    if (newline) {
        *out = '\n';
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_binascii_b2a_base64_obj, 1, mod_binascii_b2a_base64);

// CIRCUITPY uses a self-contained implementation of CRC32,
// instead of depending on uzlib, like MicroPython.

/*
 * CRC32 checksum
 *
 * Copyright (c) 1998-2003 by Joergen Ibsen / Jibz
 * All Rights Reserved
 *
 * http://www.ibsensoftware.com/
 *
 * This software is provided 'as-is', without any express
 * or implied warranty.  In no event will the authors be
 * held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be
 *    misrepresented; you must not claim that you
 *    wrote the original software. If you use this
 *    software in a product, an acknowledgment in
 *    the product documentation would be appreciated
 *    but is not required.
 *
 * 2. Altered source versions must be plainly marked
 *    as such, and must not be misrepresented as
 *    being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */

/*
 * CRC32 algorithm taken from the zlib source, which is
 * Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler
 */


static const unsigned int tinf_crc32tab[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190,
    0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344,
    0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278,
    0xbdbdf21c
};

/* crc is previous value for incremental computation, 0xffffffff initially */
static uint32_t from_uzlib_crc32(const void *data, unsigned int length, uint32_t crc) {
    const unsigned char *buf = (const unsigned char *)data;
    unsigned int i;

    for (i = 0; i < length; ++i)
    {
        crc ^= buf[i];
        crc = tinf_crc32tab[crc & 0x0f] ^ (crc >> 4);
        crc = tinf_crc32tab[crc & 0x0f] ^ (crc >> 4);
    }

    // return value suitable for passing in next time, for final value invert it
    return crc /* ^ 0xffffffff*/;
}

STATIC mp_obj_t mod_binascii_crc32(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t bufinfo;
    check_not_unicode(args[0]);
    mp_get_buffer_raise(args[0], &bufinfo, MP_BUFFER_READ);
    uint32_t crc = (n_args > 1) ? mp_obj_get_int_truncated(args[1]) : 0;
    crc = from_uzlib_crc32(bufinfo.buf, bufinfo.len, crc ^ 0xffffffff);
    return mp_obj_new_int_from_uint(crc ^ 0xffffffff);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_binascii_crc32_obj, 1, 2, mod_binascii_crc32);


STATIC const mp_rom_map_elem_t mp_module_binascii_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_binascii) },
    { MP_ROM_QSTR(MP_QSTR_hexlify), MP_ROM_PTR(&mod_binascii_hexlify_obj) },
    { MP_ROM_QSTR(MP_QSTR_unhexlify), MP_ROM_PTR(&mod_binascii_unhexlify_obj) },
    { MP_ROM_QSTR(MP_QSTR_a2b_base64), MP_ROM_PTR(&mod_binascii_a2b_base64_obj) },
    { MP_ROM_QSTR(MP_QSTR_b2a_base64), MP_ROM_PTR(&mod_binascii_b2a_base64_obj) },
    { MP_ROM_QSTR(MP_QSTR_crc32), MP_ROM_PTR(&mod_binascii_crc32_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_binascii_globals, mp_module_binascii_globals_table);

const mp_obj_module_t mp_module_ubinascii = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_binascii_globals,
};

MP_REGISTER_MODULE(MP_QSTR_binascii, mp_module_ubinascii);

#endif // MICROPY_PY_UBINASCII

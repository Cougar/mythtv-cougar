/*
 * Constants for DV codec
 * Copyright (c) 2002 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NB_DV_VLC 409
#define AAUX_OFFSET (80*6 + 80*16*3 + 3)

static const uint16_t dv_vlc_bits[409] = {
 0x0000, 0x0002, 0x0007, 0x0008, 0x0009, 0x0014, 0x0015, 0x0016,
 0x0017, 0x0030, 0x0031, 0x0032, 0x0033, 0x0068, 0x0069, 0x006a,
 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x00e0, 0x00e1, 0x00e2,
 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea,
 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x01e0, 0x01e1, 0x01e2,
 0x01e3, 0x01e4, 0x01e5, 0x01e6, 0x01e7, 0x01e8, 0x01e9, 0x01ea,
 0x01eb, 0x01ec, 0x01ed, 0x01ee, 0x01ef, 0x03e0, 0x03e1, 0x03e2,
 0x03e3, 0x03e4, 0x03e5, 0x03e6, 0x07ce, 0x07cf, 0x07d0, 0x07d1,
 0x07d2, 0x07d3, 0x07d4, 0x07d5, 0x0fac, 0x0fad, 0x0fae, 0x0faf,
 0x0fb0, 0x0fb1, 0x0fb2, 0x0fb3, 0x0fb4, 0x0fb5, 0x0fb6, 0x0fb7,
 0x0fb8, 0x0fb9, 0x0fba, 0x0fbb, 0x0fbc, 0x0fbd, 0x0fbe, 0x0fbf,
 0x1f80, 0x1f81, 0x1f82, 0x1f83, 0x1f84, 0x1f85, 0x1f86, 0x1f87,
 0x1f88, 0x1f89, 0x1f8a, 0x1f8b, 0x1f8c, 0x1f8d, 0x1f8e, 0x1f8f,
 0x1f90, 0x1f91, 0x1f92, 0x1f93, 0x1f94, 0x1f95, 0x1f96, 0x1f97,
 0x1f98, 0x1f99, 0x1f9a, 0x1f9b, 0x1f9c, 0x1f9d, 0x1f9e, 0x1f9f,
 0x1fa0, 0x1fa1, 0x1fa2, 0x1fa3, 0x1fa4, 0x1fa5, 0x1fa6, 0x1fa7,
 0x1fa8, 0x1fa9, 0x1faa, 0x1fab, 0x1fac, 0x1fad, 0x1fae, 0x1faf,
 0x1fb0, 0x1fb1, 0x1fb2, 0x1fb3, 0x1fb4, 0x1fb5, 0x1fb6, 0x1fb7,
 0x1fb8, 0x1fb9, 0x1fba, 0x1fbb, 0x1fbc, 0x1fbd, 0x1fbe, 0x1fbf,
 0x7f00, 0x7f01, 0x7f02, 0x7f03, 0x7f04, 0x7f05, 0x7f06, 0x7f07,
 0x7f08, 0x7f09, 0x7f0a, 0x7f0b, 0x7f0c, 0x7f0d, 0x7f0e, 0x7f0f,
 0x7f10, 0x7f11, 0x7f12, 0x7f13, 0x7f14, 0x7f15, 0x7f16, 0x7f17,
 0x7f18, 0x7f19, 0x7f1a, 0x7f1b, 0x7f1c, 0x7f1d, 0x7f1e, 0x7f1f,
 0x7f20, 0x7f21, 0x7f22, 0x7f23, 0x7f24, 0x7f25, 0x7f26, 0x7f27,
 0x7f28, 0x7f29, 0x7f2a, 0x7f2b, 0x7f2c, 0x7f2d, 0x7f2e, 0x7f2f,
 0x7f30, 0x7f31, 0x7f32, 0x7f33, 0x7f34, 0x7f35, 0x7f36, 0x7f37,
 0x7f38, 0x7f39, 0x7f3a, 0x7f3b, 0x7f3c, 0x7f3d, 0x7f3e, 0x7f3f,
 0x7f40, 0x7f41, 0x7f42, 0x7f43, 0x7f44, 0x7f45, 0x7f46, 0x7f47,
 0x7f48, 0x7f49, 0x7f4a, 0x7f4b, 0x7f4c, 0x7f4d, 0x7f4e, 0x7f4f,
 0x7f50, 0x7f51, 0x7f52, 0x7f53, 0x7f54, 0x7f55, 0x7f56, 0x7f57,
 0x7f58, 0x7f59, 0x7f5a, 0x7f5b, 0x7f5c, 0x7f5d, 0x7f5e, 0x7f5f,
 0x7f60, 0x7f61, 0x7f62, 0x7f63, 0x7f64, 0x7f65, 0x7f66, 0x7f67,
 0x7f68, 0x7f69, 0x7f6a, 0x7f6b, 0x7f6c, 0x7f6d, 0x7f6e, 0x7f6f,
 0x7f70, 0x7f71, 0x7f72, 0x7f73, 0x7f74, 0x7f75, 0x7f76, 0x7f77,
 0x7f78, 0x7f79, 0x7f7a, 0x7f7b, 0x7f7c, 0x7f7d, 0x7f7e, 0x7f7f,
 0x7f80, 0x7f81, 0x7f82, 0x7f83, 0x7f84, 0x7f85, 0x7f86, 0x7f87,
 0x7f88, 0x7f89, 0x7f8a, 0x7f8b, 0x7f8c, 0x7f8d, 0x7f8e, 0x7f8f,
 0x7f90, 0x7f91, 0x7f92, 0x7f93, 0x7f94, 0x7f95, 0x7f96, 0x7f97,
 0x7f98, 0x7f99, 0x7f9a, 0x7f9b, 0x7f9c, 0x7f9d, 0x7f9e, 0x7f9f,
 0x7fa0, 0x7fa1, 0x7fa2, 0x7fa3, 0x7fa4, 0x7fa5, 0x7fa6, 0x7fa7,
 0x7fa8, 0x7fa9, 0x7faa, 0x7fab, 0x7fac, 0x7fad, 0x7fae, 0x7faf,
 0x7fb0, 0x7fb1, 0x7fb2, 0x7fb3, 0x7fb4, 0x7fb5, 0x7fb6, 0x7fb7,
 0x7fb8, 0x7fb9, 0x7fba, 0x7fbb, 0x7fbc, 0x7fbd, 0x7fbe, 0x7fbf,
 0x7fc0, 0x7fc1, 0x7fc2, 0x7fc3, 0x7fc4, 0x7fc5, 0x7fc6, 0x7fc7,
 0x7fc8, 0x7fc9, 0x7fca, 0x7fcb, 0x7fcc, 0x7fcd, 0x7fce, 0x7fcf,
 0x7fd0, 0x7fd1, 0x7fd2, 0x7fd3, 0x7fd4, 0x7fd5, 0x7fd6, 0x7fd7,
 0x7fd8, 0x7fd9, 0x7fda, 0x7fdb, 0x7fdc, 0x7fdd, 0x7fde, 0x7fdf,
 0x7fe0, 0x7fe1, 0x7fe2, 0x7fe3, 0x7fe4, 0x7fe5, 0x7fe6, 0x7fe7,
 0x7fe8, 0x7fe9, 0x7fea, 0x7feb, 0x7fec, 0x7fed, 0x7fee, 0x7fef,
 0x7ff0, 0x7ff1, 0x7ff2, 0x7ff3, 0x7ff4, 0x7ff5, 0x7ff6, 0x7ff7,
 0x7ff8, 0x7ff9, 0x7ffa, 0x7ffb, 0x7ffc, 0x7ffd, 0x7ffe, 0x7fff,
 0x0006,
};

static const uint8_t dv_vlc_len[409] = {
  2,  3,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,
  7,  7,  7,  7,  7,  8,  8,  8,
  8,  8,  8,  8,  8,  8,  8,  8,
  8,  8,  8,  8,  8,  9,  9,  9,
  9,  9,  9,  9,  9,  9,  9,  9,
  9,  9,  9,  9,  9, 10, 10, 10,
 10, 10, 10, 10, 11, 11, 11, 11,
 11, 11, 11, 11, 12, 12, 12, 12,
 12, 12, 12, 12, 12, 12, 12, 12,
 12, 12, 12, 12, 12, 12, 12, 12,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 13, 13, 13, 13, 13, 13, 13, 13,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
 15, 15, 15, 15, 15, 15, 15, 15,
  4,
};

static const uint8_t dv_vlc_run[409] = {
  0,  0,  1,  0,  0,  2,  1,  0,
  0,  3,  4,  0,  0,  5,  6,  2,
  1,  1,  0,  0,  0,  7,  8,  9,
 10,  3,  4,  2,  1,  1,  1,  0,
  0,  0,  0,  0,  0, 11, 12, 13,
 14,  5,  6,  3,  4,  2,  2,  1,
  0,  0,  0,  0,  0,  5,  3,  3,
  2,  1,  1,  1,  0,  1,  6,  4,
  3,  1,  1,  1,  2,  3,  4,  5,
  7,  8,  9, 10,  7,  8,  4,  3,
  2,  2,  2,  2,  2,  1,  1,  1,
  0,  1,  2,  3,  4,  5,  6,  7,
  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23,
 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39,
 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55,
 56, 57, 58, 59, 60, 61, 62, 63,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,
};

static const uint8_t dv_vlc_level[409] = {
  1,  2,  1,  3,  4,  1,  2,  5,
  6,  1,  1,  7,  8,  1,  1,  2,
  3,  4,  9, 10, 11,  1,  1,  1,
  1,  2,  2,  3,  5,  6,  7, 12,
 13, 14, 15, 16, 17,  1,  1,  1,
  1,  2,  2,  3,  3,  4,  5,  8,
 18, 19, 20, 21, 22,  3,  4,  5,
  6,  9, 10, 11,  0,  0,  3,  4,
  6, 12, 13, 14,  0,  0,  0,  0,
  2,  2,  2,  2,  3,  3,  5,  7,
  7,  8,  9, 10, 11, 15, 16, 17,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  1,  2,  3,  4,  5,  6,  7,
  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23,
 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39,
 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55,
 56, 57, 58, 59, 60, 61, 62, 63,
 64, 65, 66, 67, 68, 69, 70, 71,
 72, 73, 74, 75, 76, 77, 78, 79,
 80, 81, 82, 83, 84, 85, 86, 87,
 88, 89, 90, 91, 92, 93, 94, 95,
 96, 97, 98, 99, 100, 101, 102, 103,
 104, 105, 106, 107, 108, 109, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119,
 120, 121, 122, 123, 124, 125, 126, 127,
 128, 129, 130, 131, 132, 133, 134, 135,
 136, 137, 138, 139, 140, 141, 142, 143,
 144, 145, 146, 147, 148, 149, 150, 151,
 152, 153, 154, 155, 156, 157, 158, 159,
 160, 161, 162, 163, 164, 165, 166, 167,
 168, 169, 170, 171, 172, 173, 174, 175,
 176, 177, 178, 179, 180, 181, 182, 183,
 184, 185, 186, 187, 188, 189, 190, 191,
 192, 193, 194, 195, 196, 197, 198, 199,
 200, 201, 202, 203, 204, 205, 206, 207,
 208, 209, 210, 211, 212, 213, 214, 215,
 216, 217, 218, 219, 220, 221, 222, 223,
 224, 225, 226, 227, 228, 229, 230, 231,
 232, 233, 234, 235, 236, 237, 238, 239,
 240, 241, 242, 243, 244, 245, 246, 247,
 248, 249, 250, 251, 252, 253, 254, 255,
  0,
};

/* Specific zigzag scan for 248 idct. NOTE that unlike the
   specification, we interleave the fields */
static const uint8_t dv_248_zigzag[64] = {
  0,  8,  1,  9, 16, 24,  2, 10,
 17, 25, 32, 40, 48, 56, 33, 41,
 18, 26,  3, 11,  4, 12, 19, 27,
 34, 42, 49, 57, 50, 58, 35, 43,
 20, 28,  5, 13,  6, 14, 21, 29,
 36, 44, 51, 59, 52, 60, 37, 45,
 22, 30,  7, 15, 23, 31, 38, 46,
 53, 61, 54, 62, 39, 47, 55, 63,
};

/* unquant tables (not used directly) */
static const uint8_t dv_88_areas[64] = {
    0,0,0,1,1,1,2,2,
    0,0,1,1,1,2,2,2,
    0,1,1,1,2,2,2,3,
    1,1,1,2,2,2,3,3,
    1,1,2,2,2,3,3,3,
    1,2,2,2,3,3,3,3,
    2,2,2,3,3,3,3,3,
    2,2,3,3,3,3,3,3,
};

static const uint8_t dv_248_areas[64] = {
    0,0,1,1,1,2,2,3,
    0,0,1,1,2,2,2,3,
    0,1,1,2,2,2,3,3,
    0,1,1,2,2,2,3,3,
    1,1,2,2,2,3,3,3,
    1,1,2,2,2,3,3,3,
    1,2,2,2,3,3,3,3,
    1,2,2,3,3,3,3,3,
};

static uint8_t dv_quant_shifts[22][4] = {
  { 3,3,4,4 }, 
  { 3,3,4,4 }, 
  { 2,3,3,4 }, 
  { 2,3,3,4 },
  { 2,2,3,3 }, 
  { 2,2,3,3 }, 
  { 1,2,2,3 }, 
  { 1,2,2,3 }, 
  { 1,1,2,2 }, 
  { 1,1,2,2 }, 
  { 0,1,1,2 }, 
  { 0,1,1,2 }, 
  { 0,0,1,1 }, 
  { 0,0,1,1 },
  { 0,0,0,1 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 }, 
  { 0,0,0,0 },
};

static const uint8_t dv_quant_offset[4] = { 6, 3, 0, 1 };

/* NOTE: I prefer hardcoding the positionning of dv blocks, it is
   simpler :-) */

static const uint16_t dv_place_420[1620] = {
 0x0c24, 0x2412, 0x3036, 0x0000, 0x1848,
 0x0e24, 0x2612, 0x3236, 0x0200, 0x1a48,
 0x1024, 0x2812, 0x3436, 0x0400, 0x1c48,
 0x1026, 0x2814, 0x3438, 0x0402, 0x1c4a,
 0x0e26, 0x2614, 0x3238, 0x0202, 0x1a4a,
 0x0c26, 0x2414, 0x3038, 0x0002, 0x184a,
 0x0c28, 0x2416, 0x303a, 0x0004, 0x184c,
 0x0e28, 0x2616, 0x323a, 0x0204, 0x1a4c,
 0x1028, 0x2816, 0x343a, 0x0404, 0x1c4c,
 0x102a, 0x2818, 0x343c, 0x0406, 0x1c4e,
 0x0e2a, 0x2618, 0x323c, 0x0206, 0x1a4e,
 0x0c2a, 0x2418, 0x303c, 0x0006, 0x184e,
 0x0c2c, 0x241a, 0x303e, 0x0008, 0x1850,
 0x0e2c, 0x261a, 0x323e, 0x0208, 0x1a50,
 0x102c, 0x281a, 0x343e, 0x0408, 0x1c50,
 0x102e, 0x281c, 0x3440, 0x040a, 0x1c52,
 0x0e2e, 0x261c, 0x3240, 0x020a, 0x1a52,
 0x0c2e, 0x241c, 0x3040, 0x000a, 0x1852,
 0x0c30, 0x241e, 0x3042, 0x000c, 0x1854,
 0x0e30, 0x261e, 0x3242, 0x020c, 0x1a54,
 0x1030, 0x281e, 0x3442, 0x040c, 0x1c54,
 0x1032, 0x2820, 0x3444, 0x040e, 0x1c56,
 0x0e32, 0x2620, 0x3244, 0x020e, 0x1a56,
 0x0c32, 0x2420, 0x3044, 0x000e, 0x1856,
 0x0c34, 0x2422, 0x3046, 0x0010, 0x1858,
 0x0e34, 0x2622, 0x3246, 0x0210, 0x1a58,
 0x1034, 0x2822, 0x3446, 0x0410, 0x1c58,
 0x1224, 0x2a12, 0x3636, 0x0600, 0x1e48,
 0x1424, 0x2c12, 0x3836, 0x0800, 0x2048,
 0x1624, 0x2e12, 0x3a36, 0x0a00, 0x2248,
 0x1626, 0x2e14, 0x3a38, 0x0a02, 0x224a,
 0x1426, 0x2c14, 0x3838, 0x0802, 0x204a,
 0x1226, 0x2a14, 0x3638, 0x0602, 0x1e4a,
 0x1228, 0x2a16, 0x363a, 0x0604, 0x1e4c,
 0x1428, 0x2c16, 0x383a, 0x0804, 0x204c,
 0x1628, 0x2e16, 0x3a3a, 0x0a04, 0x224c,
 0x162a, 0x2e18, 0x3a3c, 0x0a06, 0x224e,
 0x142a, 0x2c18, 0x383c, 0x0806, 0x204e,
 0x122a, 0x2a18, 0x363c, 0x0606, 0x1e4e,
 0x122c, 0x2a1a, 0x363e, 0x0608, 0x1e50,
 0x142c, 0x2c1a, 0x383e, 0x0808, 0x2050,
 0x162c, 0x2e1a, 0x3a3e, 0x0a08, 0x2250,
 0x162e, 0x2e1c, 0x3a40, 0x0a0a, 0x2252,
 0x142e, 0x2c1c, 0x3840, 0x080a, 0x2052,
 0x122e, 0x2a1c, 0x3640, 0x060a, 0x1e52,
 0x1230, 0x2a1e, 0x3642, 0x060c, 0x1e54,
 0x1430, 0x2c1e, 0x3842, 0x080c, 0x2054,
 0x1630, 0x2e1e, 0x3a42, 0x0a0c, 0x2254,
 0x1632, 0x2e20, 0x3a44, 0x0a0e, 0x2256,
 0x1432, 0x2c20, 0x3844, 0x080e, 0x2056,
 0x1232, 0x2a20, 0x3644, 0x060e, 0x1e56,
 0x1234, 0x2a22, 0x3646, 0x0610, 0x1e58,
 0x1434, 0x2c22, 0x3846, 0x0810, 0x2058,
 0x1634, 0x2e22, 0x3a46, 0x0a10, 0x2258,
 0x1824, 0x3012, 0x3c36, 0x0c00, 0x2448,
 0x1a24, 0x3212, 0x3e36, 0x0e00, 0x2648,
 0x1c24, 0x3412, 0x4036, 0x1000, 0x2848,
 0x1c26, 0x3414, 0x4038, 0x1002, 0x284a,
 0x1a26, 0x3214, 0x3e38, 0x0e02, 0x264a,
 0x1826, 0x3014, 0x3c38, 0x0c02, 0x244a,
 0x1828, 0x3016, 0x3c3a, 0x0c04, 0x244c,
 0x1a28, 0x3216, 0x3e3a, 0x0e04, 0x264c,
 0x1c28, 0x3416, 0x403a, 0x1004, 0x284c,
 0x1c2a, 0x3418, 0x403c, 0x1006, 0x284e,
 0x1a2a, 0x3218, 0x3e3c, 0x0e06, 0x264e,
 0x182a, 0x3018, 0x3c3c, 0x0c06, 0x244e,
 0x182c, 0x301a, 0x3c3e, 0x0c08, 0x2450,
 0x1a2c, 0x321a, 0x3e3e, 0x0e08, 0x2650,
 0x1c2c, 0x341a, 0x403e, 0x1008, 0x2850,
 0x1c2e, 0x341c, 0x4040, 0x100a, 0x2852,
 0x1a2e, 0x321c, 0x3e40, 0x0e0a, 0x2652,
 0x182e, 0x301c, 0x3c40, 0x0c0a, 0x2452,
 0x1830, 0x301e, 0x3c42, 0x0c0c, 0x2454,
 0x1a30, 0x321e, 0x3e42, 0x0e0c, 0x2654,
 0x1c30, 0x341e, 0x4042, 0x100c, 0x2854,
 0x1c32, 0x3420, 0x4044, 0x100e, 0x2856,
 0x1a32, 0x3220, 0x3e44, 0x0e0e, 0x2656,
 0x1832, 0x3020, 0x3c44, 0x0c0e, 0x2456,
 0x1834, 0x3022, 0x3c46, 0x0c10, 0x2458,
 0x1a34, 0x3222, 0x3e46, 0x0e10, 0x2658,
 0x1c34, 0x3422, 0x4046, 0x1010, 0x2858,
 0x1e24, 0x3612, 0x4236, 0x1200, 0x2a48,
 0x2024, 0x3812, 0x4436, 0x1400, 0x2c48,
 0x2224, 0x3a12, 0x4636, 0x1600, 0x2e48,
 0x2226, 0x3a14, 0x4638, 0x1602, 0x2e4a,
 0x2026, 0x3814, 0x4438, 0x1402, 0x2c4a,
 0x1e26, 0x3614, 0x4238, 0x1202, 0x2a4a,
 0x1e28, 0x3616, 0x423a, 0x1204, 0x2a4c,
 0x2028, 0x3816, 0x443a, 0x1404, 0x2c4c,
 0x2228, 0x3a16, 0x463a, 0x1604, 0x2e4c,
 0x222a, 0x3a18, 0x463c, 0x1606, 0x2e4e,
 0x202a, 0x3818, 0x443c, 0x1406, 0x2c4e,
 0x1e2a, 0x3618, 0x423c, 0x1206, 0x2a4e,
 0x1e2c, 0x361a, 0x423e, 0x1208, 0x2a50,
 0x202c, 0x381a, 0x443e, 0x1408, 0x2c50,
 0x222c, 0x3a1a, 0x463e, 0x1608, 0x2e50,
 0x222e, 0x3a1c, 0x4640, 0x160a, 0x2e52,
 0x202e, 0x381c, 0x4440, 0x140a, 0x2c52,
 0x1e2e, 0x361c, 0x4240, 0x120a, 0x2a52,
 0x1e30, 0x361e, 0x4242, 0x120c, 0x2a54,
 0x2030, 0x381e, 0x4442, 0x140c, 0x2c54,
 0x2230, 0x3a1e, 0x4642, 0x160c, 0x2e54,
 0x2232, 0x3a20, 0x4644, 0x160e, 0x2e56,
 0x2032, 0x3820, 0x4444, 0x140e, 0x2c56,
 0x1e32, 0x3620, 0x4244, 0x120e, 0x2a56,
 0x1e34, 0x3622, 0x4246, 0x1210, 0x2a58,
 0x2034, 0x3822, 0x4446, 0x1410, 0x2c58,
 0x2234, 0x3a22, 0x4646, 0x1610, 0x2e58,
 0x2424, 0x3c12, 0x0036, 0x1800, 0x3048,
 0x2624, 0x3e12, 0x0236, 0x1a00, 0x3248,
 0x2824, 0x4012, 0x0436, 0x1c00, 0x3448,
 0x2826, 0x4014, 0x0438, 0x1c02, 0x344a,
 0x2626, 0x3e14, 0x0238, 0x1a02, 0x324a,
 0x2426, 0x3c14, 0x0038, 0x1802, 0x304a,
 0x2428, 0x3c16, 0x003a, 0x1804, 0x304c,
 0x2628, 0x3e16, 0x023a, 0x1a04, 0x324c,
 0x2828, 0x4016, 0x043a, 0x1c04, 0x344c,
 0x282a, 0x4018, 0x043c, 0x1c06, 0x344e,
 0x262a, 0x3e18, 0x023c, 0x1a06, 0x324e,
 0x242a, 0x3c18, 0x003c, 0x1806, 0x304e,
 0x242c, 0x3c1a, 0x003e, 0x1808, 0x3050,
 0x262c, 0x3e1a, 0x023e, 0x1a08, 0x3250,
 0x282c, 0x401a, 0x043e, 0x1c08, 0x3450,
 0x282e, 0x401c, 0x0440, 0x1c0a, 0x3452,
 0x262e, 0x3e1c, 0x0240, 0x1a0a, 0x3252,
 0x242e, 0x3c1c, 0x0040, 0x180a, 0x3052,
 0x2430, 0x3c1e, 0x0042, 0x180c, 0x3054,
 0x2630, 0x3e1e, 0x0242, 0x1a0c, 0x3254,
 0x2830, 0x401e, 0x0442, 0x1c0c, 0x3454,
 0x2832, 0x4020, 0x0444, 0x1c0e, 0x3456,
 0x2632, 0x3e20, 0x0244, 0x1a0e, 0x3256,
 0x2432, 0x3c20, 0x0044, 0x180e, 0x3056,
 0x2434, 0x3c22, 0x0046, 0x1810, 0x3058,
 0x2634, 0x3e22, 0x0246, 0x1a10, 0x3258,
 0x2834, 0x4022, 0x0446, 0x1c10, 0x3458,
 0x2a24, 0x4212, 0x0636, 0x1e00, 0x3648,
 0x2c24, 0x4412, 0x0836, 0x2000, 0x3848,
 0x2e24, 0x4612, 0x0a36, 0x2200, 0x3a48,
 0x2e26, 0x4614, 0x0a38, 0x2202, 0x3a4a,
 0x2c26, 0x4414, 0x0838, 0x2002, 0x384a,
 0x2a26, 0x4214, 0x0638, 0x1e02, 0x364a,
 0x2a28, 0x4216, 0x063a, 0x1e04, 0x364c,
 0x2c28, 0x4416, 0x083a, 0x2004, 0x384c,
 0x2e28, 0x4616, 0x0a3a, 0x2204, 0x3a4c,
 0x2e2a, 0x4618, 0x0a3c, 0x2206, 0x3a4e,
 0x2c2a, 0x4418, 0x083c, 0x2006, 0x384e,
 0x2a2a, 0x4218, 0x063c, 0x1e06, 0x364e,
 0x2a2c, 0x421a, 0x063e, 0x1e08, 0x3650,
 0x2c2c, 0x441a, 0x083e, 0x2008, 0x3850,
 0x2e2c, 0x461a, 0x0a3e, 0x2208, 0x3a50,
 0x2e2e, 0x461c, 0x0a40, 0x220a, 0x3a52,
 0x2c2e, 0x441c, 0x0840, 0x200a, 0x3852,
 0x2a2e, 0x421c, 0x0640, 0x1e0a, 0x3652,
 0x2a30, 0x421e, 0x0642, 0x1e0c, 0x3654,
 0x2c30, 0x441e, 0x0842, 0x200c, 0x3854,
 0x2e30, 0x461e, 0x0a42, 0x220c, 0x3a54,
 0x2e32, 0x4620, 0x0a44, 0x220e, 0x3a56,
 0x2c32, 0x4420, 0x0844, 0x200e, 0x3856,
 0x2a32, 0x4220, 0x0644, 0x1e0e, 0x3656,
 0x2a34, 0x4222, 0x0646, 0x1e10, 0x3658,
 0x2c34, 0x4422, 0x0846, 0x2010, 0x3858,
 0x2e34, 0x4622, 0x0a46, 0x2210, 0x3a58,
 0x3024, 0x0012, 0x0c36, 0x2400, 0x3c48,
 0x3224, 0x0212, 0x0e36, 0x2600, 0x3e48,
 0x3424, 0x0412, 0x1036, 0x2800, 0x4048,
 0x3426, 0x0414, 0x1038, 0x2802, 0x404a,
 0x3226, 0x0214, 0x0e38, 0x2602, 0x3e4a,
 0x3026, 0x0014, 0x0c38, 0x2402, 0x3c4a,
 0x3028, 0x0016, 0x0c3a, 0x2404, 0x3c4c,
 0x3228, 0x0216, 0x0e3a, 0x2604, 0x3e4c,
 0x3428, 0x0416, 0x103a, 0x2804, 0x404c,
 0x342a, 0x0418, 0x103c, 0x2806, 0x404e,
 0x322a, 0x0218, 0x0e3c, 0x2606, 0x3e4e,
 0x302a, 0x0018, 0x0c3c, 0x2406, 0x3c4e,
 0x302c, 0x001a, 0x0c3e, 0x2408, 0x3c50,
 0x322c, 0x021a, 0x0e3e, 0x2608, 0x3e50,
 0x342c, 0x041a, 0x103e, 0x2808, 0x4050,
 0x342e, 0x041c, 0x1040, 0x280a, 0x4052,
 0x322e, 0x021c, 0x0e40, 0x260a, 0x3e52,
 0x302e, 0x001c, 0x0c40, 0x240a, 0x3c52,
 0x3030, 0x001e, 0x0c42, 0x240c, 0x3c54,
 0x3230, 0x021e, 0x0e42, 0x260c, 0x3e54,
 0x3430, 0x041e, 0x1042, 0x280c, 0x4054,
 0x3432, 0x0420, 0x1044, 0x280e, 0x4056,
 0x3232, 0x0220, 0x0e44, 0x260e, 0x3e56,
 0x3032, 0x0020, 0x0c44, 0x240e, 0x3c56,
 0x3034, 0x0022, 0x0c46, 0x2410, 0x3c58,
 0x3234, 0x0222, 0x0e46, 0x2610, 0x3e58,
 0x3434, 0x0422, 0x1046, 0x2810, 0x4058,
 0x3624, 0x0612, 0x1236, 0x2a00, 0x4248,
 0x3824, 0x0812, 0x1436, 0x2c00, 0x4448,
 0x3a24, 0x0a12, 0x1636, 0x2e00, 0x4648,
 0x3a26, 0x0a14, 0x1638, 0x2e02, 0x464a,
 0x3826, 0x0814, 0x1438, 0x2c02, 0x444a,
 0x3626, 0x0614, 0x1238, 0x2a02, 0x424a,
 0x3628, 0x0616, 0x123a, 0x2a04, 0x424c,
 0x3828, 0x0816, 0x143a, 0x2c04, 0x444c,
 0x3a28, 0x0a16, 0x163a, 0x2e04, 0x464c,
 0x3a2a, 0x0a18, 0x163c, 0x2e06, 0x464e,
 0x382a, 0x0818, 0x143c, 0x2c06, 0x444e,
 0x362a, 0x0618, 0x123c, 0x2a06, 0x424e,
 0x362c, 0x061a, 0x123e, 0x2a08, 0x4250,
 0x382c, 0x081a, 0x143e, 0x2c08, 0x4450,
 0x3a2c, 0x0a1a, 0x163e, 0x2e08, 0x4650,
 0x3a2e, 0x0a1c, 0x1640, 0x2e0a, 0x4652,
 0x382e, 0x081c, 0x1440, 0x2c0a, 0x4452,
 0x362e, 0x061c, 0x1240, 0x2a0a, 0x4252,
 0x3630, 0x061e, 0x1242, 0x2a0c, 0x4254,
 0x3830, 0x081e, 0x1442, 0x2c0c, 0x4454,
 0x3a30, 0x0a1e, 0x1642, 0x2e0c, 0x4654,
 0x3a32, 0x0a20, 0x1644, 0x2e0e, 0x4656,
 0x3832, 0x0820, 0x1444, 0x2c0e, 0x4456,
 0x3632, 0x0620, 0x1244, 0x2a0e, 0x4256,
 0x3634, 0x0622, 0x1246, 0x2a10, 0x4258,
 0x3834, 0x0822, 0x1446, 0x2c10, 0x4458,
 0x3a34, 0x0a22, 0x1646, 0x2e10, 0x4658,
 0x3c24, 0x0c12, 0x1836, 0x3000, 0x0048,
 0x3e24, 0x0e12, 0x1a36, 0x3200, 0x0248,
 0x4024, 0x1012, 0x1c36, 0x3400, 0x0448,
 0x4026, 0x1014, 0x1c38, 0x3402, 0x044a,
 0x3e26, 0x0e14, 0x1a38, 0x3202, 0x024a,
 0x3c26, 0x0c14, 0x1838, 0x3002, 0x004a,
 0x3c28, 0x0c16, 0x183a, 0x3004, 0x004c,
 0x3e28, 0x0e16, 0x1a3a, 0x3204, 0x024c,
 0x4028, 0x1016, 0x1c3a, 0x3404, 0x044c,
 0x402a, 0x1018, 0x1c3c, 0x3406, 0x044e,
 0x3e2a, 0x0e18, 0x1a3c, 0x3206, 0x024e,
 0x3c2a, 0x0c18, 0x183c, 0x3006, 0x004e,
 0x3c2c, 0x0c1a, 0x183e, 0x3008, 0x0050,
 0x3e2c, 0x0e1a, 0x1a3e, 0x3208, 0x0250,
 0x402c, 0x101a, 0x1c3e, 0x3408, 0x0450,
 0x402e, 0x101c, 0x1c40, 0x340a, 0x0452,
 0x3e2e, 0x0e1c, 0x1a40, 0x320a, 0x0252,
 0x3c2e, 0x0c1c, 0x1840, 0x300a, 0x0052,
 0x3c30, 0x0c1e, 0x1842, 0x300c, 0x0054,
 0x3e30, 0x0e1e, 0x1a42, 0x320c, 0x0254,
 0x4030, 0x101e, 0x1c42, 0x340c, 0x0454,
 0x4032, 0x1020, 0x1c44, 0x340e, 0x0456,
 0x3e32, 0x0e20, 0x1a44, 0x320e, 0x0256,
 0x3c32, 0x0c20, 0x1844, 0x300e, 0x0056,
 0x3c34, 0x0c22, 0x1846, 0x3010, 0x0058,
 0x3e34, 0x0e22, 0x1a46, 0x3210, 0x0258,
 0x4034, 0x1022, 0x1c46, 0x3410, 0x0458,
 0x4224, 0x1212, 0x1e36, 0x3600, 0x0648,
 0x4424, 0x1412, 0x2036, 0x3800, 0x0848,
 0x4624, 0x1612, 0x2236, 0x3a00, 0x0a48,
 0x4626, 0x1614, 0x2238, 0x3a02, 0x0a4a,
 0x4426, 0x1414, 0x2038, 0x3802, 0x084a,
 0x4226, 0x1214, 0x1e38, 0x3602, 0x064a,
 0x4228, 0x1216, 0x1e3a, 0x3604, 0x064c,
 0x4428, 0x1416, 0x203a, 0x3804, 0x084c,
 0x4628, 0x1616, 0x223a, 0x3a04, 0x0a4c,
 0x462a, 0x1618, 0x223c, 0x3a06, 0x0a4e,
 0x442a, 0x1418, 0x203c, 0x3806, 0x084e,
 0x422a, 0x1218, 0x1e3c, 0x3606, 0x064e,
 0x422c, 0x121a, 0x1e3e, 0x3608, 0x0650,
 0x442c, 0x141a, 0x203e, 0x3808, 0x0850,
 0x462c, 0x161a, 0x223e, 0x3a08, 0x0a50,
 0x462e, 0x161c, 0x2240, 0x3a0a, 0x0a52,
 0x442e, 0x141c, 0x2040, 0x380a, 0x0852,
 0x422e, 0x121c, 0x1e40, 0x360a, 0x0652,
 0x4230, 0x121e, 0x1e42, 0x360c, 0x0654,
 0x4430, 0x141e, 0x2042, 0x380c, 0x0854,
 0x4630, 0x161e, 0x2242, 0x3a0c, 0x0a54,
 0x4632, 0x1620, 0x2244, 0x3a0e, 0x0a56,
 0x4432, 0x1420, 0x2044, 0x380e, 0x0856,
 0x4232, 0x1220, 0x1e44, 0x360e, 0x0656,
 0x4234, 0x1222, 0x1e46, 0x3610, 0x0658,
 0x4434, 0x1422, 0x2046, 0x3810, 0x0858,
 0x4634, 0x1622, 0x2246, 0x3a10, 0x0a58,
 0x0024, 0x1812, 0x2436, 0x3c00, 0x0c48,
 0x0224, 0x1a12, 0x2636, 0x3e00, 0x0e48,
 0x0424, 0x1c12, 0x2836, 0x4000, 0x1048,
 0x0426, 0x1c14, 0x2838, 0x4002, 0x104a,
 0x0226, 0x1a14, 0x2638, 0x3e02, 0x0e4a,
 0x0026, 0x1814, 0x2438, 0x3c02, 0x0c4a,
 0x0028, 0x1816, 0x243a, 0x3c04, 0x0c4c,
 0x0228, 0x1a16, 0x263a, 0x3e04, 0x0e4c,
 0x0428, 0x1c16, 0x283a, 0x4004, 0x104c,
 0x042a, 0x1c18, 0x283c, 0x4006, 0x104e,
 0x022a, 0x1a18, 0x263c, 0x3e06, 0x0e4e,
 0x002a, 0x1818, 0x243c, 0x3c06, 0x0c4e,
 0x002c, 0x181a, 0x243e, 0x3c08, 0x0c50,
 0x022c, 0x1a1a, 0x263e, 0x3e08, 0x0e50,
 0x042c, 0x1c1a, 0x283e, 0x4008, 0x1050,
 0x042e, 0x1c1c, 0x2840, 0x400a, 0x1052,
 0x022e, 0x1a1c, 0x2640, 0x3e0a, 0x0e52,
 0x002e, 0x181c, 0x2440, 0x3c0a, 0x0c52,
 0x0030, 0x181e, 0x2442, 0x3c0c, 0x0c54,
 0x0230, 0x1a1e, 0x2642, 0x3e0c, 0x0e54,
 0x0430, 0x1c1e, 0x2842, 0x400c, 0x1054,
 0x0432, 0x1c20, 0x2844, 0x400e, 0x1056,
 0x0232, 0x1a20, 0x2644, 0x3e0e, 0x0e56,
 0x0032, 0x1820, 0x2444, 0x3c0e, 0x0c56,
 0x0034, 0x1822, 0x2446, 0x3c10, 0x0c58,
 0x0234, 0x1a22, 0x2646, 0x3e10, 0x0e58,
 0x0434, 0x1c22, 0x2846, 0x4010, 0x1058,
 0x0624, 0x1e12, 0x2a36, 0x4200, 0x1248,
 0x0824, 0x2012, 0x2c36, 0x4400, 0x1448,
 0x0a24, 0x2212, 0x2e36, 0x4600, 0x1648,
 0x0a26, 0x2214, 0x2e38, 0x4602, 0x164a,
 0x0826, 0x2014, 0x2c38, 0x4402, 0x144a,
 0x0626, 0x1e14, 0x2a38, 0x4202, 0x124a,
 0x0628, 0x1e16, 0x2a3a, 0x4204, 0x124c,
 0x0828, 0x2016, 0x2c3a, 0x4404, 0x144c,
 0x0a28, 0x2216, 0x2e3a, 0x4604, 0x164c,
 0x0a2a, 0x2218, 0x2e3c, 0x4606, 0x164e,
 0x082a, 0x2018, 0x2c3c, 0x4406, 0x144e,
 0x062a, 0x1e18, 0x2a3c, 0x4206, 0x124e,
 0x062c, 0x1e1a, 0x2a3e, 0x4208, 0x1250,
 0x082c, 0x201a, 0x2c3e, 0x4408, 0x1450,
 0x0a2c, 0x221a, 0x2e3e, 0x4608, 0x1650,
 0x0a2e, 0x221c, 0x2e40, 0x460a, 0x1652,
 0x082e, 0x201c, 0x2c40, 0x440a, 0x1452,
 0x062e, 0x1e1c, 0x2a40, 0x420a, 0x1252,
 0x0630, 0x1e1e, 0x2a42, 0x420c, 0x1254,
 0x0830, 0x201e, 0x2c42, 0x440c, 0x1454,
 0x0a30, 0x221e, 0x2e42, 0x460c, 0x1654,
 0x0a32, 0x2220, 0x2e44, 0x460e, 0x1656,
 0x0832, 0x2020, 0x2c44, 0x440e, 0x1456,
 0x0632, 0x1e20, 0x2a44, 0x420e, 0x1256,
 0x0634, 0x1e22, 0x2a46, 0x4210, 0x1258,
 0x0834, 0x2022, 0x2c46, 0x4410, 0x1458,
 0x0a34, 0x2222, 0x2e46, 0x4610, 0x1658,
};

static const uint16_t dv_place_411[1350] = {
 0x0c24, 0x2710, 0x3334, 0x0000, 0x1848,
 0x0d24, 0x2810, 0x3434, 0x0100, 0x1948,
 0x0e24, 0x2910, 0x3534, 0x0200, 0x1a48,
 0x0f24, 0x2914, 0x3538, 0x0300, 0x1b48,
 0x1024, 0x2814, 0x3438, 0x0400, 0x1c48,
 0x1124, 0x2714, 0x3338, 0x0500, 0x1d48,
 0x1128, 0x2614, 0x3238, 0x0504, 0x1d4c,
 0x1028, 0x2514, 0x3138, 0x0404, 0x1c4c,
 0x0f28, 0x2414, 0x3038, 0x0304, 0x1b4c,
 0x0e28, 0x2418, 0x303c, 0x0204, 0x1a4c,
 0x0d28, 0x2518, 0x313c, 0x0104, 0x194c,
 0x0c28, 0x2618, 0x323c, 0x0004, 0x184c,
 0x0c2c, 0x2718, 0x333c, 0x0008, 0x1850,
 0x0d2c, 0x2818, 0x343c, 0x0108, 0x1950,
 0x0e2c, 0x2918, 0x353c, 0x0208, 0x1a50,
 0x0f2c, 0x291c, 0x3540, 0x0308, 0x1b50,
 0x102c, 0x281c, 0x3440, 0x0408, 0x1c50,
 0x112c, 0x271c, 0x3340, 0x0508, 0x1d50,
 0x1130, 0x261c, 0x3240, 0x050c, 0x1d54,
 0x1030, 0x251c, 0x3140, 0x040c, 0x1c54,
 0x0f30, 0x241c, 0x3040, 0x030c, 0x1b54,
 0x0e30, 0x2420, 0x3044, 0x020c, 0x1a54,
 0x0d30, 0x2520, 0x3144, 0x010c, 0x1954,
 0x0c30, 0x2620, 0x3244, 0x000c, 0x1854,
 0x0c34, 0x2720, 0x3344, 0x0010, 0x1858,
 0x0d34, 0x2820, 0x3444, 0x0110, 0x1a58,
 0x0e34, 0x2920, 0x3544, 0x0210, 0x1c58,
 0x1224, 0x2d10, 0x3934, 0x0600, 0x1e48,
 0x1324, 0x2e10, 0x3a34, 0x0700, 0x1f48,
 0x1424, 0x2f10, 0x3b34, 0x0800, 0x2048,
 0x1524, 0x2f14, 0x3b38, 0x0900, 0x2148,
 0x1624, 0x2e14, 0x3a38, 0x0a00, 0x2248,
 0x1724, 0x2d14, 0x3938, 0x0b00, 0x2348,
 0x1728, 0x2c14, 0x3838, 0x0b04, 0x234c,
 0x1628, 0x2b14, 0x3738, 0x0a04, 0x224c,
 0x1528, 0x2a14, 0x3638, 0x0904, 0x214c,
 0x1428, 0x2a18, 0x363c, 0x0804, 0x204c,
 0x1328, 0x2b18, 0x373c, 0x0704, 0x1f4c,
 0x1228, 0x2c18, 0x383c, 0x0604, 0x1e4c,
 0x122c, 0x2d18, 0x393c, 0x0608, 0x1e50,
 0x132c, 0x2e18, 0x3a3c, 0x0708, 0x1f50,
 0x142c, 0x2f18, 0x3b3c, 0x0808, 0x2050,
 0x152c, 0x2f1c, 0x3b40, 0x0908, 0x2150,
 0x162c, 0x2e1c, 0x3a40, 0x0a08, 0x2250,
 0x172c, 0x2d1c, 0x3940, 0x0b08, 0x2350,
 0x1730, 0x2c1c, 0x3840, 0x0b0c, 0x2354,
 0x1630, 0x2b1c, 0x3740, 0x0a0c, 0x2254,
 0x1530, 0x2a1c, 0x3640, 0x090c, 0x2154,
 0x1430, 0x2a20, 0x3644, 0x080c, 0x2054,
 0x1330, 0x2b20, 0x3744, 0x070c, 0x1f54,
 0x1230, 0x2c20, 0x3844, 0x060c, 0x1e54,
 0x1234, 0x2d20, 0x3944, 0x0610, 0x1e58,
 0x1334, 0x2e20, 0x3a44, 0x0710, 0x2058,
 0x1434, 0x2f20, 0x3b44, 0x0810, 0x2258,
 0x1824, 0x3310, 0x0334, 0x0c00, 0x2448,
 0x1924, 0x3410, 0x0434, 0x0d00, 0x2548,
 0x1a24, 0x3510, 0x0534, 0x0e00, 0x2648,
 0x1b24, 0x3514, 0x0538, 0x0f00, 0x2748,
 0x1c24, 0x3414, 0x0438, 0x1000, 0x2848,
 0x1d24, 0x3314, 0x0338, 0x1100, 0x2948,
 0x1d28, 0x3214, 0x0238, 0x1104, 0x294c,
 0x1c28, 0x3114, 0x0138, 0x1004, 0x284c,
 0x1b28, 0x3014, 0x0038, 0x0f04, 0x274c,
 0x1a28, 0x3018, 0x003c, 0x0e04, 0x264c,
 0x1928, 0x3118, 0x013c, 0x0d04, 0x254c,
 0x1828, 0x3218, 0x023c, 0x0c04, 0x244c,
 0x182c, 0x3318, 0x033c, 0x0c08, 0x2450,
 0x192c, 0x3418, 0x043c, 0x0d08, 0x2550,
 0x1a2c, 0x3518, 0x053c, 0x0e08, 0x2650,
 0x1b2c, 0x351c, 0x0540, 0x0f08, 0x2750,
 0x1c2c, 0x341c, 0x0440, 0x1008, 0x2850,
 0x1d2c, 0x331c, 0x0340, 0x1108, 0x2950,
 0x1d30, 0x321c, 0x0240, 0x110c, 0x2954,
 0x1c30, 0x311c, 0x0140, 0x100c, 0x2854,
 0x1b30, 0x301c, 0x0040, 0x0f0c, 0x2754,
 0x1a30, 0x3020, 0x0044, 0x0e0c, 0x2654,
 0x1930, 0x3120, 0x0144, 0x0d0c, 0x2554,
 0x1830, 0x3220, 0x0244, 0x0c0c, 0x2454,
 0x1834, 0x3320, 0x0344, 0x0c10, 0x2458,
 0x1934, 0x3420, 0x0444, 0x0d10, 0x2658,
 0x1a34, 0x3520, 0x0544, 0x0e10, 0x2858,
 0x1e24, 0x3910, 0x0934, 0x1200, 0x2a48,
 0x1f24, 0x3a10, 0x0a34, 0x1300, 0x2b48,
 0x2024, 0x3b10, 0x0b34, 0x1400, 0x2c48,
 0x2124, 0x3b14, 0x0b38, 0x1500, 0x2d48,
 0x2224, 0x3a14, 0x0a38, 0x1600, 0x2e48,
 0x2324, 0x3914, 0x0938, 0x1700, 0x2f48,
 0x2328, 0x3814, 0x0838, 0x1704, 0x2f4c,
 0x2228, 0x3714, 0x0738, 0x1604, 0x2e4c,
 0x2128, 0x3614, 0x0638, 0x1504, 0x2d4c,
 0x2028, 0x3618, 0x063c, 0x1404, 0x2c4c,
 0x1f28, 0x3718, 0x073c, 0x1304, 0x2b4c,
 0x1e28, 0x3818, 0x083c, 0x1204, 0x2a4c,
 0x1e2c, 0x3918, 0x093c, 0x1208, 0x2a50,
 0x1f2c, 0x3a18, 0x0a3c, 0x1308, 0x2b50,
 0x202c, 0x3b18, 0x0b3c, 0x1408, 0x2c50,
 0x212c, 0x3b1c, 0x0b40, 0x1508, 0x2d50,
 0x222c, 0x3a1c, 0x0a40, 0x1608, 0x2e50,
 0x232c, 0x391c, 0x0940, 0x1708, 0x2f50,
 0x2330, 0x381c, 0x0840, 0x170c, 0x2f54,
 0x2230, 0x371c, 0x0740, 0x160c, 0x2e54,
 0x2130, 0x361c, 0x0640, 0x150c, 0x2d54,
 0x2030, 0x3620, 0x0644, 0x140c, 0x2c54,
 0x1f30, 0x3720, 0x0744, 0x130c, 0x2b54,
 0x1e30, 0x3820, 0x0844, 0x120c, 0x2a54,
 0x1e34, 0x3920, 0x0944, 0x1210, 0x2a58,
 0x1f34, 0x3a20, 0x0a44, 0x1310, 0x2c58,
 0x2034, 0x3b20, 0x0b44, 0x1410, 0x2e58,
 0x2424, 0x0310, 0x0f34, 0x1800, 0x3048,
 0x2524, 0x0410, 0x1034, 0x1900, 0x3148,
 0x2624, 0x0510, 0x1134, 0x1a00, 0x3248,
 0x2724, 0x0514, 0x1138, 0x1b00, 0x3348,
 0x2824, 0x0414, 0x1038, 0x1c00, 0x3448,
 0x2924, 0x0314, 0x0f38, 0x1d00, 0x3548,
 0x2928, 0x0214, 0x0e38, 0x1d04, 0x354c,
 0x2828, 0x0114, 0x0d38, 0x1c04, 0x344c,
 0x2728, 0x0014, 0x0c38, 0x1b04, 0x334c,
 0x2628, 0x0018, 0x0c3c, 0x1a04, 0x324c,
 0x2528, 0x0118, 0x0d3c, 0x1904, 0x314c,
 0x2428, 0x0218, 0x0e3c, 0x1804, 0x304c,
 0x242c, 0x0318, 0x0f3c, 0x1808, 0x3050,
 0x252c, 0x0418, 0x103c, 0x1908, 0x3150,
 0x262c, 0x0518, 0x113c, 0x1a08, 0x3250,
 0x272c, 0x051c, 0x1140, 0x1b08, 0x3350,
 0x282c, 0x041c, 0x1040, 0x1c08, 0x3450,
 0x292c, 0x031c, 0x0f40, 0x1d08, 0x3550,
 0x2930, 0x021c, 0x0e40, 0x1d0c, 0x3554,
 0x2830, 0x011c, 0x0d40, 0x1c0c, 0x3454,
 0x2730, 0x001c, 0x0c40, 0x1b0c, 0x3354,
 0x2630, 0x0020, 0x0c44, 0x1a0c, 0x3254,
 0x2530, 0x0120, 0x0d44, 0x190c, 0x3154,
 0x2430, 0x0220, 0x0e44, 0x180c, 0x3054,
 0x2434, 0x0320, 0x0f44, 0x1810, 0x3058,
 0x2534, 0x0420, 0x1044, 0x1910, 0x3258,
 0x2634, 0x0520, 0x1144, 0x1a10, 0x3458,
 0x2a24, 0x0910, 0x1534, 0x1e00, 0x3648,
 0x2b24, 0x0a10, 0x1634, 0x1f00, 0x3748,
 0x2c24, 0x0b10, 0x1734, 0x2000, 0x3848,
 0x2d24, 0x0b14, 0x1738, 0x2100, 0x3948,
 0x2e24, 0x0a14, 0x1638, 0x2200, 0x3a48,
 0x2f24, 0x0914, 0x1538, 0x2300, 0x3b48,
 0x2f28, 0x0814, 0x1438, 0x2304, 0x3b4c,
 0x2e28, 0x0714, 0x1338, 0x2204, 0x3a4c,
 0x2d28, 0x0614, 0x1238, 0x2104, 0x394c,
 0x2c28, 0x0618, 0x123c, 0x2004, 0x384c,
 0x2b28, 0x0718, 0x133c, 0x1f04, 0x374c,
 0x2a28, 0x0818, 0x143c, 0x1e04, 0x364c,
 0x2a2c, 0x0918, 0x153c, 0x1e08, 0x3650,
 0x2b2c, 0x0a18, 0x163c, 0x1f08, 0x3750,
 0x2c2c, 0x0b18, 0x173c, 0x2008, 0x3850,
 0x2d2c, 0x0b1c, 0x1740, 0x2108, 0x3950,
 0x2e2c, 0x0a1c, 0x1640, 0x2208, 0x3a50,
 0x2f2c, 0x091c, 0x1540, 0x2308, 0x3b50,
 0x2f30, 0x081c, 0x1440, 0x230c, 0x3b54,
 0x2e30, 0x071c, 0x1340, 0x220c, 0x3a54,
 0x2d30, 0x061c, 0x1240, 0x210c, 0x3954,
 0x2c30, 0x0620, 0x1244, 0x200c, 0x3854,
 0x2b30, 0x0720, 0x1344, 0x1f0c, 0x3754,
 0x2a30, 0x0820, 0x1444, 0x1e0c, 0x3654,
 0x2a34, 0x0920, 0x1544, 0x1e10, 0x3658,
 0x2b34, 0x0a20, 0x1644, 0x1f10, 0x3858,
 0x2c34, 0x0b20, 0x1744, 0x2010, 0x3a58,
 0x3024, 0x0f10, 0x1b34, 0x2400, 0x0048,
 0x3124, 0x1010, 0x1c34, 0x2500, 0x0148,
 0x3224, 0x1110, 0x1d34, 0x2600, 0x0248,
 0x3324, 0x1114, 0x1d38, 0x2700, 0x0348,
 0x3424, 0x1014, 0x1c38, 0x2800, 0x0448,
 0x3524, 0x0f14, 0x1b38, 0x2900, 0x0548,
 0x3528, 0x0e14, 0x1a38, 0x2904, 0x054c,
 0x3428, 0x0d14, 0x1938, 0x2804, 0x044c,
 0x3328, 0x0c14, 0x1838, 0x2704, 0x034c,
 0x3228, 0x0c18, 0x183c, 0x2604, 0x024c,
 0x3128, 0x0d18, 0x193c, 0x2504, 0x014c,
 0x3028, 0x0e18, 0x1a3c, 0x2404, 0x004c,
 0x302c, 0x0f18, 0x1b3c, 0x2408, 0x0050,
 0x312c, 0x1018, 0x1c3c, 0x2508, 0x0150,
 0x322c, 0x1118, 0x1d3c, 0x2608, 0x0250,
 0x332c, 0x111c, 0x1d40, 0x2708, 0x0350,
 0x342c, 0x101c, 0x1c40, 0x2808, 0x0450,
 0x352c, 0x0f1c, 0x1b40, 0x2908, 0x0550,
 0x3530, 0x0e1c, 0x1a40, 0x290c, 0x0554,
 0x3430, 0x0d1c, 0x1940, 0x280c, 0x0454,
 0x3330, 0x0c1c, 0x1840, 0x270c, 0x0354,
 0x3230, 0x0c20, 0x1844, 0x260c, 0x0254,
 0x3130, 0x0d20, 0x1944, 0x250c, 0x0154,
 0x3030, 0x0e20, 0x1a44, 0x240c, 0x0054,
 0x3034, 0x0f20, 0x1b44, 0x2410, 0x0058,
 0x3134, 0x1020, 0x1c44, 0x2510, 0x0258,
 0x3234, 0x1120, 0x1d44, 0x2610, 0x0458,
 0x3624, 0x1510, 0x2134, 0x2a00, 0x0648,
 0x3724, 0x1610, 0x2234, 0x2b00, 0x0748,
 0x3824, 0x1710, 0x2334, 0x2c00, 0x0848,
 0x3924, 0x1714, 0x2338, 0x2d00, 0x0948,
 0x3a24, 0x1614, 0x2238, 0x2e00, 0x0a48,
 0x3b24, 0x1514, 0x2138, 0x2f00, 0x0b48,
 0x3b28, 0x1414, 0x2038, 0x2f04, 0x0b4c,
 0x3a28, 0x1314, 0x1f38, 0x2e04, 0x0a4c,
 0x3928, 0x1214, 0x1e38, 0x2d04, 0x094c,
 0x3828, 0x1218, 0x1e3c, 0x2c04, 0x084c,
 0x3728, 0x1318, 0x1f3c, 0x2b04, 0x074c,
 0x3628, 0x1418, 0x203c, 0x2a04, 0x064c,
 0x362c, 0x1518, 0x213c, 0x2a08, 0x0650,
 0x372c, 0x1618, 0x223c, 0x2b08, 0x0750,
 0x382c, 0x1718, 0x233c, 0x2c08, 0x0850,
 0x392c, 0x171c, 0x2340, 0x2d08, 0x0950,
 0x3a2c, 0x161c, 0x2240, 0x2e08, 0x0a50,
 0x3b2c, 0x151c, 0x2140, 0x2f08, 0x0b50,
 0x3b30, 0x141c, 0x2040, 0x2f0c, 0x0b54,
 0x3a30, 0x131c, 0x1f40, 0x2e0c, 0x0a54,
 0x3930, 0x121c, 0x1e40, 0x2d0c, 0x0954,
 0x3830, 0x1220, 0x1e44, 0x2c0c, 0x0854,
 0x3730, 0x1320, 0x1f44, 0x2b0c, 0x0754,
 0x3630, 0x1420, 0x2044, 0x2a0c, 0x0654,
 0x3634, 0x1520, 0x2144, 0x2a10, 0x0658,
 0x3734, 0x1620, 0x2244, 0x2b10, 0x0858,
 0x3834, 0x1720, 0x2344, 0x2c10, 0x0a58,
 0x0024, 0x1b10, 0x2734, 0x3000, 0x0c48,
 0x0124, 0x1c10, 0x2834, 0x3100, 0x0d48,
 0x0224, 0x1d10, 0x2934, 0x3200, 0x0e48,
 0x0324, 0x1d14, 0x2938, 0x3300, 0x0f48,
 0x0424, 0x1c14, 0x2838, 0x3400, 0x1048,
 0x0524, 0x1b14, 0x2738, 0x3500, 0x1148,
 0x0528, 0x1a14, 0x2638, 0x3504, 0x114c,
 0x0428, 0x1914, 0x2538, 0x3404, 0x104c,
 0x0328, 0x1814, 0x2438, 0x3304, 0x0f4c,
 0x0228, 0x1818, 0x243c, 0x3204, 0x0e4c,
 0x0128, 0x1918, 0x253c, 0x3104, 0x0d4c,
 0x0028, 0x1a18, 0x263c, 0x3004, 0x0c4c,
 0x002c, 0x1b18, 0x273c, 0x3008, 0x0c50,
 0x012c, 0x1c18, 0x283c, 0x3108, 0x0d50,
 0x022c, 0x1d18, 0x293c, 0x3208, 0x0e50,
 0x032c, 0x1d1c, 0x2940, 0x3308, 0x0f50,
 0x042c, 0x1c1c, 0x2840, 0x3408, 0x1050,
 0x052c, 0x1b1c, 0x2740, 0x3508, 0x1150,
 0x0530, 0x1a1c, 0x2640, 0x350c, 0x1154,
 0x0430, 0x191c, 0x2540, 0x340c, 0x1054,
 0x0330, 0x181c, 0x2440, 0x330c, 0x0f54,
 0x0230, 0x1820, 0x2444, 0x320c, 0x0e54,
 0x0130, 0x1920, 0x2544, 0x310c, 0x0d54,
 0x0030, 0x1a20, 0x2644, 0x300c, 0x0c54,
 0x0034, 0x1b20, 0x2744, 0x3010, 0x0c58,
 0x0134, 0x1c20, 0x2844, 0x3110, 0x0e58,
 0x0234, 0x1d20, 0x2944, 0x3210, 0x1058,
 0x0624, 0x2110, 0x2d34, 0x3600, 0x1248,
 0x0724, 0x2210, 0x2e34, 0x3700, 0x1348,
 0x0824, 0x2310, 0x2f34, 0x3800, 0x1448,
 0x0924, 0x2314, 0x2f38, 0x3900, 0x1548,
 0x0a24, 0x2214, 0x2e38, 0x3a00, 0x1648,
 0x0b24, 0x2114, 0x2d38, 0x3b00, 0x1748,
 0x0b28, 0x2014, 0x2c38, 0x3b04, 0x174c,
 0x0a28, 0x1f14, 0x2b38, 0x3a04, 0x164c,
 0x0928, 0x1e14, 0x2a38, 0x3904, 0x154c,
 0x0828, 0x1e18, 0x2a3c, 0x3804, 0x144c,
 0x0728, 0x1f18, 0x2b3c, 0x3704, 0x134c,
 0x0628, 0x2018, 0x2c3c, 0x3604, 0x124c,
 0x062c, 0x2118, 0x2d3c, 0x3608, 0x1250,
 0x072c, 0x2218, 0x2e3c, 0x3708, 0x1350,
 0x082c, 0x2318, 0x2f3c, 0x3808, 0x1450,
 0x092c, 0x231c, 0x2f40, 0x3908, 0x1550,
 0x0a2c, 0x221c, 0x2e40, 0x3a08, 0x1650,
 0x0b2c, 0x211c, 0x2d40, 0x3b08, 0x1750,
 0x0b30, 0x201c, 0x2c40, 0x3b0c, 0x1754,
 0x0a30, 0x1f1c, 0x2b40, 0x3a0c, 0x1654,
 0x0930, 0x1e1c, 0x2a40, 0x390c, 0x1554,
 0x0830, 0x1e20, 0x2a44, 0x380c, 0x1454,
 0x0730, 0x1f20, 0x2b44, 0x370c, 0x1354,
 0x0630, 0x2020, 0x2c44, 0x360c, 0x1254,
 0x0634, 0x2120, 0x2d44, 0x3610, 0x1258,
 0x0734, 0x2220, 0x2e44, 0x3710, 0x1458,
 0x0834, 0x2320, 0x2f44, 0x3810, 0x1658,
};

static const uint16_t dv_place_audio60[10][9] = {
  {  0, 30, 60, 20, 50, 80, 10, 40, 70 }, /* 1st channel */
  {  6, 36, 66, 26, 56, 86, 16, 46, 76 },
  { 12, 42, 72,  2, 32, 62, 22, 52, 82 },
  { 18, 48, 78,  8, 38, 68, 28, 58, 88 },
  { 24, 54, 84, 14, 44, 74,  4, 34, 64 },
  
  {  1, 31, 61, 21, 51, 81, 11, 41, 71 }, /* 2nd channel */
  {  7, 37, 67, 27, 57, 87, 17, 47, 77 },
  { 13, 43, 73,  3, 33, 63, 23, 53, 83 },
  { 19, 49, 79,  9, 39, 69, 29, 59, 89 },
  { 25, 55, 85, 15, 45, 75,  5, 35, 65 },
};

static const uint16_t dv_place_audio50[12][9] = {
  {   0,  36,  72,  26,  62,  98,  16,  52,  88}, /* 1st channel */
  {   6,  42,  78,  32,  68, 104,  22,  58,  94},
  {  12,  48,  84,   2,  38,  74,  28,  64, 100},
  {  18,  54,  90,   8,  44,  80,  34,  70, 106},
  {  24,  60,  96,  14,  50,  86,   4,  40,  76},  
  {  30,  66, 102,  20,  56,  92,  10,  46,  82},
	
  {   1,  37,  73,  27,  63,  99,  17,  53,  89}, /* 2nd channel */
  {   7,  43,  79,  33,  69, 105,  23,  59,  95},
  {  13,  49,  85,   3,  39,  75,  29,  65, 101},
  {  19,  55,  91,   9,  45,  81,  35,  71, 107},
  {  25,  61,  97,  15,  51,  87,   5,  41,  77},  
  {  31,  67, 103,  21,  57,  93,  11,  47,  83},
};

static const int dv_audio_frequency[3] = {
    48000, 44100, 32000, 
};

static const int dv_audio_min_samples[2][3] = {
    { 1580, 1452, 1053 }, /* 60 fields */
    { 1896, 1742, 1264 }, /* 50 fileds */
};

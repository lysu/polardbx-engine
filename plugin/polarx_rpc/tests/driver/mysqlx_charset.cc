/*****************************************************************************

Copyright (c) 2023, 2024, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/


/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "mysqlx_charset.h"

namespace xcl {

const Charset::Charset_entry Charset::m_charsets_info[] = {
    /* The list is taken from protocol/mysqlx-protocol-xplugin.rst */
    {1, "big5", "big5_chinese_ci"},
    {2, "latin2", "latin2_czech_cs"},
    {3, "dec8", "dec8_swedish_ci"},
    {4, "cp850", "cp850_general_ci"},
    {5, "latin1", "latin1_german1_ci"},
    {6, "hp8", "hp8_english_ci"},
    {7, "koi8r", "koi8r_general_ci"},
    {8, "latin1", "latin1_swedish_ci"},
    {9, "latin2", "latin2_general_ci"},
    {10, "swe7", "swe7_swedish_ci"},
    {11, "ascii", "ascii_general_ci"},
    {12, "ujis", "ujis_japanese_ci"},
    {13, "sjis", "sjis_japanese_ci"},
    {14, "cp1251", "cp1251_bulgarian_ci"},
    {15, "latin1", "latin1_danish_ci"},
    {16, "hebrew", "hebrew_general_ci"},
    {18, "tis620", "tis620_thai_ci"},
    {19, "euckr", "euckr_korean_ci"},
    {20, "latin7", "latin7_estonian_cs"},
    {21, "latin2", "latin2_hungarian_ci"},
    {22, "koi8u", "koi8u_general_ci"},
    {23, "cp1251", "cp1251_ukrainian_ci"},
    {24, "gb2312", "gb2312_chinese_ci"},
    {25, "greek", "greek_general_ci"},
    {26, "cp1250", "cp1250_general_ci"},
    {27, "latin2", "latin2_croatian_ci"},
    {28, "gbk", "gbk_chinese_ci"},
    {29, "cp1257", "cp1257_lithuanian_ci"},
    {30, "latin5", "latin5_turkish_ci"},
    {31, "latin1", "latin1_german2_ci"},
    {32, "armscii8", "armscii8_general_ci"},
    {33, "utf8", "utf8_general_ci"},
    {34, "cp1250", "cp1250_czech_cs"},
    {35, "ucs2", "ucs2_general_ci"},
    {36, "cp866", "cp866_general_ci"},
    {37, "keybcs2", "keybcs2_general_ci"},
    {38, "macce", "macce_general_ci"},
    {39, "macroman", "macroman_general_ci"},
    {40, "cp852", "cp852_general_ci"},
    {41, "latin7", "latin7_general_ci"},
    {42, "latin7", "latin7_general_cs"},
    {43, "macce", "macce_bin"},
    {44, "cp1250", "cp1250_croatian_ci"},
    {45, "utf8mb4", "utf8mb4_general_ci"},
    {46, "utf8mb4", "utf8mb4_bin"},
    {47, "latin1", "latin1_bin"},
    {48, "latin1", "latin1_general_ci"},
    {49, "latin1", "latin1_general_cs"},
    {50, "cp1251", "cp1251_bin"},
    {51, "cp1251", "cp1251_general_ci"},
    {52, "cp1251", "cp1251_general_cs"},
    {53, "macroman", "macroman_bin"},
    {54, "utf16", "utf16_general_ci"},
    {55, "utf16", "utf16_bin"},
    {56, "utf16le", "utf16le_general_ci"},
    {57, "cp1256", "cp1256_general_ci"},
    {58, "cp1257", "cp1257_bin"},
    {59, "cp1257", "cp1257_general_ci"},
    {60, "utf32", "utf32_general_ci"},
    {61, "utf32", "utf32_bin"},
    {62, "utf16le", "utf16le_bin"},
    {63, "binary", "binary"},
    {64, "armscii8", "armscii8_bin"},
    {65, "ascii", "ascii_bin"},
    {66, "cp1250", "cp1250_bin"},
    {67, "cp1256", "cp1256_bin"},
    {68, "cp866", "cp866_bin"},
    {69, "dec8", "dec8_bin"},
    {70, "greek", "greek_bin"},
    {71, "hebrew", "hebrew_bin"},
    {72, "hp8", "hp8_bin"},
    {73, "keybcs2", "keybcs2_bin"},
    {74, "koi8r", "koi8r_bin"},
    {75, "koi8u", "koi8u_bin"},
    {77, "latin2", "latin2_bin"},
    {78, "latin5", "latin5_bin"},
    {79, "latin7", "latin7_bin"},
    {80, "cp850", "cp850_bin"},
    {81, "cp852", "cp852_bin"},
    {82, "swe7", "swe7_bin"},
    {83, "utf8", "utf8_bin"},
    {84, "big5", "big5_bin"},
    {85, "euckr", "euckr_bin"},
    {86, "gb2312", "gb2312_bin"},
    {87, "gbk", "gbk_bin"},
    {88, "sjis", "sjis_bin"},
    {89, "tis620", "tis620_bin"},
    {90, "ucs2", "ucs2_bin"},
    {91, "ujis", "ujis_bin"},
    {92, "geostd8", "geostd8_general_ci"},
    {93, "geostd8", "geostd8_bin"},
    {94, "latin1", "latin1_spanish_ci"},
    {95, "cp932", "cp932_japanese_ci"},
    {96, "cp932", "cp932_bin"},
    {97, "eucjpms", "eucjpms_japanese_ci"},
    {98, "eucjpms", "eucjpms_bin"},
    {99, "cp1250", "cp1250_polish_ci"},
    {101, "utf16", "utf16_unicode_ci"},
    {102, "utf16", "utf16_icelandic_ci"},
    {103, "utf16", "utf16_latvian_ci"},
    {104, "utf16", "utf16_romanian_ci"},
    {105, "utf16", "utf16_slovenian_ci"},
    {106, "utf16", "utf16_polish_ci"},
    {107, "utf16", "utf16_estonian_ci"},
    {108, "utf16", "utf16_spanish_ci"},
    {109, "utf16", "utf16_swedish_ci"},
    {110, "utf16", "utf16_turkish_ci"},
    {111, "utf16", "utf16_czech_ci"},
    {112, "utf16", "utf16_danish_ci"},
    {113, "utf16", "utf16_lithuanian_ci"},
    {114, "utf16", "utf16_slovak_ci"},
    {115, "utf16", "utf16_spanish2_ci"},
    {116, "utf16", "utf16_roman_ci"},
    {117, "utf16", "utf16_persian_ci"},
    {118, "utf16", "utf16_esperanto_ci"},
    {119, "utf16", "utf16_hungarian_ci"},
    {120, "utf16", "utf16_sinhala_ci"},
    {121, "utf16", "utf16_german2_ci"},
    {122, "utf16", "utf16_croatian_ci"},
    {123, "utf16", "utf16_unicode_520_ci"},
    {124, "utf16", "utf16_vietnamese_ci"},
    {128, "ucs2", "ucs2_unicode_ci"},
    {129, "ucs2", "ucs2_icelandic_ci"},
    {130, "ucs2", "ucs2_latvian_ci"},
    {131, "ucs2", "ucs2_romanian_ci"},
    {132, "ucs2", "ucs2_slovenian_ci"},
    {133, "ucs2", "ucs2_polish_ci"},
    {134, "ucs2", "ucs2_estonian_ci"},
    {135, "ucs2", "ucs2_spanish_ci"},
    {136, "ucs2", "ucs2_swedish_ci"},
    {137, "ucs2", "ucs2_turkish_ci"},
    {138, "ucs2", "ucs2_czech_ci"},
    {139, "ucs2", "ucs2_danish_ci"},
    {140, "ucs2", "ucs2_lithuanian_ci"},
    {141, "ucs2", "ucs2_slovak_ci"},
    {142, "ucs2", "ucs2_spanish2_ci"},
    {143, "ucs2", "ucs2_roman_ci"},
    {144, "ucs2", "ucs2_persian_ci"},
    {145, "ucs2", "ucs2_esperanto_ci"},
    {146, "ucs2", "ucs2_hungarian_ci"},
    {147, "ucs2", "ucs2_sinhala_ci"},
    {148, "ucs2", "ucs2_german2_ci"},
    {149, "ucs2", "ucs2_croatian_ci"},
    {150, "ucs2", "ucs2_unicode_520_ci"},
    {151, "ucs2", "ucs2_vietnamese_ci"},
    {159, "ucs2", "ucs2_general_mysql500_ci"},
    {160, "utf32", "utf32_unicode_ci"},
    {161, "utf32", "utf32_icelandic_ci"},
    {162, "utf32", "utf32_latvian_ci"},
    {163, "utf32", "utf32_romanian_ci"},
    {164, "utf32", "utf32_slovenian_ci"},
    {165, "utf32", "utf32_polish_ci"},
    {166, "utf32", "utf32_estonian_ci"},
    {167, "utf32", "utf32_spanish_ci"},
    {168, "utf32", "utf32_swedish_ci"},
    {169, "utf32", "utf32_turkish_ci"},
    {170, "utf32", "utf32_czech_ci"},
    {171, "utf32", "utf32_danish_ci"},
    {172, "utf32", "utf32_lithuanian_ci"},
    {173, "utf32", "utf32_slovak_ci"},
    {174, "utf32", "utf32_spanish2_ci"},
    {175, "utf32", "utf32_roman_ci"},
    {176, "utf32", "utf32_persian_ci"},
    {177, "utf32", "utf32_esperanto_ci"},
    {178, "utf32", "utf32_hungarian_ci"},
    {179, "utf32", "utf32_sinhala_ci"},
    {180, "utf32", "utf32_german2_ci"},
    {181, "utf32", "utf32_croatian_ci"},
    {182, "utf32", "utf32_unicode_520_ci"},
    {183, "utf32", "utf32_vietnamese_ci"},
    {192, "utf8", "utf8_unicode_ci"},
    {193, "utf8", "utf8_icelandic_ci"},
    {194, "utf8", "utf8_latvian_ci"},
    {195, "utf8", "utf8_romanian_ci"},
    {196, "utf8", "utf8_slovenian_ci"},
    {197, "utf8", "utf8_polish_ci"},
    {198, "utf8", "utf8_estonian_ci"},
    {199, "utf8", "utf8_spanish_ci"},
    {200, "utf8", "utf8_swedish_ci"},
    {201, "utf8", "utf8_turkish_ci"},
    {202, "utf8", "utf8_czech_ci"},
    {203, "utf8", "utf8_danish_ci"},
    {204, "utf8", "utf8_lithuanian_ci"},
    {205, "utf8", "utf8_slovak_ci"},
    {206, "utf8", "utf8_spanish2_ci"},
    {207, "utf8", "utf8_roman_ci"},
    {208, "utf8", "utf8_persian_ci"},
    {209, "utf8", "utf8_esperanto_ci"},
    {210, "utf8", "utf8_hungarian_ci"},
    {211, "utf8", "utf8_sinhala_ci"},
    {212, "utf8", "utf8_german2_ci"},
    {213, "utf8", "utf8_croatian_ci"},
    {214, "utf8", "utf8_unicode_520_ci"},
    {215, "utf8", "utf8_vietnamese_ci"},
    {223, "utf8", "utf8_general_mysql500_ci"},
    {224, "utf8mb4", "utf8mb4_unicode_ci"},
    {225, "utf8mb4", "utf8mb4_icelandic_ci"},
    {226, "utf8mb4", "utf8mb4_latvian_ci"},
    {227, "utf8mb4", "utf8mb4_romanian_ci"},
    {228, "utf8mb4", "utf8mb4_slovenian_ci"},
    {229, "utf8mb4", "utf8mb4_polish_ci"},
    {230, "utf8mb4", "utf8mb4_estonian_ci"},
    {231, "utf8mb4", "utf8mb4_spanish_ci"},
    {232, "utf8mb4", "utf8mb4_swedish_ci"},
    {233, "utf8mb4", "utf8mb4_turkish_ci"},
    {234, "utf8mb4", "utf8mb4_czech_ci"},
    {235, "utf8mb4", "utf8mb4_danish_ci"},
    {236, "utf8mb4", "utf8mb4_lithuanian_ci"},
    {237, "utf8mb4", "utf8mb4_slovak_ci"},
    {238, "utf8mb4", "utf8mb4_spanish2_ci"},
    {239, "utf8mb4", "utf8mb4_roman_ci"},
    {240, "utf8mb4", "utf8mb4_persian_ci"},
    {241, "utf8mb4", "utf8mb4_esperanto_ci"},
    {242, "utf8mb4", "utf8mb4_hungarian_ci"},
    {243, "utf8mb4", "utf8mb4_sinhala_ci"},
    {244, "utf8mb4", "utf8mb4_german2_ci"},
    {245, "utf8mb4", "utf8mb4_croatian_ci"},
    {246, "utf8mb4", "utf8mb4_unicode_520_ci"},
    {247, "utf8mb4", "utf8mb4_vietnamese_ci"},
    {248, "gb18030", "gb18030_chinese_ci"},
    {249, "gb18030", "gb18030_bin"},
    {250, "gb18030", "gb18030_unicode_520_ci"}};

std::string Charset::charset_name_from_id(uint32_t id) {
  return field_from_id(id, &Charset_entry::name);
}

std::string Charset::collation_name_from_id(uint32_t id) {
  return field_from_id(id, &Charset_entry::collation);
}

std::string Charset::field_from_id(uint32_t id,
                                   std::string Charset_entry::*field) {
  std::string result = (id == 0) ? "" : "?";

  for (const auto &charset_info : m_charsets_info) {
    if (charset_info.id == id) {
      result = charset_info.*field;
      break;
    }
  }

  return result;
}

uint32_t Charset::id_from_collation_name(const std::string &collation_name) {
  uint32_t result = 0;

  for (const auto &charset_info : m_charsets_info) {
    if (charset_info.collation == collation_name) {
      result = charset_info.id;
      break;
    }
  }

  return result;
}

}  // namespace xcl

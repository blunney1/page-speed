/*---------------------------------------------------------------------------

   rpng - simple PNG display program                              readpng.h

  ---------------------------------------------------------------------------

      Copyright (c) 1998-2007 Greg Roelofs.  All rights reserved.

      This software is provided "as is," without warranty of any kind,
      express or implied.  In no event shall the author or contributors
      be held liable for any damages arising in any way from the use of
      this software.

      The contents of this file are DUAL-LICENSED.  You may modify and/or
      redistribute this software according to the terms of one of the
      following two licenses (at your option):


      LICENSE 1 ("BSD-like with advertising clause"):

      Permission is granted to anyone to use this software for any purpose,
      including commercial applications, and to alter it and redistribute
      it freely, subject to the following restrictions:

      1. Redistributions of source code must retain the above copyright
         notice, disclaimer, and this list of conditions.
      2. Redistributions in binary form must reproduce the above copyright
         notice, disclaimer, and this list of conditions in the documenta-
         tion and/or other materials provided with the distribution.
      3. All advertising materials mentioning features or use of this
         software must display the following acknowledgment:

            This product includes software developed by Greg Roelofs
            and contributors for the book, "PNG: The Definitive Guide,"
            published by O'Reilly and Associates.


      LICENSE 2 (GNU GPL v2 or later):

      This program is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation; either version 2 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program; if not, write to the Free Software Foundation,
      Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  ---------------------------------------------------------------------------*/

#ifndef READPNG_H_
#define READPNG_H_

#include <sstream>

namespace readpng {

class ReadPNG {
 public:
  ReadPNG();
  ~ReadPNG();

  void readpng_version_info(void);

  int readpng_init(std::istringstream &in,
                   unsigned long *pWidth,
                   unsigned long *pHeight);

  int readpng_get_bgcolor(unsigned char *bg_red,
                          unsigned char *bg_green,
                          unsigned char *bg_blue);

  unsigned char *readpng_get_image(int *pChannels,
                                   unsigned long *pRowbytes);

  void readpng_cleanup(int free_image_data);

 private:
  // members from readpng.c
  png_structp png_ptr;
  png_infop info_ptr;

  png_uint_32 width, height;
  int bit_depth, color_type;
  int interlace_type, compression_type, filter_type;
  unsigned char *image_data;
};

}  // namespace readpng

#endif  // READPNG_H_

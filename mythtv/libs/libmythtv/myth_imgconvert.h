/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2009, Janne Grunau <janne-mythtv@grunau.be>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */
#ifndef MYTH_IMGCONVERT_H
#define MYTH_IMGCONVERT_H

extern "C" {
#include "libavcodec/avcodec.h"
}

/**
 * convert among pixel formats
 * utility function to replace deprecated img_convert with
 * the software scaler (swscale).
 */
MPUBLIC int myth_sws_img_convert(AVPicture *dst, int dst_pix_fmt,
                                 AVPicture *src, int pix_fmt,
                                 int width, int height);

#endif /* MYTH_IMGCONVERT_H */

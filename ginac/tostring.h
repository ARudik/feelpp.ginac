/** @file tostring.h
 *
 *  Convert object to its string representation (output form). This is an
 *  internal header file. */

/*
 *  GiNaC Copyright (C) 1999-2001 Johannes Gutenberg University Mainz, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __GINAC_TOSTRING_H__
#define __GINAC_TOSTRING_H__

#include "config.h"

#if defined(HAVE_SSTREAM)
#include <sstream>
#elif defined(HAVE_STRSTREAM)
#include <strstream>
#else
#error Need either sstream or strstream
#endif

namespace GiNaC {

// This should be obsoleted once <sstream> is widely deployed.
template<class T>
std::string ToString(const T & t)
{
#if defined(HAVE_SSTREAM)
	std::ostringstream buf;
	buf << t << std::ends;
	return buf.str();
#else
	char buf[256];
	std::ostrstream(buf,sizeof(buf)) << t << std::ends;
	return buf;
#endif
}

} // namespace GiNaC

#endif // ndef __GINAC_TOSTRING_H__
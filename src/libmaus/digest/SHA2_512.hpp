/*
    libmaus
    Copyright (C) 2009-2014 German Tischler
    Copyright (C) 2011-2014 Genome Research Limited

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#if ! defined(LIBMAUS_DIGEST_SHA2_512_HPP)
#define LIBMAUS_DIGEST_SHA2_512_HPP

#include <libmaus/digest/DigestBase.hpp>

namespace libmaus
{
	namespace digest
	{
		struct SHA2_512 : public DigestBase<64,7 /* block size 64 shift */, true /* need padding */, 16 /* number length */, false>
		{
			void * ctx;

			SHA2_512();
			~SHA2_512();
			
			void init();
			void update(uint8_t const * t, size_t l);
			void digest(uint8_t * digest);
			void copyFrom(SHA2_512 const & O);
			static size_t getDigestLength() { return digestlength; }
		};
	}
}
#endif

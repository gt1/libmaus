/*
    libmaus
    Copyright (C) 2009-2015 German Tischler
    Copyright (C) 2011-2015 Genome Research Limited

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
#include <libmaus/fastx/RefPathToken.hpp>

std::ostream & libmaus::fastx::operator<<(std::ostream & out, libmaus::fastx::RefPathToken const & O)
{
	if ( O.token == libmaus::fastx::RefPathToken::path_token_literal )
	{
		return out << "RefPathToken(literal," << O.s << ")";
	}
	else
	{
		return out << "RefPathToken(s," << O.n << ")";
	}
}
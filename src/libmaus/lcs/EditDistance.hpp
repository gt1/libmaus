/*
    libmaus
    Copyright (C) 2009-2013 German Tischler
    Copyright (C) 2011-2013 Genome Research Limited

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

#if !defined(LIBMAUS_LCS_EDITDISTANCE_HPP)
#define LIBMAUS_LCS_EDITDISTANCE_HPP

#include <libmaus/lcs/EditDistanceTraceContainer.hpp>
#include <libmaus/lcs/AlignmentPrint.hpp>
#include <libmaus/types/types.hpp>
#include <libmaus/autoarray/AutoArray.hpp>
#include <libmaus/exception/LibMausException.hpp>
#include <libmaus/lcs/EditDistanceResult.hpp>
#include <libmaus/lcs/BaseConstants.hpp>
#include <libmaus/lcs/PenaltyConstants.hpp>
#include <libmaus/lcs/AlignmentTraceContainer.hpp>
#include <map>
#include <iostream>
#include <iomanip>

namespace libmaus
{
	namespace lcs
	{		
		struct EditDistance : public EditDistanceTraceContainer
		{
			typedef EditDistance this_type;
			typedef ::libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
		
			private:
			uint64_t const n; // rows
			uint64_t const m; // columns
			uint64_t const n1;
			uint64_t const m1;

			// matrix stored column wise
			typedef std::pair < similarity_type, step_type > element_type;
			::libmaus::autoarray::AutoArray<element_type> M;

			public:
			EditDistance(uint64_t const rn, uint64_t const rm, uint64_t const = 0)
			: EditDistanceTraceContainer(rn+rm), n(rn), m(rm), n1(n+1), m1(m+1), M( n1*m1, false )
			{
				// std::cerr << "n=" << n << " m=" << m << " n1=" << n1 << " m1=" << m1 << std::endl;
			}
			
			element_type operator()(uint64_t const i, uint64_t const j) const
			{
				return M [ i * n1 + j ];
			}
			
			template<typename iterator_a, typename iterator_b>
			void printMatrix(iterator_a a, iterator_b b) const
			{
				element_type const * p = M.begin();
				for ( uint64_t i = 0; i < m1; ++i )
				{
					for ( uint64_t j = 0; j < n1; ++j, ++p )
					{
						std::cerr << "(" 
							<< std::setw(4)
							<< p->first 
							<< std::setw(0)
							<< "," 
							<< p->second 
							<< ","
							<< ((j > 0) ? a[j-1] : ' ')
							<< ","
							<< ((i > 0) ? b[i-1] : ' ')
							<< ")";
					}
					
					std::cerr << std::endl;
				}
			
			}
			
			template<typename iterator_a, typename iterator_b>
			EditDistanceResult process(
				iterator_a a, iterator_b b,
				similarity_type const gain_match = 1,
				similarity_type const penalty_subst = 1,
				similarity_type const penalty_ins = 1,
				similarity_type const penalty_del = 1
			)
			{
			
				element_type * p = M.begin();

				int64_t firstpen = 0;
				for ( uint64_t i = 0; i < n1; ++i, firstpen -= penalty_del )
					*(p++) = element_type(firstpen,STEP_DEL);
					
				element_type * q = M.begin();

				iterator_a const ae = a+n;
				iterator_b const be = b+m;				
				while ( b != be )
				{
					typename std::iterator_traits<iterator_b>::value_type const bchar = *(b++);
					
					assert ( (p-M.begin()) % n1 == 0 );
					assert ( (q-M.begin()) % n1 == 0 );
					
					// top
					*p = element_type(q->first-penalty_ins,STEP_INS);
					
					for ( iterator_a aa = a; aa != ae; ++aa )
					{
						// left
						similarity_type const left =  p->first - penalty_del;
						// diagonal match?
						bool const dmatch = (*aa == bchar);
						// diagonal
						similarity_type const diag =
							dmatch 
							?
							(q->first + gain_match)
							:
							(q->first - penalty_subst);
						// move pointer in row above
						q++;
						// top
						similarity_type const top = q->first - penalty_ins;
						// move pointer in current row
						p++;
						
						if ( diag >= left )
						{
							if ( diag >= top )
								// diag
								*p = element_type(diag,dmatch ? STEP_MATCH : STEP_MISMATCH);
							else
								// top
								*p = element_type(top,STEP_INS);
						}
						else
						{
							if ( left >= top )
								// left
								*p = element_type(left,STEP_DEL);
							else
								// top
								*p = element_type(top,STEP_INS);
						}
					}	
					
					p++;
					q++;				
				}
				
				b -= m;

				uint64_t i = n;
				uint64_t j = m;
				element_type * pq = M.get() + j*n1 + i;

				ta = te;
				
				uint64_t numdel = 0;
				uint64_t numins = 0;
				uint64_t nummat = 0;
				uint64_t nummis = 0;

				while ( pq != M.begin() )
				{
					*(--ta) = pq->second;
					
					switch ( pq->second )
					{
						// previous row
						case STEP_INS:
							pq -= n1;
							numins++;
							break;
						// previos column
						case STEP_DEL:
							pq -= 1;
							numdel++;
							break;
						// diagonal
						case STEP_MATCH:
							pq -= (n1+1);
							nummat++;
							break;
						// diagonal
						case STEP_MISMATCH:
							pq -= (n1+1);
							nummis++;
							break;
						default:
							break;
					}
				}
				
				return EditDistanceResult(numins,numdel,nummat,nummis);
			}	
		};
	}
}
#endif

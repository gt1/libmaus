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
#if ! defined(LIBMAUS_BAMBAM_BAMDECOMPRESSIONCONTROL_HPP)
#define LIBMAUS_BAMBAM_BAMDECOMPRESSIONCONTROL_HPP

#include <libmaus/bambam/BamAlignmentDecoderBase.hpp>
#include <libmaus/bambam/BamHeaderParserState.hpp>
#include <libmaus/bambam/BamHeaderLowMem.hpp>
#include <libmaus/lz/BgzfInflateBase.hpp>
#include <libmaus/lz/BgzfInflateHeaderBase.hpp>
#include <libmaus/parallel/PosixSpinLock.hpp>
#include <libmaus/parallel/PosixMutex.hpp>
#include <libmaus/parallel/LockedFreeList.hpp>
#include <libmaus/util/FreeList.hpp>
#include <libmaus/util/GetObject.hpp>
#include <libmaus/util/GrowingFreeList.hpp>

#include <libmaus/timing/RealTimeClock.hpp>

namespace libmaus
{
	namespace bambam
	{
		struct BamParallelDecodingInputBlock
		{
			typedef BamParallelDecodingInputBlock this_type;
			typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;
		
			//! input data
			libmaus::lz::BgzfInflateHeaderBase inflateheaderbase;
			//! size of block payload
			uint64_t payloadsize;
			//! decompressed data
			libmaus::autoarray::AutoArray<char> C;
			//! size of decompressed data
			uint64_t uncompdatasize;
			//! true if this is the last block in the stream
			bool final;
			
			BamParallelDecodingInputBlock() 
			: 
				inflateheaderbase(),
				payloadsize(0),
				C(libmaus::lz::BgzfConstants::getBgzfMaxBlockSize(),false), 
				uncompdatasize(0),
				final(false) 
			{}
			
			template<typename stream_type>
			void readBlock(stream_type & stream)
			{
				payloadsize = inflateheaderbase.readHeader(stream);
				
				// read block
				stream.read(C.begin(),payloadsize + 8);

				if ( stream.gcount() != static_cast<int64_t>(payloadsize + 8) )
				{
					::libmaus::exception::LibMausException se;
					se.getStream() << "BamParallelDecodingInputBlock::readBlock(): unexpected eof" << std::endl;
					se.finish(false);
					throw se;
				}
				
				uncompdatasize = 
					(static_cast<uint64_t>(static_cast<uint8_t>(C[payloadsize+4])) << 0)
					|
					(static_cast<uint64_t>(static_cast<uint8_t>(C[payloadsize+5])) << 8)
					|
					(static_cast<uint64_t>(static_cast<uint8_t>(C[payloadsize+6])) << 16)
					|
					(static_cast<uint64_t>(static_cast<uint8_t>(C[payloadsize+7])) << 24);

				if ( uncompdatasize > libmaus::lz::BgzfConstants::getBgzfMaxBlockSize() )
				{
					::libmaus::exception::LibMausException se;
					se.getStream() << "BamParallelDecodingInputBlock::readBlock(): uncompressed size is too large";
					se.finish(false);
					throw se;									
				}
				
				final = (uncompdatasize == 0) && (stream.peek() < 0);
			}
		};
		
		struct BamParallelDecodingDecompressedBlock
		{
			typedef BamParallelDecodingDecompressedBlock this_type;
			typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;
			
			//! decompressed data
			libmaus::autoarray::AutoArray<char> D;
			//! size of uncompressed data
			uint64_t uncompdatasize;
			//! next byte pointer
			char const * P;
			//! true if this is the last block in the stream
			bool final;
			
			BamParallelDecodingDecompressedBlock() 
			: 
				D(libmaus::lz::BgzfConstants::getBgzfMaxBlockSize(),false), 
				uncompdatasize(0), 
				P(0),
				final(false)
			{}
			
			uint64_t decompressBlock(
				libmaus::parallel::LockedFreeList<libmaus::lz::BgzfInflateZStreamBase> & deccont,
				char * in,
				unsigned int const inlen,
				unsigned int const outlen
			)
			{
				libmaus::lz::BgzfInflateZStreamBase * decoder = deccont.get();				
				decoder->zdecompress(reinterpret_cast<uint8_t *>(in),inlen,D.begin(),outlen);
				deccont.put(decoder);
				
				return outlen;
			}
			
			uint64_t decompressBlock(
				libmaus::parallel::LockedFreeList<libmaus::lz::BgzfInflateZStreamBase> & deccont,
				BamParallelDecodingInputBlock & inblock
			)
			{
				uncompdatasize = inblock.uncompdatasize;
				final = inblock.final;
				P = D.begin();
				return decompressBlock(deccont,inblock.C.begin(),inblock.payloadsize,uncompdatasize);
			}
		};
		
		struct BamParallelDecodingAlignmentBuffer
		{
			typedef uint64_t pointer_type;
		
			libmaus::autoarray::AutoArray<uint8_t> A;
			
			uint8_t * pA;
			pointer_type * pP;
			
			uint64_t pointerdif;
			
			BamParallelDecodingAlignmentBuffer(uint64_t const buffersize, uint64_t const rpointerdif = 1)
			: A(buffersize,false), pA(A.begin()), pP(reinterpret_cast<pointer_type *>(A.end())), pointerdif(rpointerdif)
			{
				assert ( pointerdif >= 1 );
			}
			
			bool empty() const
			{
				return pA == A.begin();
			}
			
			uint64_t fill() const
			{
				return (reinterpret_cast<pointer_type const *>(A.end()) - pP) / pointerdif;
			}
			
			void reset()
			{
				pA = A.begin();
				pP = reinterpret_cast<pointer_type *>(A.end());
			}
			
			uint64_t free() const
			{
				return reinterpret_cast<uint8_t *>(pP) - pA;
			}
			
			uint32_t decodeLength(uint64_t const off) const
			{
				uint8_t const * B = A.begin() + off;
				
				return
					(static_cast<uint32_t>(B[0]) << 0)  |
					(static_cast<uint32_t>(B[1]) << 8)  |
					(static_cast<uint32_t>(B[2]) << 16) |
					(static_cast<uint32_t>(B[3]) << 24) ;
			}
			
			void reorder()
			{
				// compact pointer array if pointerdif>1
				if ( pointerdif > 1 )
				{
					pointer_type * i = pP;
					pointer_type * j = pP;
					
					while ( i != reinterpret_cast<pointer_type *>(A.end()) )
					{
						*(j++) = *i;
						i += pointerdif;
					}
				}
				
				std::reverse(pP,pP+fill());
			}
			
			bool checkValidPacked() const
			{
				uint64_t const f = fill();
				bool ok = true;
				
				for ( uint64_t i = 0; ok && i < f; ++i )
				{
					libmaus::bambam::libmaus_bambam_alignment_validity val = 
						libmaus::bambam::BamAlignmentDecoderBase::valid(
							A.begin() + pP[i] + 4,
							decodeLength(pP[i])
					);
				
					ok = ok && ( val == libmaus::bambam::libmaus_bambam_alignment_validity_ok );
				}
				
				return ok;		
			}

			bool checkValidUnpacked() const
			{
				uint64_t const f = fill();
				bool ok = true;
				
				for ( uint64_t i = 0; ok && i < f; ++i )
				{
					libmaus::bambam::libmaus_bambam_alignment_validity val = 
						libmaus::bambam::BamAlignmentDecoderBase::valid(
							A.begin() + pP[pointerdif*i] + 4,
							decodeLength(pP[pointerdif*i])
					);
				
					ok = ok && ( val == libmaus::bambam::libmaus_bambam_alignment_validity_ok );
				}
				
				return ok;		
			}
			
			bool put(char const * p, uint64_t const n)
			{
				if ( n + sizeof(uint32_t) + pointerdif * sizeof(pointer_type) <= free() )
				{
					// store pointer
					pP -= pointerdif;
					*pP = pA-A.begin();

					// store length
					*(pA++) = (n >>  0) & 0xFF;
					*(pA++) = (n >>  8) & 0xFF;
					*(pA++) = (n >> 16) & 0xFF;
					*(pA++) = (n >> 24) & 0xFF;
					// copy alignment data
					// std::copy(p,p+n,reinterpret_cast<char *>(pA));
					memcpy(pA,p,n);
					pA += n;
														
					return true;
				}
				else
				{
					return false;
				}
			}
		};

		struct BamParallelDecodingParseInfo
		{
			typedef BamParallelDecodingParseInfo this_type;
			typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;
			
			libmaus::bambam::BamHeaderParserState BHPS;
			bool headerComplete;
			libmaus::bambam::BamHeaderLowMem::unique_ptr_type Pheader;
			
			libmaus::autoarray::AutoArray<char> putbackBuffer;
			unsigned int putbackBufferFill;

			libmaus::autoarray::AutoArray<char> concatBuffer;
			unsigned int concatBufferFill;
			
			enum parser_state_type {
				parser_state_read_blocklength,
				parser_state_read_block
			};
			
			parser_state_type parser_state;
			unsigned int blocklengthread;
			uint32_t blocklen;
			
			BamParallelDecodingParseInfo()
			: BHPS(), headerComplete(false),
			  putbackBuffer(), putbackBufferFill(0),
			  concatBuffer(), concatBufferFill(0), 
			  parser_state(parser_state_read_blocklength),
			  blocklengthread(0), blocklen(0)
			{
			
			}
			
			static uint32_t getLE4(char const * A)
			{
				unsigned char const * B = reinterpret_cast<unsigned char const *>(A);
				
				return
					(static_cast<uint32_t>(B[0]) << 0)  |
					(static_cast<uint32_t>(B[1]) << 8)  |
					(static_cast<uint32_t>(B[2]) << 16) |
					(static_cast<uint32_t>(B[3]) << 24) ;
			}

			bool parseBlock(
				BamParallelDecodingDecompressedBlock & block,
				BamParallelDecodingAlignmentBuffer & algnbuf
			)
			{
				while ( ! headerComplete )
				{
					libmaus::util::GetObject<uint8_t const *> G(reinterpret_cast<uint8_t const *>(block.P));
					std::pair<bool,uint64_t> Q = BHPS.parseHeader(G,block.uncompdatasize);
					
					block.P += Q.second;
					block.uncompdatasize -= Q.second;
					
					if ( Q.first )
					{
						headerComplete = true;
						libmaus::bambam::BamHeaderLowMem::unique_ptr_type Theader(
							libmaus::bambam::BamHeaderLowMem::constructFromText(
								BHPS.text.begin(),
								BHPS.text.begin()+BHPS.l_text
							)
						);
						Pheader = UNIQUE_PTR_MOVE(Theader);						
					}
				}
			
				if ( putbackBufferFill )
				{
					if ( ! (algnbuf.put(putbackBuffer.begin(),putbackBufferFill)) )
						return false;

					putbackBufferFill = 0;
				}
				
				// concat buffer contains data
				if ( concatBufferFill )
				{
					// parser state should be reading block
					assert ( parser_state == parser_state_read_block );
					
					// number of bytes to copy
					uint64_t const tocopy = std::min(
						static_cast<uint64_t>(blocklen - concatBufferFill),
						static_cast<uint64_t>(block.uncompdatasize)
					);
					
					// make sure there is sufficient space
					if ( concatBufferFill + tocopy > concatBuffer.size() )
						concatBuffer.resize(concatBufferFill + tocopy);

					// copy bytes
					std::copy(block.P,block.P+tocopy,concatBuffer.begin()+concatBufferFill);
					
					// adjust pointers
					concatBufferFill += tocopy;
					block.uncompdatasize -= tocopy;
					block.P += tocopy;
					
					if ( concatBufferFill == blocklen )
					{
						if ( ! (algnbuf.put(concatBuffer.begin(),concatBufferFill)) )
							return false;

						concatBufferFill = 0;
						parser_state = parser_state_read_blocklength;
						blocklengthread = 0;
						blocklen = 0;
					}
				}
				
				while ( block.uncompdatasize )
				{
					switch ( parser_state )
					{
						case parser_state_read_blocklength:
						{
							while ( 
								(!blocklengthread) && 
								(block.uncompdatasize >= 4) &&
								(
									block.uncompdatasize >= 4 + (blocklen = getLE4(block.P))
								)
							)
							{
								if ( ! (algnbuf.put(block.P+4,blocklen)) )
									return false;

								// skip
								blocklengthread = 0;
								block.uncompdatasize -= (blocklen+4);
								block.P += blocklen+4;
								blocklen = 0;
							}
							
							if ( block.uncompdatasize )
							{
								while ( blocklengthread < 4 && block.uncompdatasize )
								{
									blocklen |= static_cast<uint32_t>(*reinterpret_cast<unsigned char const *>(block.P)) << (blocklengthread*8);
									block.P++;
									block.uncompdatasize--;
									blocklengthread++;
								}
								
								if ( blocklengthread == 4 )
								{
									parser_state = parser_state_read_block;
								}
							}

							break;
						}
						case parser_state_read_block:
						{
							uint64_t const tocopy = std::min(
								static_cast<uint64_t>(blocklen),
								static_cast<uint64_t>(block.uncompdatasize)
							);
							if ( concatBufferFill + tocopy > concatBuffer.size() )
								concatBuffer.resize(concatBufferFill + tocopy);
							
							std::copy(
								block.P,
								block.P+tocopy,
								concatBuffer.begin()+concatBufferFill
							);
							
							concatBufferFill += tocopy;
							block.P += tocopy;
							block.uncompdatasize -= tocopy;
							
							// handle alignment if complete
							if ( concatBufferFill == blocklen )
							{
								if ( ! (algnbuf.put(concatBuffer.begin(),concatBufferFill)) )
									return false;
				
								blocklen = 0;
								blocklengthread = 0;
								parser_state = parser_state_read_blocklength;
								concatBufferFill = 0;
							}
						
							break;
						}
					}
				}
				
				return true;
			}
		};
		
		struct BamParallelDecodingControl
		{
			typedef BamParallelDecodingControl this_type;
			typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;

			typedef libmaus::bambam::BamParallelDecodingInputBlock input_block_type;
			typedef libmaus::bambam::BamParallelDecodingDecompressedBlock decompressed_block_type;

			uint64_t numthreads;
			
			libmaus::parallel::LockedFreeList<libmaus::lz::BgzfInflateZStreamBase>::unique_ptr_type Pdecoders;
			libmaus::parallel::LockedFreeList<libmaus::lz::BgzfInflateZStreamBase> & decoders;
			
			libmaus::parallel::LockedFreeList<input_block_type> inputBlockFreeList;			
			libmaus::parallel::LockedFreeList<decompressed_block_type> decompressedBlockFreeList;
			
			libmaus::parallel::PosixSpinLock readLock;

			bool eof;
			
			BamParallelDecodingControl(
				uint64_t rnumthreads
			)
			: 
				numthreads(rnumthreads),
				Pdecoders(new libmaus::parallel::LockedFreeList<libmaus::lz::BgzfInflateZStreamBase>(numthreads)),
				decoders(*Pdecoders),
				inputBlockFreeList(numthreads),
				decompressedBlockFreeList(numthreads),
				readLock(),
				eof(false)
			{
			
			}
			
			void serialTestDecode(std::istream & in, std::ostream & out)
			{
				libmaus::timing::RealTimeClock rtc; rtc.start();
			
				bool running = true;
				BamParallelDecodingParseInfo BPDP;
				// BamParallelDecodingAlignmentBuffer BPDAB(2*1024ull*1024ull*1024ull,2);
				BamParallelDecodingAlignmentBuffer BPDAB(1024ull*1024ull,2);
				uint64_t pcnt = 0;
				uint64_t cnt = 0;
				
				while ( running )
				{
					input_block_type * inblock = inputBlockFreeList.get();
					
					inblock->readBlock(in);
					
					decompressed_block_type * outblock = decompressedBlockFreeList.get();
										
					outblock->decompressBlock(decoders,*inblock);

					running = running && (!(outblock->final));
					
					while ( ! BPDP.parseBlock(*outblock,BPDAB) )
					{
						cnt += BPDAB.fill();
						BPDAB.reset();
						
						if ( cnt / (1024*1024) != pcnt / (1024*1024) )
						{
							std::cerr << "cnt=" << cnt << " als/sec=" << (static_cast<double>(cnt) / rtc.getElapsedSeconds()) << std::endl;
							pcnt = cnt;			
						}
					}
										
					// out.write(outblock->P,outblock->uncompdatasize);
					
					decompressedBlockFreeList.put(outblock);
					inputBlockFreeList.put(inblock);	
				}
				
				cnt += BPDAB.fill();
				
				std::cerr << "cnt=" << cnt << " als/sec=" << (static_cast<double>(cnt) / rtc.getElapsedSeconds()) << std::endl;
			}
			
			static void serialTestDecode1(std::istream & in, std::ostream & out)
			{
				BamParallelDecodingControl BPDC(1);
				BPDC.serialTestDecode(in,out);
			}
		};	
	}
}
#endif
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
#if ! defined(LIBMAUS_BAMBAM_PARALLEL_PAIRREADENDSMERGEWORKPACKAGEDISPATCHER_HPP)
#define LIBMAUS_BAMBAM_PARALLEL_PAIRREADENDSMERGEWORKPACKAGEDISPATCHER_HPP

#include <libmaus/bambam/parallel/PairReadEndsMergeWorkPackageReturnInterface.hpp>
#include <libmaus/bambam/parallel/PairReadEndsMergeWorkPackageFinishedInterface.hpp>
#include <libmaus/bambam/parallel/AddDuplicationMetricsInterface.hpp>
#include <libmaus/bambam/ReadEndsBlockIndexSet.hpp>
#include <libmaus/parallel/SimpleThreadWorkPackageDispatcher.hpp>

namespace libmaus
{
	namespace bambam
	{
		namespace parallel
		{
			struct PairReadEndsMergeWorkPackageDispatcher : public libmaus::parallel::SimpleThreadWorkPackageDispatcher
			{
				typedef PairReadEndsMergeWorkPackageDispatcher this_type;
				typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
				typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;
				
				PairReadEndsMergeWorkPackageReturnInterface & packageReturnInterface;
				PairReadEndsMergeWorkPackageFinishedInterface & mergeFinishedInterface;
				AddDuplicationMetricsInterface & addDuplicationMetricsInterface;
						
				PairReadEndsMergeWorkPackageDispatcher(
					PairReadEndsMergeWorkPackageReturnInterface & rpackageReturnInterface,
					PairReadEndsMergeWorkPackageFinishedInterface & rmergeFinishedInterface,
					AddDuplicationMetricsInterface & raddDuplicationMetricsInterface
				) : libmaus::parallel::SimpleThreadWorkPackageDispatcher(), 
				    packageReturnInterface(rpackageReturnInterface), mergeFinishedInterface(rmergeFinishedInterface),
				    addDuplicationMetricsInterface(raddDuplicationMetricsInterface)
				{
				
				}
				virtual ~PairReadEndsMergeWorkPackageDispatcher() {}
				virtual void dispatch(libmaus::parallel::SimpleThreadWorkPackage * P, libmaus::parallel::SimpleThreadPoolInterfaceEnqueTermInterface & /* tpi */)
				{
					PairReadEndsMergeWorkPackage * BP = dynamic_cast<PairReadEndsMergeWorkPackage *>(P);
					assert ( BP );

					ReadEndsBlockIndexSet pairindexset(*(BP->REQ.MI));
					libmaus::bambam::DupSetCallbackSharedVector dvec(*(BP->REQ.dupbitvec));
							
					pairindexset.merge(
						BP->REQ.SMI,
						libmaus::bambam::DupMarkBase::isDupPair,
						libmaus::bambam::DupMarkBase::markDuplicatePairs,
						dvec);

					addDuplicationMetricsInterface.addDuplicationMetrics(dvec.metrics);
										
					mergeFinishedInterface.pairReadEndsMergeWorkPackageFinished(BP);
					packageReturnInterface.pairReadEndsMergeWorkPackageReturn(BP);
				}
			};
		}
	}
}
#endif
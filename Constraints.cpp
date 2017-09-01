#include "Constraints.hpp"

// return whether this constraint is compatible with the subexons.
bool Constraints::ConvertAlignmentToBitTable( struct _pair *segments, int segCnt, 
	struct _subexon *subexons, int seCnt, int seStart, struct _constraint &ct ) 
{
	int i, j, k ;
	k = seStart ;
	ct.vector.Init( seCnt ) ;
	// Each segment of an alignment can cover several subexons.
	// But the first and last segment can partially cover a subexon.
	for ( i = 0 ; i < segCnt ; ++i )
	{
		int leftIdx, rightIdx ; // the range of subexons covered by this segment.
		leftIdx = -1 ;
		rightIdx = -1 ;
		
		for ( ; k < seCnt ; ++k )
		{
			//if ( segments[0].b == 110282529 && segCnt == 2 )
			//	printf( "(%d:%d %d):(%d:%d %d)\n", i, (int)segments[i].a, (int)segments[i].b, k, (int)subexons[k].start, (int)subexons[k].end ) ;
			if ( subexons[k].start > segments[i].b )
				break ;
			if ( segments[i].a > subexons[k].end )
				continue ;
			
			if ( ( subexons[k].start >= segments[i].a && subexons[k].end <= segments[i].b ) 
				|| ( i == 0 && subexons[k].start < segments[i].a && subexons[k].end <= segments[i].b ) 
				|| ( i == segCnt - 1 && subexons[k].start >= segments[i].a && subexons[k].end > segments[i].b ) 
				|| ( i == 0 && i == segCnt - 1 && subexons[k].start < segments[i].a && subexons[k].end > segments[i].b ) )
			{
				if ( leftIdx == -1 )
					leftIdx = k ;
				rightIdx = k ;
				ct.vector.Set( k ) ;
			}
			else
			{
				return false ;
			}
		}

		if ( leftIdx == -1 )
			return false ;
		
		// The cover contradict the boundary.
		if ( !( ( subexons[leftIdx].leftType == 0 || subexons[leftIdx].start <= segments[i].a )
			&& ( subexons[rightIdx].rightType == 0 || subexons[rightIdx].end >= segments[i].b ) ) )
			return false ;

		// The intron must exists in the subexon graph.
		if ( i > 0 )
		{
			for ( j = 0 ; j < subexons[ ct.last ].nextCnt ; ++j )
				if ( subexons[ct.last].next[j] == leftIdx  )
					break ;
			if ( j >= subexons[ ct.last ].nextCnt )
				return false ;
		}

		if ( i == 0 )
			ct.first = leftIdx ;
		ct.last = rightIdx ;
	}

	return true ;
}

void Constraints::CoalesceSameConstraints()
{
	int i, k ;
	int size = constraints.size() ;
	for ( i = 0 ; i < size ; ++i )
	{
		constraints[i].info = i ;
		//printf( "constraints %d: %d %d %d\n", i, constraints[i].vector.Test( 0 ), constraints[i].vector.Test(1), constraints[i].support ) ;
	}

	std::vector<int> newIdx ;
	newIdx.resize( size, 0 ) ;
	
	// Update the constraints.
	if ( size > 0 )
	{
		std::sort( constraints.begin(), constraints.end(), CompSortConstraints ) ;

		k = 0 ;
		newIdx[ constraints[0].info ] = 0 ;
		for ( i = 1 ; i < size ; ++i )
		{
			if ( constraints[k].vector.IsEqual( constraints[i].vector ) )
			{
				constraints[k].support += constraints[i].support ;
				constraints[k].uniqSupport += constraints[i].uniqSupport ;
				constraints[k].maxReadLen = ( constraints[k].maxReadLen > constraints[i].maxReadLen ) ?
								constraints[k].maxReadLen : constraints[i].maxReadLen ;
				constraints[i].vector.Release() ;
			}
			else
			{
				++k ;
				if ( k != i )
					constraints[k] = constraints[i] ;
			}
			newIdx[ constraints[i].info ] = k ;
		}
		constraints.resize( k + 1 ) ;
	}

	// Update the mate pairs.
	size = matePairs.size() ;
	if ( size > 0 )
	{
		for ( i = 0 ; i < size ; ++i )
		{
			//printf( "%d %d: %d => %d | %d =>%d\n", i, newIdx.size(), matePairs[i].i, newIdx[ matePairs[i].i ],
			//	matePairs[i].j, newIdx[ matePairs[i].j ] ) ;
			matePairs[i].i = newIdx[ matePairs[i].i ] ;
			matePairs[i].j = newIdx[ matePairs[i].j ] ;
		}

		std::sort( matePairs.begin(), matePairs.end(), CompSortMatePairs ) ;

		k = 0 ;
		for ( i = 1 ; i < size ; ++i )
		{
			if ( matePairs[i].i == matePairs[k].i && matePairs[i].j == matePairs[k].j )
			{
				matePairs[k].support += matePairs[i].support ;
				matePairs[k].uniqSupport += matePairs[i].uniqSupport ;
			}
			else
			{
				++k ;
				matePairs[k] = matePairs[i] ;
			}
		}
		//printf( "%s: %d\n", __func__, matePairs[1].i) ;
		matePairs.resize( k + 1 ) ;
	}

	// Update the data structure for future mate pairs.
	mateReadIds.UpdateIdx( newIdx ) ;
}

void Constraints::ComputeNormAbund( struct _subexon *subexons )
{
	int i, j ;
	int ctSize = constraints.size() ;
	for ( i = 0 ; i < ctSize ; ++i )
	{
		// spanned more than 2 subexon
		int readLen = constraints[i].maxReadLen ;
		if ( constraints[i].first + 1 < constraints[i].last )
		{
			std::vector<int> subexonInd ;
			constraints[i].vector.GetOnesIndices( subexonInd ) ;
			int size = subexonInd.size() ;
			
			for ( j = 1 ; j < size - 1 ; ++j )
			{
				int a = subexonInd[j] ;
				readLen -= subexons[a].end - subexons[a].start + 1 ;
			}
		}

		int effectiveLength ; 
		if ( constraints[i].first == constraints[i].last )
		{
			effectiveLength = ( subexons[ constraints[i].first ].end - readLen + 1 )- subexons[ constraints[i].first ].start + 1 ;
			if ( effectiveLength <= 0 ) // this happens in the 3',5'-end subexon, where we trimmed the length
				effectiveLength = ( subexons[ constraints[i].first ].end - subexons[ constraints[i].first ].start + 1 ) / 2 ;  
		}
		else
		{
			int a = constraints[i].first ;
			int b = constraints[i].last ;
			int start, end ; // the range of the possible start sites of a read
			start = subexons[a].end + 1 - ( readLen - 1 ) ;
			if ( start < subexons[a].start )
				start = subexons[a].start ;

			if ( subexons[b].end - subexons[b].start + 1 >= readLen - 1 )
				end = subexons[a].end ;
			else
				end = subexons[a].end + 1 - ( readLen - ( subexons[b].end - subexons[b].start + 1 ) ) ;

			if ( end < start ) // when we trimmed the subexon.
				end = subexons[a].end ;

			effectiveLength = end - start + 1 ;
		}
		
		constraints[i].normAbund = (double)constraints[i].support / (double)effectiveLength ;
		constraints[i].abundance = constraints[i].normAbund ;
	}

	ctSize = matePairs.size() ;
	for ( i = 0 ; i < ctSize ; ++i )
	{
		double a = constraints[ matePairs[i].i ].normAbund ;
		double b = constraints[ matePairs[i].j ].normAbund ;
		
		matePairs[i].normAbund = a < b ? a : b ;
		matePairs[i].abundance = matePairs[i].normAbund ;
	}
}

int Constraints::BuildConstraints( struct _subexon *subexons, int seCnt, int start, int end )
{
	int i ;
	int tag = 0 ;
	int coalesceThreshold = 16384 ;
	Alignments &alignments = *pAlignments ;
	// Release the memory from previous gene.
	int size = constraints.size() ;
	
	if ( size > 0 )
	{
		for ( i = 0 ; i < size ; ++i )
			constraints[i].vector.Release() ;
		std::vector<struct _constraint>().swap( constraints ) ;
	}
	std::vector<struct _matePairConstraint>().swap( matePairs ) ;
	mateReadIds.Clear() ;
	
	// Start to build the constraints. 
	while ( alignments.Next() )
	{
		if ( alignments.GetChromId() < subexons[0].chrId )
			continue ;
		else if ( alignments.GetChromId() > subexons[0].chrId )
			break ;
		// locate the first subexon in this region that overlapps with current alignment.
		for ( ; tag < seCnt && subexons[tag].end < alignments.segments[0].a ; ++tag )
			;
		
		if ( tag >= seCnt )
			break ;
		
		if ( alignments.segments[ alignments.segCnt - 1 ].b < subexons[tag].start )
			continue ;
		struct _constraint ct ;
		ct.vector.Init( seCnt ) ;
		//printf( "%s %d: %lld-%lld | %d-%d\n", __func__, alignments.segCnt, alignments.segments[0].a, alignments.segments[0].b, subexons[tag].start, subexons[tag].end ) ;
		ct.weight = 1 ;
		ct.normAbund = 0 ;
		ct.support = 1 ;
		ct.uniqSupport = alignments.IsUnique() ? 1 : 0 ;
		ct.maxReadLen = alignments.GetReadLength() ;
		
		//printf( "%s\n", alignments.GetReadId() ) ;
		if ( ConvertAlignmentToBitTable( alignments.segments, alignments.segCnt, 
				subexons, seCnt, tag, ct ) )
		{

			//printf( "%s\n", alignments.GetReadId() ) ;
			constraints.push_back( ct ) ; // if we just coalesced but the list size does not decrease, this will force capacity increase.
			//printf( "ct.vector: %d %d\n", ct.vector.Test(0), ct.vector.Test(1) ) ;
			// Add the mate-pair information.
			int mateChrId ;
			int64_t matePos ;
			alignments.GetMatePosition( mateChrId, matePos ) ;
			if ( alignments.GetChromId() == mateChrId )
			{
				
				if ( matePos < alignments.segments[0].a )
				{
					int mateIdx = mateReadIds.Query( alignments.GetReadId(), alignments.segments[0].a ) ;
					if ( mateIdx != -1 )
					{
						struct _matePairConstraint nm ;
						nm.i = mateIdx ;
						nm.j = constraints.size() - 1 ;
						nm.abundance = 0 ;
						nm.support = 1 ;
						nm.uniqSupport = alignments.IsUnique() ? 1 : 0 ;				
						nm.effectiveCount = 2 ;
						matePairs.push_back( nm ) ;
					}
				}
				else if ( matePos > alignments.segments[0].a )
				{
					mateReadIds.Insert( alignments.GetReadId(), alignments.segments[0].a, constraints.size() - 1, matePos ) ;					
				}
			}

			// Coalesce if necessary.
			size = constraints.size() ;			
			if ( (int)size > coalesceThreshold && size == (int)constraints.capacity() )
			{
				
				//printf( "start coalescing. %d\n", constraints.capacity() ) ;
				CoalesceSameConstraints() ;	

				// Not coalesce enough
				if ( constraints.size() >= constraints.capacity() / 2 )
				{
					coalesceThreshold *= 2 ;
				}
			}
		}
		else
		{
			//printf( "not compatible\n" ) ;
			ct.vector.Release() ;
		}
	}
	//printf( "start coalescing. %d %d\n", constraints.size(), matePairs.size() ) ;
	CoalesceSameConstraints() ;
	//printf( "after coalescing. %d %d\n", constraints.size(), matePairs.size() ) ;
	//for ( i = 0 ; i < matePairs.size() ; ++i )
	//	printf( "matePair: %d %d %d\n", matePairs[i].i, matePairs[i].j, matePairs[i].support ) ;
	// single-end data set
	if ( matePairs.size() == 0 )
	{
		int size = constraints.size() ;
		
		for ( i = 0 ; i < size ; ++i )
		{
			struct _matePairConstraint nm ;
			nm.i = i ;
			nm.j = i ;
			nm.abundance = 0 ;
			nm.support = constraints[i].support ;
			nm.uniqSupport = constraints[i].uniqSupport ;
			nm.effectiveCount = 1 ;

			matePairs.push_back( nm ) ;
		}
	}
	
	ComputeNormAbund( subexons ) ;

	/*for ( i = 0 ; i < constraints.size() ; ++i )
	{
		printf( "constraints %d: %lf %d %d ", i, constraints[i].normAbund, constraints[i].first, constraints[i].last ) ;
		constraints[i].vector.Print() ;
	}*/

	return 0 ;
}
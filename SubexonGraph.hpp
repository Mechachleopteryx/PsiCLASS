#ifndef _MOURISL_CLASSES_SUBEXONGRAPH_HEADER
#define _MOURISL_CLASSES_SUBEXONGRAPH_HEADER

#include "alignments.hpp"
#include "blocks.hpp"

struct _subexon
{
	char chrId ;
	int start, end ;
	int leftType, rightType ;
	double avgDepth ;
	//double ratio, classifier ;
	double leftRatio, rightRatio ;
	double leftClassifier, rightClassifier ;
	int lcCnt, rcCnt ;
	int leftStrand, rightStrand ;
	
	int nextCnt, prevCnt ;
	int *next, *prev ;
} ;

struct _geneInterval
{
	int startIdx, endIdx ;
	int start, end ; // The start and end of a gene interval might be adjusted, so it does not 
			// need to be match with the corresponding subexons
} ;

class SubexonGraph
{
private:
	int *visit ;
	double classifierThreshold ;

	void GetGeneBoundary( int tag, int strand, int &boundary, int timeStamp ) ;
public:
	std::vector<struct _subexon> subexons ;
	std::vector<struct _geneInterval> geneIntervals ;

	~SubexonGraph() {} 

	SubexonGraph( double classifierThreshold, Alignments &bam, FILE *fpSubexon ) 
	{ 
		// Read in the subexons
		rewind( fpSubexon ) ; 
		char buffer[2048] ;
		int subexonCnt ;
		int i, j, k ;
		while ( fgets( buffer, sizeof( buffer ), fpSubexon ) != NULL )
		{
			if ( buffer[0] == '#' )
				continue ;

			struct _subexon se ;
			InputSubexon( buffer, bam, se, true ) ;

			// filter.
			if ( ( se.leftType == 0 && se.rightType == 0 ) 
				|| ( se.leftType == 0 && se.rightType == 1 ) 	// overhang
				|| ( se.leftType == 2 && se.rightType == 0 ) // overhang
				|| ( se.leftType == 2 && se.rightType == 1 ) ) // ir
			{
				if ( se.leftClassifier >= classifierThreshold || se.leftClassifier < 0 )
				//if ( se.leftClassifier < classifierThreshold )
					continue ;
			}
			
			// Adjust the coordinate.
			subexons.push_back( se ) ;	
		}

		// Convert the coordinate to index
		// Note that each coordinate can only associate with one subexon.
		subexonCnt = subexons.size() ;
		for ( i = 0 ; i < subexonCnt ; ++i )
		{	
			//printf( "hi1\n" ) ;
			struct _subexon &se = subexons[i] ;
			int cnt = 0 ;

			// due to filter, we may not fully match the coordinate and the subexon
			for ( j = i - 1, k = 0 ; k < se.prevCnt && j >= 0 && subexons[j].end >= se.prev[0] ; --j )
			{
				if ( subexons[j].end == se.prev[se.prevCnt - 1 - k] ) // notice the order is reversed
				{
					se.prev[cnt] = j ;
					++k ;
					++cnt ;
				}
				else if ( subexons[j].end > se.prev[ se.prevCnt - 1 - k ] ) // the corresponding subexon gets filtered.
				{
					++k ;
					++j ; // counter the --j in the loop
				}
			}
			se.prevCnt = cnt ;
			//printf( "hi2\n" ) ;
			// Reverse the list
			for ( j = 0, k = se.prevCnt - 1 ; j < k ; ++j, --k )
			{
				int tmp = se.prev[j] ;
				se.prev[j] = se.prev[k] ;
				se.prev[k] = tmp ;
			}
			
			cnt = 0 ;
			for ( j = i + 1, k = 0 ; k < se.nextCnt && j < subexonCnt && subexons[j].start <= se.next[ se.nextCnt - 1 ] ; ++j )
			{
				if ( subexons[j].start == se.next[k] )
				{
					se.next[cnt] = j ;
					++k ;
					++cnt ;
				}
				else if ( subexons[j].start > se.next[k] ) 
				{
					++k ;
					--j ;
				}
			}
			se.nextCnt = cnt ;
		}

		// Adjust the coordinate
		int seCnt = subexons.size() ;
		for ( i = 0 ; i < seCnt ; ++i )
		{
			--subexons[i].start ;
			--subexons[i].end ;
		}
		rewind( fpSubexon ) ;
		
		visit = new int[ seCnt ] ;

		this->classifierThreshold = classifierThreshold ;
	} 
	
	static bool IsSameStrand( int a, int b )
	{
		if ( a == 0 || b == 0 )
			return true ;
		if ( a != b )
			return false ;
		return true ;
	}
	// Parse the input line
	static int InputSubexon( char *in, Alignments &alignments, struct _subexon &se, bool needPrevNext = false )
	{
		int i, k ;
		char chrName[50] ;
		char ls[3], rs[3] ;	
		sscanf( in, "%s %d %d %d %d %s %s %lf %lf %lf %lf %lf", chrName, &se.start, &se.end, &se.leftType, &se.rightType, ls, rs,
				&se.avgDepth, &se.leftRatio, &se.rightRatio, 
				&se.leftClassifier, &se.rightClassifier ) ;	
		se.chrId = alignments.GetChromIdFromName( chrName ) ;
		se.nextCnt = se.prevCnt = 0 ;
		se.next = se.prev = NULL ;
		se.lcCnt = se.rcCnt = 0 ;

		if ( ls[0] == '+' )
			se.leftStrand = 1 ;
		else if ( ls[0] == '-' )
			se.leftStrand = -1 ;
		else
			se.leftStrand = 0 ;

		if ( rs[0] == '+' )
			se.rightStrand = 1 ;
		else if ( rs[0] == '-' )
			se.rightStrand = -1 ;
		else
			se.rightStrand = 0 ;

		if ( needPrevNext )
		{
			char *p = in ;
			// Locate the offset for prevCnt
			for ( i = 0 ; i <= 11 ; ++i )
			{
				p = strchr( p, ' ' ) ;
				++p ;
			}

			sscanf( p, "%d", &se.prevCnt ) ;
			p = strchr( p, ' ' ) ;
			++p ;
			se.prev = new int[ se.prevCnt ] ;
			for ( i = 0 ; i < se.prevCnt ; ++i )
			{
				sscanf( p, "%d", &se.prev[i] ) ;
				p = strchr( p, ' ' ) ;
				++p ;
			}

			sscanf( p, "%d", &se.nextCnt ) ;
			p = strchr( p, ' ' ) ;
			++p ;
			se.next = new int[ se.nextCnt ] ;
			for ( i = 0 ; i < se.nextCnt ; ++i )
			{
				sscanf( p, "%d", &se.next[i] ) ;
				p = strchr( p, ' ' ) ;
				++p ;
			}
		}
		return 1 ;
	}
	
	int GetGeneIntervalIdx( int startIdx, int &endIdx, int timeStamp ) ;

	//@return: the number of intervals found
	int ComputeGeneIntervals() ;
} ;

#endif
#ifndef CACHE_H
#define CACHE_H

#define __STDC_LIMIT_MACROS

#include <sstream>
#include <cassert>
#include <string>

using namespace std;

#define KILO 1024

typedef long long unsigned int CACHE_STATS; // type of cache hit/miss counters

enum OPERATION {
    OPERATION_MISS = 0,
    OPERATION_HIT = 1,
    OPERATION_NUM
};

enum ACCESS_TYPE {
    ACCESS_TYPE_INSTRUCTION,
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    ACCESS_TYPE_NUM
};

enum CACHE_TYPE{
    CACHE_TYPE_ICACHE,
    CACHE_TYPE_DCACHE,
    CACHE_TYPE_NUM
};

enum STORE_ALLOCATION{
    STORE_NO_ALLOCATE,
    STORE_ALLOCATE
};


static bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

static INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0) return -1;

    if (n & 0xffff0000) { p +=  16; n >>=  16; }
    if (n & 0x0000ff00) { p +=   8; n >>=   8; }
    if (n & 0x000000f0) { p +=   4; n >>=   4; }
    if (n & 0x0000000c) { p +=   2; n >>=   2; }
    if (n & 0x00000002) { p +=   1; }

    return p;
}


class CACHE_TAG
{
  public:
    ADDRINT _tag;
    UINT64 _lastAccess;
    UINT64 _bringAccess;

    void InitTag(UINT32 words){
        _tag = 0;
        _lastAccess = 0;
        _bringAccess = 0;
    }

    void InitTagTable(UINT32 words){
        _tag = 0;
        _lastAccess = 0;
        _bringAccess = 0;
    }

    // Tag Address------------------------------------------------------
    void SetTag(ADDRINT tag){this->_tag = tag;}
    ADDRINT GetTag(){return this->_tag;}

    // LastAccess - Used for LRU ---------------------------------------
    void SetLastAccess(UINT64 lastAccess){this->_lastAccess = lastAccess;}
    UINT64 GetLastAccess() { return(this->_lastAccess); }
};


class CACHE_SET
{
  public:
    CACHE_TAG *_ways;

    void InitSet(UINT32 ways, UINT32 words) {
        UINT32 i;
        this->_ways = new CACHE_TAG[ways];
        for(i=0; i< ways; i++){
            _ways[i].InitTag(words);
        }
    }

    void InitSetTable(UINT32 ways, UINT32 words) {
        UINT32 i;
        this->_ways = new CACHE_TAG[ways];
        for(i=0; i< ways; i++){
            _ways[i].InitTagTable(words);
        }
    }

    INT32 Find_Tag(UINT32 ways, ADDRINT find_tag){
        UINT32 i;
        for(i =0; i< ways; i++){
            if (this->_ways[i]._tag == find_tag)
                return i;
        }
        return -1;
    }


    INT32 Find_LRU(UINT32 ways){
        UINT32 i;
        UINT32 old_way = 0;
        UINT64 old_clock = this->_ways[0].GetLastAccess();
        for(i=0; i < ways; i++){
             if (old_clock > this->_ways[i].GetLastAccess()){
                old_clock = this->_ways[i].GetLastAccess();
                old_way = i;
            }
        }
        return old_way;
    }

};



class CACHE_BASE
{
  private:
    // Input Parameters ------------------------------------------------
    string _name;
    UINT32 _cacheLevel;
    UINT32 _cacheSize;
    UINT32 _lineSize;
    UINT32 _associativity;
    STORE_ALLOCATION _writeAllocate;
    CACHE_TYPE _cacheType;
    CACHE_BASE *_nextCacheLevel;

    // Computed Parameters ---------------------------------------------
    UINT32 _lineShift;
    UINT32 _indexMask;
    UINT32 _offSetMask;
    UINT64 _clock;

    CACHE_SET *_sets;

    // Statistics ------------------------------------------------------
    CACHE_STATS _access[ACCESS_TYPE_NUM][OPERATION_NUM];
    CACHE_STATS _evictedLines;
    CACHE_STATS _unusedLines;
    CACHE_STATS _coldStart;
    CACHE_STATS _flushedLines;

  public:

    // allocator
    void Init(string name, CACHE_TYPE cacheType, UINT32 cacheLevel, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, STORE_ALLOCATION writeAllocate);

    // Accessors
    VOID SetCacheSize(UINT32 cacheSize) { this->_cacheSize = cacheSize; }
    VOID SetLineSize(UINT32 lineSize) { this->_lineSize = lineSize; }
    VOID SetAssociativity(UINT32 associativity) { this->_associativity = associativity; }
    VOID SetWriteAllocate(STORE_ALLOCATION writeAllocate) { this->_writeAllocate = writeAllocate; }
    VOID SetCacheType(CACHE_TYPE cacheType) { this->_cacheType = cacheType; }
    VOID SetNextCacheLevel(CACHE_BASE *NextCache) { this->_nextCacheLevel = NextCache; }

    UINT32 GetCacheSize() { return _cacheSize; }
    UINT32 GetLineSize() { return _lineSize; }
    UINT32 GetAssociativity() { return _associativity; }

    STORE_ALLOCATION GetWriteAllocate(){ return this->_writeAllocate; }
    CACHE_TYPE GetCacheType(){ return this->_cacheType; }
    CACHE_BASE *GetNextCacheLevel() { return this->_nextCacheLevel; }
    VOID PrintName(){ fprintf(stderr,"Name: %s\n",this->_name.c_str() );}

    OPERATION Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc);
    OPERATION Find(UINT32 index, ADDRINT tag,  UINT64 clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc);
    VOID Replace(UINT32 index, ADDRINT tag, UINT64 clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc, ADDRINT addr);


    // Statistics
    CACHE_STATS Hits(ACCESS_TYPE accessType) { return _access[accessType][OPERATION_HIT];}
    CACHE_STATS Misses(ACCESS_TYPE accessType) { return _access[accessType][OPERATION_MISS];}
    CACHE_STATS Accesses(ACCESS_TYPE accessType) { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() { return SumAccess(OPERATION_HIT);}
    CACHE_STATS Misses() { return SumAccess(OPERATION_MISS);}
    CACHE_STATS Accesses() { return Hits() + Misses();}
    CACHE_STATS SumAccess(OPERATION hit){
        CACHE_STATS sum = 0;
        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
            sum += _access[accessType][hit];
        return sum;
        }

    CACHE_STATS EvictedLines() { return _evictedLines; }
    CACHE_STATS FlushedLines() { return _flushedLines; }
    CACHE_STATS UnusedLines() { return _unusedLines; }
    CACHE_STATS ColdStart() { return _coldStart; }

    //==================================================================
    VOID SplitAddress(const ADDRINT addr, ADDRINT & tag, UINT32 & setIndex, UINT32 & setOffSet)
    {
        tag = addr >> _lineShift;
        setIndex = tag & _indexMask;
        setOffSet = addr & _offSetMask;
        #ifdef DEBUG_MODE
            if (setOffSet > this->_lineSize){ fprintf(stderr,"SplitAddress() - Assert Failed: offset+size > _lineSize");  ASSERTX(false); }
            if (setIndex >= (this->_cacheSize / (this->_associativity * this->_lineSize))){ fprintf(stderr,"SplitAddress()");  ASSERTX(false); }
         #endif
    }

};


void CACHE_BASE::Init(string name, CACHE_TYPE cacheType, UINT32 cacheLevel, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, STORE_ALLOCATION writeAllocate)
{
    CACHE_STATS i;

    this->_name = name;
    this->_cacheType = cacheType;
    this->_cacheLevel = cacheLevel;
    this->_cacheSize = cacheSize;
    this->_lineSize = lineSize;
    this->_associativity = associativity;
    this->_writeAllocate = writeAllocate;
    this->_nextCacheLevel = NULL;

    this->_clock = 0;
    this->_lineShift = FloorLog2(lineSize);
    this->_indexMask = (this->_cacheSize / (this->_associativity * this->_lineSize)) - 1;
    this->_offSetMask = this->_lineSize - 1;

    ASSERTX(this->GetCacheSize() != 0);
    ASSERTX(IsPower2(this->GetAssociativity()));
    ASSERTX(IsPower2(this->_lineSize));
    ASSERTX(IsPower2(this->_indexMask + 1));

    for (i = 0; i < ACCESS_TYPE_NUM; i++){
        this->_access[i][OPERATION_MISS] = 0;
        this->_access[i][OPERATION_HIT] = 0;
    }

    this->_sets = new CACHE_SET[this->_indexMask+1];
    for (i = 0; i < this->_indexMask+1  ; i++){
        this->_sets[i].InitSet(this->GetAssociativity(), this->_lineSize);
    }

    this->_evictedLines=0;
    this->_unusedLines=0;
    this->_flushedLines=0;
    this->_coldStart=0;
}


OPERATION CACHE_BASE::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc)
{
    const ADDRINT highAddr = addr + size;
    const ADDRINT lineSize = GetLineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);

    OPERATION allHit = OPERATION_HIT;
    OPERATION localHit = OPERATION_MISS;
    ADDRINT tag=0;
    UINT32 setIndex=0;
    UINT32 setOffSet=0;
    UINT32 sizeNow=0;

    this->_clock++;

    do {
        SplitAddress(addr, tag, setIndex, setOffSet);

        if (setOffSet+size > this->_lineSize) {//If the size ends on the next line...
            sizeNow = this->_lineSize - setOffSet;    //SizeNow = |.........xxx|xx
            size = size-sizeNow;
        }
        else {
            sizeNow = size;                     //SizeNow = |......xxxxx.|
        }
        localHit = Find(setIndex, tag, this->_clock, setOffSet, sizeNow, accessType, pc);

        // on miss, loads always allocate, stores optionally
        if ( ! localHit )
        {
            allHit = localHit;

            // Miss is send to the next level cache.
            if (this->_nextCacheLevel != NULL){
                this->_nextCacheLevel->Access(addr, sizeNow, accessType, pc);
            }

            // Solves the miss.
            if ( accessType == ACCESS_TYPE_LOAD || accessType == ACCESS_TYPE_INSTRUCTION || _writeAllocate == STORE_ALLOCATE ){
                Replace(setIndex, tag, this->_clock, setOffSet, sizeNow, accessType, pc, addr);
            }
        }

        addr = (addr & notLineMask) + lineSize; // start of next cache line request
    }
    while (addr < highAddr);

    this->_access[accessType][allHit]++;

    return allHit;
}


/* ===================================================================== */
OPERATION CACHE_BASE::Find(UINT32 index, ADDRINT tag,  UINT64 clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc)
{
    INT32 way=-1;

    OPERATION ret_op = OPERATION_MISS;

    #ifdef DEBUG_MODE
        if (offset+size > this->_lineSize){ fprintf(stderr,"Find()");  ASSERTX(false); }
        if (offset+size > MAX_LINE_SIZE){ fprintf(stderr,"Find()");  ASSERTX(false); }
    #endif

    way = this->_sets[index].Find_Tag(this->GetAssociativity(), tag);
    if (way != -1) {
        this->_sets[index]._ways[way].SetLastAccess(clock);
        ret_op = OPERATION_HIT;
    }

    return ret_op;
}



/* ===================================================================== */
VOID CACHE_BASE::Replace(UINT32 index, ADDRINT tag, UINT64 clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc, ADDRINT addr)
{
    #ifdef DEBUG_MODE
        if (offset+size > this->_lineSize || offset+size > MAX_LINE_SIZE){ fprintf(stderr,"Replace() - offset+size >= this->_lineSize");  ASSERTX(false); }
    #endif

    INT32 way=-1;

    way = this->_sets[index].Find_LRU(this->GetAssociativity());

    this->_sets[index]._ways[way].SetTag(tag);
    this->_sets[index]._ways[way].SetLastAccess(clock);

}

#endif // CACHE_H

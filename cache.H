/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
//
// @ORIGINAL_AUTHOR: Artur Klauser
// @EXTENDED: Marco Antonio Zanata Alves (marco.eu@gmail.com)
//

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHE_H
#define PIN_CACHE_H

/* ========================================================================================================================================== */
/* Includes                                                              */
/* ========================================================================================================================================== */
#define __STDC_LIMIT_MACROS

#include <sstream>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <limits>
#include <list>

using namespace std;
/* ========================================================================================================================================== */
/* Defines                                                               */
/* ========================================================================================================================================== */
#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)
#define MAX_ASSOCIATIVITY 8192
#define MAX_LINE_SIZE 64
#define SUB_BLOCK 16

#define WRONG_WHOLE
#define LEARN_WHOLE
/* ========================================================================================================================================== */
/* Type Definition                                                       */
/* ========================================================================================================================================== */
typedef long long unsigned int CACHE_STATS; // type of cache hit/miss counters


/* ========================================================================================================================================== */
/* Enumarations                                                          */
/* ========================================================================================================================================== */
enum OPERATION {
    OPERATION_MISS = 0,
    OPERATION_HIT = 1,
    OPERATION_NUM
};

// types, constants
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
} ;


/* ========================================================================================================================================== */
/* Static Functions                                                      */
/* ========================================================================================================================================== */

/* ===================================================================== */
/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/* ===================================================================== */
/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
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

/* ===================================================================== */
/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}

/* ===================================================================== */
/*!
 *  Call Example
 *  char binary[65];
 *  address_to_binary(uint64_t(this->_bankMask),binary);
 *  fprintf(stderr,"OffSet %s\n",binary);
 */
VOID address_to_binary(uint64_t x, char *ch)
{
    uint64_t z;
    char *b = ch;
    b[0] = '\0';

    for (z = (uint64_t)INT64_MAX+1; z > 0; z >>= 1){
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
}

/* ========================================================================================================================================== */
/* Classes Declaration                                                      */
/* ========================================================================================================================================== */


/* ===================================================================== */
class CACHE_TAG
{
  public:
    ADDRINT _tag;
    uint64_t _lastAccess;
    uint64_t _bringAccess;
    UINT8 *_usageVector;
    UINT8 *_validBitVector;

    void InitTag(UINT32 words){
        _tag = 0;
        _lastAccess = 0;
        _bringAccess = 0;
        _usageVector = new UINT8[words];
        _validBitVector =  new UINT8[words];
        CleanUsageVector(words);
        CleanValidBitVector(words);
    }

    void InitTagTable(UINT32 words){
        _tag = 0;
        _lastAccess = 0;
        _bringAccess = 0;
        _usageVector = new UINT8[words];
        CleanUsageVector(words);
        //_validBitVector =  new UINT8[1];
        //CleanValidBitVector(1);
    }

    // Tag Address------------------------------------------------------
    void SetTag(ADDRINT tag){this->_tag = tag;}
    ADDRINT GetTag(){return this->_tag;}

    // LastAccess - Used for LRU ---------------------------------------
    void SetLastAccess(uint64_t lastAccess){this->_lastAccess = lastAccess;}
    uint64_t GetLastAccess() { return(this->_lastAccess); }

    //------------------------------------------------------------------
    // Usage Vector ----------------------------------------------------
    //------------------------------------------------------------------
    void CleanUsageVector(UINT32 words){
        #ifdef DEBUG_MODE
            if (words > MAX_LINE_SIZE ){ fprintf(stderr,"CleanUsageVector()");  ASSERTX(false); }
        #endif

        for(UINT32 i=0; i<words; i++)
            this->_usageVector[i] = 0;
    }

    //------------------------------------------------------------------
    void SetUsageVector(UINT32 pos_ini, UINT32 pos_end){
        UINT32 ini = pos_ini; //UINT32(UINT32(pos_ini/SUB_BLOCK))*SUB_BLOCK;
        UINT32 end = pos_ini + pos_end;//(UINT32(UINT32((pos_ini+pos_end)/SUB_BLOCK))*SUB_BLOCK)+(SUB_BLOCK* (((pos_ini+pos_end)%SUB_BLOCK) != 0));
        #ifdef DEBUG_MODE
            if (end > MAX_LINE_SIZE || pos_ini+pos_end > MAX_LINE_SIZE){ fprintf(stderr,"SetUsageVector(), %u %u", pos_ini, pos_end);  ASSERTX(false); }
        #endif

        for(UINT32 i=ini; i<end; i++){
            if (this->_usageVector[i] == 0){
                this->_usageVector[i] = 1;
            }
        }
    }

    //------------------------------------------------------------------
    UINT32 CountUsageVector(UINT32 words){
        #ifdef DEBUG_MODE
            if (words > MAX_LINE_SIZE ){ fprintf(stderr,"CountUsageVector()");  ASSERTX(false); }
        #endif

        UINT32 count=0;
        for(UINT32 i=0; i<words; i++){
            if (this->_usageVector[i] != 0){
                count++;
            }
        }
        return count;
    }

    //------------------------------------------------------------------
    UINT32 CheckUsageVectorSubBlock(UINT32 subblock){
        #ifdef DEBUG_MODE
            if (subblock > MAX_LINE_SIZE/SUB_BLOCK ){ fprintf(stderr,"CheckUsageVectorSubBlock()");  ASSERTX(false); }
        #endif

        UINT32 i, count=0;
        for(i=(subblock*SUB_BLOCK); i<(subblock+1)*SUB_BLOCK; i++){
            count += (this->_usageVector[i] != 0);
        }
        return (count > 0);
    }


    //------------------------------------------------------------------
    // Valid Bit Vector ------------------------------------------------
    //------------------------------------------------------------------
    void CleanValidBitVector(UINT32 words){
        #ifdef DEBUG_MODE
            if (words > MAX_LINE_SIZE ){ fprintf(stderr,"CleanValidBitVector()");  ASSERTX(false); }
        #endif

        for(UINT32 i=0; i<words; i++){
            this->_validBitVector[i] = 0;
        }
    }

    //------------------------------------------------------------------
    void SetValidBitVectorNormal(UINT32 pos_ini, UINT32 pos_end){
        UINT32 ini = UINT32(UINT32(pos_ini/SUB_BLOCK))*SUB_BLOCK;
        UINT32 end = (UINT32(UINT32((pos_ini+pos_end)/SUB_BLOCK))*SUB_BLOCK)+(SUB_BLOCK* (((pos_ini+pos_end)%SUB_BLOCK) != 0));
        #ifdef DEBUG_MODE
            if (end > MAX_LINE_SIZE || pos_ini+pos_end > MAX_LINE_SIZE || ini > pos_ini || end < pos_ini+pos_end){ fprintf(stderr,"SetValidBitVector(), %u %u", pos_ini, pos_end);  ASSERTX(false); }
        #endif

        for(UINT32 i=ini; i<end; i++){
            if (this->_validBitVector[i] == 0)
                this->_validBitVector[i] = 1;
        }
    }

    //------------------------------------------------------------------
    void SetValidBitVectorLearn(UINT32 pos_ini, UINT32 pos_end){
        UINT32 ini = UINT32(UINT32(pos_ini/SUB_BLOCK))*SUB_BLOCK;
        UINT32 end = (UINT32(UINT32((pos_ini+pos_end)/SUB_BLOCK))*SUB_BLOCK)+(SUB_BLOCK* (((pos_ini+pos_end)%SUB_BLOCK) != 0));
        #ifdef DEBUG_MODE
            if (end > MAX_LINE_SIZE || pos_ini+pos_end > MAX_LINE_SIZE || ini > pos_ini || end < pos_ini+pos_end){ fprintf(stderr,"SetValidBitVector(), %u %u", pos_ini, pos_end);  ASSERTX(false); }
        #endif

        for(UINT32 i=ini; i<end; i++){
            if (this->_validBitVector[i] == 0)
                this->_validBitVector[i] = 2;
        }
    }

    //------------------------------------------------------------------
    void SetValidBitVectorWrongFirst(UINT32 pos_ini, UINT32 pos_end){
        UINT32 ini = UINT32(UINT32(pos_ini/SUB_BLOCK))*SUB_BLOCK;
        UINT32 end = (UINT32(UINT32((pos_ini+pos_end)/SUB_BLOCK))*SUB_BLOCK)+(SUB_BLOCK* (((pos_ini+pos_end)%SUB_BLOCK) != 0));
        #ifdef DEBUG_MODE
            if (end > MAX_LINE_SIZE || pos_ini+pos_end > MAX_LINE_SIZE || ini > pos_ini || end < pos_ini+pos_end){ fprintf(stderr,"SetValidBitVector(), %u %u", pos_ini, pos_end);  ASSERTX(false); }
        #endif

        for(UINT32 i=ini; i<end; i++){
            if (this->_validBitVector[i] == 0)
                this->_validBitVector[i] = 3;
        }
    }

    //------------------------------------------------------------------
    UINT32 CountValidBitVector(UINT32 words){
        #ifdef DEBUG_MODE
            if (words > MAX_LINE_SIZE ){ fprintf(stderr,"CountUsageVector()");  ASSERTX(false); }
        #endif

        UINT32 count=0;
        for(UINT32 i=0; i<words; i++){
            if (this->_validBitVector[i] != 0){
                count++;
            }
        }

        count /= SUB_BLOCK;
        #ifdef DEBUG_MODE
            if (count > SUB_BLOCK){ fprintf(stderr,"CountValidBitVector(), %u %u", count, words);  ASSERTX(false); }
        #endif

        return count;
    }


    //------------------------------------------------------------------
    OPERATION CheckValidBitVector(UINT32 pos_ini, UINT32 pos_end){
        UINT32 ini = pos_ini;//UINT32(UINT32(pos_ini/SUB_BLOCK))*SUB_BLOCK;
        UINT32 end = pos_ini + pos_end;//(UINT32(UINT32((pos_ini+pos_end)/SUB_BLOCK))*SUB_BLOCK)+(SUB_BLOCK* (((pos_ini+pos_end)%SUB_BLOCK) != 0));
        #ifdef DEBUG_MODE
            if (end > MAX_LINE_SIZE || pos_ini+pos_end > MAX_LINE_SIZE){ fprintf(stderr,"CheckValidBitVector(), %u %u", pos_ini, pos_end);  ASSERTX(false); }
        #endif

        for(UINT32 i=ini; i<end; i++){
            if (this->_validBitVector[i] == 0){
                return OPERATION_MISS;
            }
        }

        return OPERATION_HIT;
    }

};


/* ===================================================================== */
class CACHE_SET
{
  public:
    CACHE_TAG *_ways;
    CACHE_STATS *_tmp;

    void InitSet(UINT32 ways, UINT32 words) {
        UINT32 i;
        this->_ways = new CACHE_TAG[ways];
        this->_tmp = new CACHE_STATS[2*ways];
        for(i=0; i< ways; i++){
            _ways[i].InitTag(words);
        }
        for(i=0; i< 2*ways; i++){
            _tmp[i] = 0;
        }
    }

    void InitSetTable(UINT32 ways, UINT32 words) {
        UINT32 i;
        this->_ways = new CACHE_TAG[ways];
        //this->_tmp = new CACHE_STATS[1];
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
        uint64_t old_clock = this->_ways[0].GetLastAccess();
        for(i=0; i < ways; i++){
             if (old_clock > this->_ways[i].GetLastAccess()){
                old_clock = this->_ways[i].GetLastAccess();
                old_way = i;
            }
        }
        return old_way;
    }

};



/* ===================================================================== */
/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
  private:
    // Input Parameters ------------------------------------------------
    std::string _name;
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
    uint64_t _clock;

    CACHE_SET *_sets;

    // Statistics ------------------------------------------------------
    CACHE_STATS _access[ACCESS_TYPE_NUM][OPERATION_NUM];
    CACHE_STATS **_usedBlocks;
    CACHE_STATS *_requestedBlocks;
    CACHE_STATS _evictedLines;
    CACHE_STATS _unusedLines;
    CACHE_STATS _coldStart;
    CACHE_STATS _flushedLines;
    CACHE_STATS _bringWrongTime;
    CACHE_STATS _bringWrongNumber;

    CACHE_STATS _predictCorrect;
    CACHE_STATS _predictCorrectNormal;
    CACHE_STATS _predictCorrectLearn;
    CACHE_STATS _predictCorrectWrongFirst;

    CACHE_STATS _predictOver;
    CACHE_STATS _predictOverNormal;
    CACHE_STATS _predictOverLearn;
    CACHE_STATS _predictOverWrongFirst;

    CACHE_STATS _predictMiss;
    CACHE_STATS _predictMissNormal;
    CACHE_STATS _predictMissLearn;
    CACHE_STATS _predictMissWrongFirst;

  protected:
    UINT32 NumSets() const { return _indexMask + 1; }

  public:
    // constructors/destructors
    CACHE_BASE();

    // allocator
    void Init(std::string name, CACHE_TYPE cacheType, UINT32 cacheLevel, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, STORE_ALLOCATION writeAllocate);

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
    OPERATION Find(UINT32 index, ADDRINT tag,  uint64_t clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc);
    VOID Replace(UINT32 index, ADDRINT tag, uint64_t clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc, ADDRINT addr);
    VOID FlushCache(){
        for (UINT32 index=0 ; index< this->_indexMask+1; index++){
            for (UINT32 way=0; way < this->GetAssociativity()  ; way++){
                this->SaveUsageVectorFlush( this->_sets[index]._ways[way] );
            }
        }
    }

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
    CACHE_STATS BringWrongAverageTime() {
        if (this->_bringWrongNumber == 0)
            return 0;
        else
            return this->_bringWrongTime / this->_bringWrongNumber;
        }

    CACHE_STATS UsedBlocks(CACHE_STATS subblocks, ACCESS_TYPE accesstype){
        #ifdef DEBUG_MODE
            if (subblocks > this->_lineSize/SUB_BLOCK || accesstype >= ACCESS_TYPE_NUM ){ fprintf(stderr,"UsedBlocks()");  ASSERTX(false); }
        #endif
        return this->_usedBlocks[subblocks][accesstype];
        }

    CACHE_STATS RequestedBlocks(CACHE_STATS subblocks){
        #ifdef DEBUG_MODE
            if (subblocks > this->_lineSize/SUB_BLOCK){ fprintf(stderr,"RequestedBlocks()");  ASSERTX(false); }
        #endif
        return this->_requestedBlocks[subblocks];
        }

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

    //==================================================================
    VOID SaveStatistics(UINT32 Index, UINT32 Way){
        // ~ this->_predictOver += this->_sets[Index]._ways[Way].CountOverPredict(this->_lineSize);
    }


    //==================================================================
    VOID SaveUsageVector(CACHE_TAG &evicted_tag){
        evicted_tag._bringAccess = this->_clock;
        //printf("bring -> %llu \n",evicted_tag._bringAccess);

        if (evicted_tag._tag == 0){
            this->_coldStart++;
        }
        else {
            this->_evictedLines++;
        }
    }

    //==================================================================
    VOID SaveUsageVectorFlush(CACHE_TAG &flushed_tag){
        if (flushed_tag._tag == 0){
            this->_unusedLines++;
        }
        else {
            this->_flushedLines++;
        }
    }
};



/* ========================================================================================================================================== */
/*            METHODS                                                    */
/* ========================================================================================================================================== */
CACHE_BASE::CACHE_BASE(){}

/* ===================================================================== */

void CACHE_BASE::Init(std::string name, CACHE_TYPE cacheType, UINT32 cacheLevel, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, STORE_ALLOCATION writeAllocate)
{
    CACHE_STATS i,j;

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

    ASSERTX(this->GetAssociativity() <= MAX_ASSOCIATIVITY);
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
    this->_bringWrongTime=0;
    this->_bringWrongNumber=0;

    this->_requestedBlocks = new CACHE_STATS[(this->_lineSize/SUB_BLOCK)+1];
    this->_usedBlocks = new CACHE_STATS*[(this->_lineSize/SUB_BLOCK)+1];

    for (i = 0; i<=this->_lineSize/SUB_BLOCK ; i++){
        this->_requestedBlocks[i] = 0;
        this->_usedBlocks[i] = new CACHE_STATS[ACCESS_TYPE_NUM];
        for (j = 0; j < ACCESS_TYPE_NUM ; j++){
            this->_usedBlocks[i][j] = 0;
        }
    }
}

/* ===================================================================== */
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
OPERATION CACHE_BASE::Find(UINT32 index, ADDRINT tag,  uint64_t clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc)
{
    INT32 way=-1;

    UINT32 ActiveBits = 0;
    OPERATION ret_op = OPERATION_MISS;

    #ifdef DEBUG_MODE
        if (offset+size > this->_lineSize){ fprintf(stderr,"Find()");  ASSERTX(false); }
        if (offset+size > MAX_LINE_SIZE){ fprintf(stderr,"Find()");  ASSERTX(false); }
    #endif

    way = this->_sets[index].Find_Tag(this->GetAssociativity(), tag);
    // If Hits and Valid Bits enable
    if (way != -1 && this->_sets[index]._ways[way].CheckValidBitVector(offset, size) == OPERATION_HIT) {
        ActiveBits = this->_sets[index]._ways[way].CountValidBitVector(this->_lineSize);
        this->_usedBlocks[ActiveBits][accessType]++;

        this->_sets[index]._ways[way].SetLastAccess(clock);
        this->_sets[index]._ways[way].SetUsageVector(offset, size);
        ret_op = OPERATION_HIT;
    }
    else if (way != -1){ // Just wrong first
        this->_bringWrongNumber++;
        this->_bringWrongTime += this->_clock - this->_sets[index]._ways[way]._bringAccess ;
        //printf("bring_number -> %llu    bring_time -> %llu\n",_bringWrongNumber, _bringWrongTime);
    }

    return ret_op;
}



/* ===================================================================== */
VOID CACHE_BASE::Replace(UINT32 index, ADDRINT tag, uint64_t clock, UINT32 offset, UINT32 size, ACCESS_TYPE accessType, ADDRINT pc, ADDRINT addr)
{
    #ifdef DEBUG_MODE
        if (offset+size > this->_lineSize || offset+size > MAX_LINE_SIZE){ fprintf(stderr,"Replace() - offset+size >= this->_lineSize");  ASSERTX(false); }
    #endif

    INT32 way=-1;

    UINT32 ActiveBitsBefore=0, ActiveBitsAfter=0;

    way = this->_sets[index].Find_LRU(this->GetAssociativity());
    SaveStatistics(index, way);

    this->SaveUsageVector( this->_sets[index]._ways[way] );
    this->_sets[index]._ways[way].SetTag(tag);
    this->_sets[index]._ways[way].SetLastAccess(clock);
    this->_sets[index]._ways[way].CleanValidBitVector(this->_lineSize);
    this->_sets[index]._ways[way].SetValidBitVectorNormal(0, this->_lineSize);
    this->_sets[index]._ways[way].CleanUsageVector(this->_lineSize);
    this->_sets[index]._ways[way].SetUsageVector(offset, size);


    ActiveBitsAfter = this->_sets[index]._ways[way].CountValidBitVector(this->_lineSize);
    this->_usedBlocks[ActiveBitsAfter][accessType]++;
    this->_requestedBlocks[ActiveBitsAfter-ActiveBitsBefore]++;

}

#endif // PIN_CACHE_H

#define __STDC_LIMIT_MACROS

// #define DEBUG_MODE

// Pin Library
#include "pin.H"
#include "instlib.H"
using namespace INSTLIB;

//Traditional Library
#include <stdlib.h>
#include <iostream>
#include <fstream>

//Rest of PinTool
#include "cache.H"
#include "cache_parameters.H"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,            "pintool",  "o",    "SimResults",       "specify dcache file name");
KNOB<BOOL>   KnobTrackLoads(KNOB_MODE_WRITEONCE,            "pintool",  "tl",   "1",                "track individual loads");
KNOB<BOOL>   KnobTrackStores(KNOB_MODE_WRITEONCE,           "pintool",  "ts",   "1",                "track individual stores");
KNOB<BOOL>   KnobTrackInstructions(KNOB_MODE_WRITEONCE,     "pintool",  "ti",   "0",                "track individual instructions -- increases profiling time");


INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

FILE * out;
CACHE_STATS CountAccess[TOTAL_THREADS][3];
CACHE_STATS CountAllAccess[TOTAL_THREADS];
PIN_LOCK lock;



/* ===================================================================== */
VOID LoadMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_LOAD]++;
    for (UINT32 i=0; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
        Hierarchies[i][0].dl1[threadid].Access(addr, size, ACCESS_TYPE_LOAD, instAddr);
    }
    PIN_ReleaseLock(&lock);
}


/* ===================================================================== */
VOID StoreMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_STORE]++;
    for (UINT32 i=0; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
        Hierarchies[i][0].dl1[threadid].Access(addr, size, ACCESS_TYPE_STORE, instAddr);
    }
    PIN_ReleaseLock(&lock);
}


/* ===================================================================== */
VOID LoadInstructionMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_INSTRUCTION]++;
    for (UINT32 i=0; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
        Hierarchies[i][0].il1[threadid].Access(addr, size, ACCESS_TYPE_INSTRUCTION, instAddr);
    }
    PIN_ReleaseLock(&lock);
}

VOID Instruction(INS ins, void * v)
{
    // Track the Instructions and send to the Instruction Cache
    if( KnobTrackInstructions )
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LoadInstructionMulti,
            IARG_INST_PTR,
            IARG_UINT32,
            INS_Size(ins),
            IARG_INST_PTR,
            IARG_THREAD_ID,
            IARG_END);
    }


    // Track the Loads and Stores and send to the Data Cache
    if (INS_IsMemoryRead(ins) && KnobTrackLoads)
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE,  (AFUNPTR) LoadMulti,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_INST_PTR,
            IARG_THREAD_ID,
            IARG_END);
    }

    if ( INS_IsMemoryWrite(ins) && KnobTrackStores)
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE,  (AFUNPTR) StoreMulti,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_INST_PTR,
            IARG_THREAD_ID,
            IARG_END);
    }
}


/* ===================================================================== */
VOID Fini(int code, VOID * v)
{
    CACHE_STATS TID=0; // Thread Id Iterator
    CACHE_STATS HIE=0; // Hierarchy Iterator
    CACHE_STATS LVL=0; // Cache Level Iterator

    ////////////////////////////////////////////////////////////////////
    CACHE_STATS Max_Threads = TOTAL_THREADS;
    UINT32 DLineSize=0, ILineSize=0;
    UINT32 DCacheSize=0, ICacheSize=0;

    char *stats_file_name = new char [KnobOutputFile.Value().size()+65];
    char buffer [50];

    for (HIE=0 ; HIE<TOTAL_CACHES_HIERARCHIES ; HIE++ ){

        buffer[0] = '\0';
        DLineSize = Hierarchies[HIE][0].dl1[0].GetLineSize();
        DCacheSize = Hierarchies[HIE][0].dl1[0].GetCacheSize() / KILO;
        strcpy (stats_file_name, KnobOutputFile.Value().c_str());
        sprintf (buffer, ".%dKB%dB%dSubB%dL.out", DCacheSize,DLineSize,SUB_BLOCK,Levels[HIE]);

        strcat(stats_file_name, buffer);
        out = fopen(stats_file_name, "w");

        for (LVL=0 ; LVL<Levels[HIE]; LVL++){

            if (LVL<3) Max_Threads = TOTAL_THREADS;
            else Max_Threads = 1;

            #ifdef DEBUG_MODE
                fprintf(stderr,"Statistics Output Loop Hierarchy:%llu Level:%llu\n",HIE+1,LVL+1);
            #endif
            //=====================================================================
            fprintf(out,"#CACHE LEVEL %llu\n",LVL+1);
            //=====================================================================

            if( KnobTrackLoads ||  KnobTrackStores){
                DLineSize = Hierarchies[HIE][LVL].dl1[0].GetLineSize();
                DCacheSize = Hierarchies[HIE][LVL].dl1[0].GetCacheSize() / KILO;
                //=====================================================================
                fprintf(out,"#DATA CACHE - DESCRIPTION\n");
                //=====================================================================
                fprintf(out,"#L%llu_DATA_CACHE;SIZE;LINE_SIZE;ASSOCIATIVITY;WRITE_ALLOCATE;",LVL+1);
                fprintf(out,"\n");
                fprintf(out,"%d;%d;%d;%d;%d;",0, Hierarchies[HIE][LVL].dl1[0].GetCacheSize(), Hierarchies[HIE][LVL].dl1[0].GetLineSize(), Hierarchies[HIE][LVL].dl1[0].GetAssociativity(), Hierarchies[HIE][LVL].dl1[0].GetWriteAllocate());
                fprintf(out,"\n");
                fprintf(out,"\n");
            }
            if( KnobTrackInstructions && LVL==0 ){
                ILineSize = Hierarchies[HIE][LVL].il1[0].GetLineSize();
                ICacheSize = Hierarchies[HIE][LVL].dl1[0].GetCacheSize() / KILO;
                //=====================================================================
                fprintf(out,"#INST CACHE - DESCRIPTION\n");
                //=====================================================================
                fprintf(out,"#L%llu_INST_CACHE;SIZE;LINE_SIZE;ASSOCIATIVITY;",LVL+1);
                fprintf(out,"\n");
                fprintf(out,"%d;%d;%d;%d;",0, Hierarchies[HIE][LVL].il1[0].GetCacheSize(), Hierarchies[HIE][LVL].il1[0].GetLineSize(), Hierarchies[HIE][LVL].il1[0].GetAssociativity());
                fprintf(out,"\n");
                fprintf(out,"\n");
            }



        //=====================================================================
        fprintf(out,"#DATA + INST CACHE - ACCESS CALL COUNTER\n");
        //=====================================================================
            fprintf(out,"#L%llu_DATA_CACHE;INSTRUCTIONS;LOAD;STORE;",LVL+1);
            fprintf(out,"\n");
            for (TID=0; TID<Max_Threads; TID++){
                fprintf(out,"%llu;%llu;%llu;%llu;",TID, CountAccess[TID][ACCESS_TYPE_INSTRUCTION],CountAccess[TID][ACCESS_TYPE_LOAD],CountAccess[TID][ACCESS_TYPE_STORE]);
                fprintf(out,"\n");
            }
            fprintf(out,"\n");


            if( KnobTrackLoads ||  KnobTrackStores){
                //=====================================================================
                fprintf(out,"#DATA CACHE - MISS / HIT PROFILE\n");
                //=====================================================================
                fprintf(out,"#L%llu_DATA_CACHE;TOTAL_ACCESS;TOTAL_HITS;TOTAL_MISSES;",LVL+1);
                fprintf(out,"LOAD_ACCESSES;LOAD_HIT;LOAD_MISSES;");
                fprintf(out,"WRITE_ACCESES;WRITE_HIT;WRITE_MISS;");
                fprintf(out,"EVICTED_LINES;FLUSHED_LINES;UNUSED_LINES;TOTAL_COLD_START_MISSES;");
                fprintf(out,"\n");
                for (TID=0; TID<Max_Threads; TID++){
                    fprintf(out,"%llu;%llu;%llu;%llu;",TID,Hierarchies[HIE][LVL].dl1[TID].Accesses(), Hierarchies[HIE][LVL].dl1[TID].Hits(),Hierarchies[HIE][LVL].dl1[TID].Misses());
                    fprintf(out,"%llu;%llu;%llu;",Hierarchies[HIE][LVL].dl1[TID].Accesses(ACCESS_TYPE_LOAD), Hierarchies[HIE][LVL].dl1[TID].Hits(ACCESS_TYPE_LOAD),Hierarchies[HIE][LVL].dl1[TID].Misses(ACCESS_TYPE_LOAD));
                    fprintf(out,"%llu;%llu;%llu;",Hierarchies[HIE][LVL].dl1[TID].Accesses(ACCESS_TYPE_STORE),Hierarchies[HIE][LVL].dl1[TID].Hits(ACCESS_TYPE_STORE),Hierarchies[HIE][LVL].dl1[TID].Misses(ACCESS_TYPE_STORE));
                    fprintf(out,"%llu;%llu;%llu;%llu;",Hierarchies[HIE][LVL].dl1[TID].EvictedLines(),Hierarchies[HIE][LVL].dl1[TID].FlushedLines(),Hierarchies[HIE][LVL].dl1[TID].UnusedLines(), Hierarchies[HIE][LVL].dl1[TID].ColdStart());
                    fprintf(out,"\n");
                }
                fprintf(out,"\n");

            }
            if( KnobTrackInstructions && LVL==0){
                //=====================================================================
                fprintf(out,"#INST CACHE - MISS / HIT PROFILE\n");
                //=====================================================================
                fprintf(out,"#L%llu_INST_CACHE;TOTAL_ACCESS;TOTAL_HITS;TOTAL_MISSES;",LVL+1);
                fprintf(out,"INSTRUCTION_ACCESSES;INSTRUCTION_HIT;INSTRUCTION_MISSES;");
                fprintf(out,"EVICTED_LINES;FLUSHED_LINES;UNUSED_LINES;TOTAL_COLD_START_MISSES;");
                fprintf(out,"\n");
                for (TID=0; TID<Max_Threads; TID++){
                    fprintf(out,"%llu;%llu;%llu;%llu;",TID,Hierarchies[HIE][LVL].il1[TID].Accesses(), Hierarchies[HIE][LVL].il1[TID].Hits(),Hierarchies[HIE][LVL].il1[TID].Misses());
                    fprintf(out,"%llu;%llu;%llu;",Hierarchies[HIE][LVL].il1[TID].Accesses(ACCESS_TYPE_INSTRUCTION), Hierarchies[HIE][LVL].il1[TID].Hits(ACCESS_TYPE_INSTRUCTION),Hierarchies[HIE][LVL].il1[TID].Misses(ACCESS_TYPE_INSTRUCTION));
                    fprintf(out,"%llu;%llu;%llu;%llu;",Hierarchies[HIE][LVL].il1[TID].EvictedLines(),Hierarchies[HIE][LVL].il1[TID].FlushedLines(),Hierarchies[HIE][LVL].il1[TID].UnusedLines(), Hierarchies[HIE][LVL].il1[TID].ColdStart());
                    fprintf(out,"\n");
                }
                fprintf(out,"\n");
            }

        }
        fclose(out);
    }
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) ) {
        return Usage();
    }

    for (UINT32 i=0; i<TOTAL_THREADS; i++){
        CountAllAccess[i] = 0;
        for (UINT32 j=0; j<3; j++){
            CountAccess[i][j] = 0;
        }
    }

    InitCache();

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}

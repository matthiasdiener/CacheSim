#include <cstdlib>
#include <iostream>
#include <cstring>

#include "pin.H"

#include "cache.H"
#include "cache_parameters.H"


KNOB<BOOL>   KnobTrackLoads(KNOB_MODE_WRITEONCE,            "pintool",  "tl",   "1",                "track individual loads");
KNOB<BOOL>   KnobTrackStores(KNOB_MODE_WRITEONCE,           "pintool",  "ts",   "1",                "track individual stores");
KNOB<BOOL>   KnobTrackInstructions(KNOB_MODE_WRITEONCE,     "pintool",  "ti",   "0",                "track individual instructions -- increases profiling time");


INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

int num_threads = 0;

CACHE_STATS CountAccess[MAX_THREADS][3];
CACHE_STATS CountAllAccess[MAX_THREADS];
PIN_LOCK lock;


VOID LoadMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_LOAD]++;
    Hierarchies[0].dl1[threadid].Access(addr, size, ACCESS_TYPE_LOAD, instAddr);

    PIN_ReleaseLock(&lock);
}


VOID StoreMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_STORE]++;
    Hierarchies[0].dl1[threadid].Access(addr, size, ACCESS_TYPE_STORE, instAddr);

    PIN_ReleaseLock(&lock);
}


VOID LoadInstructionMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_INSTRUCTION]++;
    Hierarchies[0].il1[threadid].Access(addr, size, ACCESS_TYPE_INSTRUCTION, instAddr);

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

string img_name;

VOID binName(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
        img_name = basename(IMG_Name(img).c_str());
}


VOID Fini(int code, VOID * v)
{
    CACHE_STATS TID=0; // Thread Id Iterator
    CACHE_STATS LVL=0; // Cache Level Iterator

    CACHE_STATS Max_Threads = num_threads;

    string filename = img_name+"."+to_string(Hierarchies[0].dl1[0].GetCacheSize()/KILO)+"KB"+to_string(Hierarchies[0].dl1[0].GetLineSize())+"B" + to_string(Levels) + "L.out";

    FILE *out = fopen(filename.c_str(), "w");

    for (LVL=0 ; LVL<Levels; LVL++){

        if (LVL<Levels-1) Max_Threads = num_threads;
        else Max_Threads = 1;

        //=====================================================================
        fprintf(out,"#CACHE LEVEL %llu\n",LVL+1);
        //=====================================================================

        if( KnobTrackLoads ||  KnobTrackStores){
            //=====================================================================
            fprintf(out,"#DATA CACHE - DESCRIPTION\n");
            //=====================================================================
            fprintf(out,"#L%llu_DATA_CACHE;SIZE;LINE_SIZE;ASSOCIATIVITY;WRITE_ALLOCATE;",LVL+1);
            fprintf(out,"\n");
            fprintf(out,"%d;%d;%d;%d;%d;",0, Hierarchies[LVL].dl1[0].GetCacheSize(), Hierarchies[LVL].dl1[0].GetLineSize(), Hierarchies[LVL].dl1[0].GetAssociativity(), Hierarchies[LVL].dl1[0].GetWriteAllocate());
            fprintf(out,"\n");
            fprintf(out,"\n");
        }
        if( KnobTrackInstructions && LVL==0 ){
            //=====================================================================
            fprintf(out,"#INST CACHE - DESCRIPTION\n");
            //=====================================================================
            fprintf(out,"#L%llu_INST_CACHE;SIZE;LINE_SIZE;ASSOCIATIVITY;",LVL+1);
            fprintf(out,"\n");
            fprintf(out,"%d;%d;%d;%d;",0, Hierarchies[LVL].il1[0].GetCacheSize(), Hierarchies[LVL].il1[0].GetLineSize(), Hierarchies[LVL].il1[0].GetAssociativity());
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
                fprintf(out,"%llu;%llu;%llu;%llu;",TID,Hierarchies[LVL].dl1[TID].Accesses(), Hierarchies[LVL].dl1[TID].Hits(),Hierarchies[LVL].dl1[TID].Misses());
                fprintf(out,"%llu;%llu;%llu;",Hierarchies[LVL].dl1[TID].Accesses(ACCESS_TYPE_LOAD), Hierarchies[LVL].dl1[TID].Hits(ACCESS_TYPE_LOAD),Hierarchies[LVL].dl1[TID].Misses(ACCESS_TYPE_LOAD));
                fprintf(out,"%llu;%llu;%llu;",Hierarchies[LVL].dl1[TID].Accesses(ACCESS_TYPE_STORE),Hierarchies[LVL].dl1[TID].Hits(ACCESS_TYPE_STORE),Hierarchies[LVL].dl1[TID].Misses(ACCESS_TYPE_STORE));
                fprintf(out,"%llu;%llu;%llu;%llu;",Hierarchies[LVL].dl1[TID].EvictedLines(),Hierarchies[LVL].dl1[TID].FlushedLines(),Hierarchies[LVL].dl1[TID].UnusedLines(), Hierarchies[LVL].dl1[TID].ColdStart());
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
                fprintf(out,"%llu;%llu;%llu;%llu;",TID,Hierarchies[LVL].il1[TID].Accesses(), Hierarchies[LVL].il1[TID].Hits(),Hierarchies[LVL].il1[TID].Misses());
                fprintf(out,"%llu;%llu;%llu;",Hierarchies[LVL].il1[TID].Accesses(ACCESS_TYPE_INSTRUCTION), Hierarchies[LVL].il1[TID].Hits(ACCESS_TYPE_INSTRUCTION),Hierarchies[LVL].il1[TID].Misses(ACCESS_TYPE_INSTRUCTION));
                fprintf(out,"%llu;%llu;%llu;%llu;",Hierarchies[LVL].il1[TID].EvictedLines(),Hierarchies[LVL].il1[TID].FlushedLines(),Hierarchies[LVL].il1[TID].UnusedLines(), Hierarchies[LVL].il1[TID].ColdStart());
                fprintf(out,"\n");
            }
            fprintf(out,"\n");
        }

    }
    fclose(out);
    cout << "Wrote " << filename << endl;
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    __sync_add_and_fetch(&num_threads, 1);
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    PIN_InitLock(&lock);

    if( PIN_Init(argc,argv) ) {
        return Usage();
    }

    for (UINT32 i=0; i<MAX_THREADS; i++){
        CountAllAccess[i] = 0;
        for (UINT32 j=0; j<3; j++){
            CountAccess[i][j] = 0;
        }
    }

    InitCache();

    PIN_AddThreadStartFunction(ThreadStart, 0);
    IMG_AddInstrumentFunction(binName, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}

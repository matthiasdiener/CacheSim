#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <map>

#include "pin.H"


#define MAX_THREADS 256  // maximum threads to keep track of
#include "cache.H"
#include "cache_parameters.H"

extern UINT64 spatial_comm;
extern UINT64 true_comm;
extern UINT64 no_comm;

KNOB<BOOL>   KnobTrackLoads(KNOB_MODE_WRITEONCE,            "pintool",  "tl",   "1",                "track individual loads");
KNOB<BOOL>   KnobTrackStores(KNOB_MODE_WRITEONCE,           "pintool",  "ts",   "1",                "track individual stores");
KNOB<BOOL>   KnobTrackInstructions(KNOB_MODE_WRITEONCE,     "pintool",  "ti",   "0",                "track individual instructions -- increases profiling time");

KNOB<BOOL>   KnobInf(KNOB_MODE_WRITEONCE,     "pintool",  "inf",   "0",                "infinite last level cache");

UINT64 comm_matrix[MAX_THREADS][MAX_THREADS]; // comm matrix


INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

int num_threads = 0;

CACHE_STATS CountAccess[MAX_THREADS][3];
PIN_LOCK lock;
map<UINT32, UINT32> pidmap; // pid -> tid


VOID LoadMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    if (num_threads<2)
        return;
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_LOAD]++;
    Hierarchies[0].dl1[threadid].Access(addr, size, ACCESS_TYPE_LOAD, instAddr, threadid);

    PIN_ReleaseLock(&lock);
}


VOID StoreMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    if (num_threads<2)
        return;
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_STORE]++;
    Hierarchies[0].dl1[threadid].Access(addr, size, ACCESS_TYPE_STORE, instAddr, threadid);

    PIN_ReleaseLock(&lock);
}


VOID LoadInstructionMulti(ADDRINT addr, UINT32 size, ADDRINT instAddr,THREADID threadid)
{
    PIN_GetLock(&lock, threadid+1);

    CountAccess[threadid][ACCESS_TYPE_INSTRUCTION]++;
    Hierarchies[0].il1[threadid].Access(addr, size, ACCESS_TYPE_INSTRUCTION, instAddr, threadid);

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
    int real_tid[MAX_THREADS+1];
    int i = 0, a, b;

    for (auto it : pidmap) {
        real_tid[it.second] = i++;
    }

    for (int i = num_threads-1; i>=0; i--) {
        a = real_tid[i];
        for (int j = 0; j<num_threads; j++) {
            b = real_tid[j];
            cout << comm_matrix[a][b] + comm_matrix[b][a];
            if (j != num_threads-1)
                cout << ",";
        }
        cout << endl;
    }
    cout << endl;

    cout << "True:\t" << true_comm << endl << "Spat.:\t" << spatial_comm << endl << "Non:\t" << no_comm << endl << "Ratio:\t" << (double) (true_comm+spatial_comm)*100/(no_comm+true_comm+spatial_comm) << "%" << endl <<endl;
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    __sync_add_and_fetch(&num_threads, 1);
    int pid = PIN_GetTid();
    pidmap[pid] = tid;
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    PIN_InitLock(&lock);

    if( PIN_Init(argc,argv) ) {
        return Usage();
    }

    cout << "CacheSim" << endl;

    for (UINT32 i=0; i<MAX_THREADS; i++){
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

/* ===================================================================== */
// CLASSES
/* ===================================================================== */
class CACHE_PARAMS
{
    public:
        UINT32 _cacheLevel;
        UINT32 _cacheSize;//Size in kilobytes
        UINT32 _lineSize;//Block size in bytes
        UINT32 _associativity;//Cache Associativity
        UINT8  _writeAllocate;//Write Allocate (0/1)

    CACHE_PARAMS(){
        _cacheLevel = 0;
        _cacheSize = 32;
        _lineSize = 64;
        _associativity = 8;
        _writeAllocate = STORE_ALLOCATE;
    }
};

/* ===================================================================== */
class CACHE_LEVEL
{
    public:
        CACHE_PARAMS params_dl1;
        CACHE_BASE *dl1;

        CACHE_PARAMS params_il1;
        CACHE_BASE *il1;

    CACHE_LEVEL(){
        dl1 = NULL;
        il1 = NULL;
    }
};
/* ===================================================================== */

UINT32 Levels;
UINT32 *Profiles;
CACHE_LEVEL *Hierarchies;

#define TOTAL_THREADS 4  // Total of threads to keep track of
#define TOTAL_CACHES_HIERARCHIES 1// Total of models to simulate

/* ===================================================================== */
static VOID InitCache()
{
    UINT32 i=0,j=0;

    Levels = 3;

    Hierarchies = new CACHE_LEVEL[3];


    //===== LEVEL 1
    Hierarchies[0].params_dl1._cacheSize = 32;
    Hierarchies[0].params_dl1._lineSize = 64;
    Hierarchies[0].params_dl1._associativity = 8;
    Hierarchies[1].params_dl1._writeAllocate = STORE_ALLOCATE;
    //==
    Hierarchies[0].params_il1._cacheSize = 32;
    Hierarchies[0].params_il1._lineSize = 64;
    Hierarchies[0].params_il1._associativity = 8;
    Hierarchies[1].params_il1._writeAllocate = STORE_ALLOCATE;
    //==== LEVE
    Hierarchies[1].params_dl1._cacheSize = 256;
    Hierarchies[1].params_dl1._lineSize = 64;
    Hierarchies[1].params_dl1._associativity = 16;
    Hierarchies[1].params_dl1._writeAllocate = STORE_ALLOCATE;
    //==== LEVE
    Hierarchies[2].params_dl1._cacheSize = 1024;
    Hierarchies[2].params_dl1._lineSize = 64;
    Hierarchies[2].params_dl1._associativity = 32;
    Hierarchies[2].params_dl1._writeAllocate = STORE_ALLOCATE;


    #ifdef DEBUG_MODE
        fprintf(stderr,"Set 1 Allocating Caches %d\n",i);
    #endif


    // SET CACHE L1 ================================================
    //==============================================================
    //Allocate the 1 Level Cache - DATA
    Hierarchies[0].dl1 = new CACHE_BASE[TOTAL_THREADS];
    //Allocate the 1 Level Cache - INSTRUCTION
    Hierarchies[0].il1 = new CACHE_BASE[TOTAL_THREADS];
    for (j=0; j<TOTAL_THREADS;j++){
        Hierarchies[0].dl1[j].Init("Data Cache",         CACHE_TYPE_DCACHE, 1, Hierarchies[0].params_dl1._cacheSize * KILO,
                                                                    Hierarchies[0].params_dl1._lineSize,
                                                                    Hierarchies[0].params_dl1._associativity,
                                                 (STORE_ALLOCATION) Hierarchies[0].params_dl1._writeAllocate);

        Hierarchies[0].il1[j].Init("Instruction Cache",  CACHE_TYPE_ICACHE, 1,  Hierarchies[0].params_il1._cacheSize * KILO,
                                                                    Hierarchies[0].params_il1._lineSize,
                                                                    Hierarchies[0].params_il1._associativity,
                                                 (STORE_ALLOCATION) Hierarchies[0].params_il1._writeAllocate);
    }

    // SET CACHE L2 ================================================
    //==============================================================
    if (Levels >= 2){
        Hierarchies[1].dl1 = new CACHE_BASE[TOTAL_THREADS];
        for (j=0; j<TOTAL_THREADS;j++){
            Hierarchies[1].dl1[j].Init("Data Cache", CACHE_TYPE_DCACHE, 2, Hierarchies[1].params_dl1._cacheSize * KILO,
                                                                        Hierarchies[1].params_dl1._lineSize,
                                                                        Hierarchies[1].params_dl1._associativity,
                                                     (STORE_ALLOCATION) Hierarchies[1].params_dl1._writeAllocate);
        }
    }

    // SET CACHE L3 ================================================
    //==============================================================
    if (Levels >= 3){
        Hierarchies[2].dl1 = new CACHE_BASE[1];
        Hierarchies[2].dl1[0].Init("Data Cache", CACHE_TYPE_DCACHE, 3, Hierarchies[2].params_dl1._cacheSize * KILO,
                                                                    Hierarchies[2].params_dl1._lineSize,
                                                                    Hierarchies[2].params_dl1._associativity,
                                                 (STORE_ALLOCATION) Hierarchies[2].params_dl1._writeAllocate);
    }

    for (i=0 ; i<TOTAL_CACHES_HIERARCHIES ; i++ ){\
        // CONNECT CACHE L1 => L2 ======================================
        //==============================================================
        if (Levels > 1){
            for (j=0; j<TOTAL_THREADS;j++){
                Hierarchies[0].dl1[j].SetNextCacheLevel( &Hierarchies[1].dl1[j] );
                Hierarchies[0].il1[j].SetNextCacheLevel( &Hierarchies[1].dl1[j] );
            }
        }

        // CONNECT CACHE L2 => L3 ======================================
        //==============================================================
        if (Levels > 2){
            for (j=0; j<TOTAL_THREADS;j++){
                Hierarchies[1].dl1[j].SetNextCacheLevel( &Hierarchies[2].dl1[0] );
            }
        }
    }

    #ifdef DEBUG_MODE
        fprintf(stderr,"Set 3 Checking Caches Connection\n");
        for (i=0 ; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
            for (j=0 ; j<Levels; j++){
                if (Hierarchies[j].dl1[0].GetNextCacheLevel() != NULL){
                    Hierarchies[j].dl1[0].GetNextCacheLevel()->PrintName();
                }
            }
        }

        fprintf(stderr,"End Initialization\n");
    #endif
}

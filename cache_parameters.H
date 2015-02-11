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

UINT32 *Levels;
UINT32 *Profiles;
CACHE_LEVEL **Hierarchies;

#define TOTAL_THREADS 4  // Total of threads to keep track of
#define TOTAL_CACHES_HIERARCHIES 1// Total of models to simulate

/* ===================================================================== */
static VOID InitCache()
{
    #ifdef DEBUG_MODE
        fprintf(stderr,"Initializing: %d Threads and %d Hierarchies \n",TOTAL_THREADS,TOTAL_CACHES_HIERARCHIES);
    #endif
    UINT32 i=0,j=0;
    Levels = new UINT32[TOTAL_CACHES_HIERARCHIES];
    for (i=0;i<TOTAL_CACHES_HIERARCHIES; i++){
        Levels[i] = 3;
    }

    Hierarchies = new CACHE_LEVEL*[TOTAL_CACHES_HIERARCHIES];

    for (i=0; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
        Hierarchies[i] = new CACHE_LEVEL[Levels[i]];
    }


    //============================PROFILE LEVELS
    //======================================================================
    //=========== HIERARCHY N. 0
    //===== LEVEL 1
    Hierarchies[0][0].params_dl1._cacheSize = 32;
    Hierarchies[0][0].params_dl1._lineSize = 64;
    Hierarchies[0][0].params_dl1._associativity = 8;
    Hierarchies[0][1].params_dl1._writeAllocate = STORE_ALLOCATE;
    //==
    Hierarchies[0][0].params_il1._cacheSize = 32;
    Hierarchies[0][0].params_il1._lineSize = 64;
    Hierarchies[0][0].params_il1._associativity = 8;
    Hierarchies[0][1].params_il1._writeAllocate = STORE_ALLOCATE;
    //==== LEVEL 2
    Hierarchies[0][1].params_dl1._cacheSize = 256;
    Hierarchies[0][1].params_dl1._lineSize = 64;
    Hierarchies[0][1].params_dl1._associativity = 16;
    Hierarchies[0][1].params_dl1._writeAllocate = STORE_ALLOCATE;
    //==== LEVEL 3
    Hierarchies[0][2].params_dl1._cacheSize = 1024;
    Hierarchies[0][2].params_dl1._lineSize = 64;
    Hierarchies[0][2].params_dl1._associativity = 32;
    Hierarchies[0][2].params_dl1._writeAllocate = STORE_ALLOCATE;



    for (i=0; i<TOTAL_CACHES_HIERARCHIES ; i++ ){

        #ifdef DEBUG_MODE
            fprintf(stderr,"Set 1 Allocating Caches %d\n",i);
        #endif


        // SET CACHE L1 ================================================
        //==============================================================
        //Allocate the 1 Level Cache - DATA
        Hierarchies[i][0].dl1 = new CACHE_BASE[TOTAL_THREADS];
        //Allocate the 1 Level Cache - INSTRUCTION
        Hierarchies[i][0].il1 = new CACHE_BASE[TOTAL_THREADS];
        for (j=0; j<TOTAL_THREADS;j++){
            Hierarchies[i][0].dl1[j].Init("Data Cache",         CACHE_TYPE_DCACHE, 1, Hierarchies[i][0].params_dl1._cacheSize * KILO,
                                                                        Hierarchies[i][0].params_dl1._lineSize,
                                                                        Hierarchies[i][0].params_dl1._associativity,
                                                     (STORE_ALLOCATION) Hierarchies[i][0].params_dl1._writeAllocate);

            Hierarchies[i][0].il1[j].Init("Instruction Cache",  CACHE_TYPE_ICACHE, 1,  Hierarchies[i][0].params_il1._cacheSize * KILO,
                                                                        Hierarchies[i][0].params_il1._lineSize,
                                                                        Hierarchies[i][0].params_il1._associativity,
                                                     (STORE_ALLOCATION) Hierarchies[i][0].params_il1._writeAllocate);
        }

        // SET CACHE L2 ================================================
        //==============================================================
        if (Levels[i] >= 2){
            Hierarchies[i][1].dl1 = new CACHE_BASE[TOTAL_THREADS];
            for (j=0; j<TOTAL_THREADS;j++){
                Hierarchies[i][1].dl1[j].Init("Data Cache", CACHE_TYPE_DCACHE, 2, Hierarchies[i][1].params_dl1._cacheSize * KILO,
                                                                            Hierarchies[i][1].params_dl1._lineSize,
                                                                            Hierarchies[i][1].params_dl1._associativity,
                                                         (STORE_ALLOCATION) Hierarchies[i][1].params_dl1._writeAllocate);
            }
        }

        // SET CACHE L3 ================================================
        //==============================================================
        if (Levels[i] >= 3){
            Hierarchies[i][2].dl1 = new CACHE_BASE[1];
            Hierarchies[i][2].dl1[0].Init("Data Cache", CACHE_TYPE_DCACHE, 3, Hierarchies[i][2].params_dl1._cacheSize * KILO,
                                                                        Hierarchies[i][2].params_dl1._lineSize,
                                                                        Hierarchies[i][2].params_dl1._associativity,
                                                     (STORE_ALLOCATION) Hierarchies[i][2].params_dl1._writeAllocate);
        }
    }
    #ifdef DEBUG_MODE
        fprintf(stderr,"Set 2 Connecting Caches\n");
    #endif
    for (i=0 ; i<TOTAL_CACHES_HIERARCHIES ; i++ ){\
        // CONNECT CACHE L1 => L2 ======================================
        //==============================================================
        if (Levels[i] > 1){
            for (j=0; j<TOTAL_THREADS;j++){
                Hierarchies[i][0].dl1[j].SetNextCacheLevel( &Hierarchies[i][1].dl1[j] );
                Hierarchies[i][0].il1[j].SetNextCacheLevel( &Hierarchies[i][1].dl1[j] );
            }
        }

        // CONNECT CACHE L2 => L3 ======================================
        //==============================================================
        if (Levels[i] > 2){
            for (j=0; j<TOTAL_THREADS;j++){
                Hierarchies[i][1].dl1[j].SetNextCacheLevel( &Hierarchies[i][2].dl1[0] );
            }
        }
    }

    #ifdef DEBUG_MODE
        fprintf(stderr,"Set 3 Checking Caches Connection\n");
        for (i=0 ; i<TOTAL_CACHES_HIERARCHIES ; i++ ){
            for (j=0 ; j<Levels[i]; j++){
                if (Hierarchies[i][j].dl1[0].GetNextCacheLevel() != NULL){
                    Hierarchies[i][j].dl1[0].GetNextCacheLevel()->PrintName();
                }
            }
        }

        fprintf(stderr,"End Initialization\n");
    #endif
}

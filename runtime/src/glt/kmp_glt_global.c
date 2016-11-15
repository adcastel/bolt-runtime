/*
 * kmp_abt_global.c -- KPTS global variables for runtime support library
 */


//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//


#include "kmp_glt.h"

/* global shared data */
volatile int __kmp_init_global = FALSE;
kmp_global_t __kmp_global = { 0, };

int     __kmp_generate_warnings = kmp_warnings_low;

#ifdef KMP_DEBUG
int     kmp_a_debug = 0;
int     kmp_b_debug = 0;
int     kmp_c_debug = 0;
int     kmp_d_debug = 0;
int     kmp_e_debug = 0;
int     kmp_f_debug = 0;
int     kmp_diag    = 0;
#endif

/* For debug information logging using rotating buffer */
int     __kmp_debug_buf = FALSE;        /* TRUE means use buffer, FALSE means print to stderr */
int     __kmp_debug_buf_lines = KMP_DEBUG_BUF_LINES_INIT; /* Lines of debug stored in buffer */
int     __kmp_debug_buf_chars = KMP_DEBUG_BUF_CHARS_INIT; /* Characters allowed per line in buffer */
int     __kmp_debug_buf_atomic = FALSE; /* TRUE means use atomic update of buffer entry pointer */

char   *__kmp_debug_buffer = NULL;      /* Debug buffer itself */
int     __kmp_debug_count = 0;          /* Counter for number of lines printed in buffer so far */
int     __kmp_debug_buf_warn_chars = 0; /* Keep track of char increase recommended in warnings */
/* end rotating debug buffer */

#ifdef KMP_DEBUG
int     __kmp_par_range;           /* +1 => only go par for constructs in range */
                                           /* -1 => only go par for constructs outside range */
char    __kmp_par_range_routine[KMP_PAR_RANGE_ROUTINE_LEN] = { '\0' };
char    __kmp_par_range_filename[KMP_PAR_RANGE_FILENAME_LEN] = { '\0' };
int     __kmp_par_range_lb = 0;
int     __kmp_par_range_ub = INT_MAX;
#endif /* KMP_DEBUG */

/* For printing out dynamic storage map for threads and teams */
int     __kmp_storage_map = FALSE;         /* True means print storage map for threads and teams */
int     __kmp_storage_map_verbose = FALSE; /* True means storage map includes placement info */
int     __kmp_storage_map_verbose_specified = FALSE;


volatile kmp_uint32 __kmp_team_counter  = 0;
volatile kmp_uint32 __kmp_task_counter  = 0;


void
__kmp_global_initialize(void)
{
    int status;
    int i;
    /* Initialize Argobots before other initializations. */
    //status = ABT_init(0, NULL);
    //KMP_CHECK_SYSFAIL( "ABT_init", status );
    
  /*  char *env;
    char buff[10];

    int nthreads;
    int i, k;

    // Is __kmp_global.xproc a reasonable value for the number of ESs? 
    env = getenv("KMP_GLT_NUM_THREADS");
    if (env) {
        nthreads = atoi(env);
    if (nthreads < __kmp_global.xproc) __kmp_global.xproc = nthreads;
    } else {
        nthreads = __kmp_global.xproc;
    }
    
    KA_TRACE( 1000, ("__kmp_global_initialize: # of GLT_threads = %d\n", nthreads ) );
    printf("__kmp_global_initialize: # of GLT_threads = %d\n", nthreads );

    sprintf(buff, "%d", nthreads);
    setenv("GLT_NUM_THREADS", buff, 1);
    
    glt_init(0,NULL);
*/
    __kmp_global.g = { 0 };

    /* --------------------------------------------------------------------------- */
    /* map OMP 3.0 schedule types with our internal schedule types */
    static sched_type sch_map[ kmp_sched_upper - kmp_sched_lower_ext + kmp_sched_upper_std - kmp_sched_lower - 2 ] = {
        kmp_sch_static_chunked,     // ==> kmp_sched_static            = 1
        kmp_sch_dynamic_chunked,    // ==> kmp_sched_dynamic           = 2
        kmp_sch_guided_chunked,     // ==> kmp_sched_guided            = 3
        kmp_sch_auto,               // ==> kmp_sched_auto              = 4
        kmp_sch_trapezoidal         // ==> kmp_sched_trapezoidal       = 101
                                    // will likely not used, introduced here just to debug the code
                                    // of public intel extension schedules
    };
    KMP_MEMCPY(__kmp_global.sch_map, sch_map, sizeof(sch_map));

#if OMP_40_ENABLED
    __kmp_global.nested_proc_bind = { NULL, 0, 0 };
    __kmp_global.affinity_num_places = 0;
#endif

    __kmp_global.place_num_sockets = 0;
    __kmp_global.place_socket_offset = 0;
    __kmp_global.place_num_cores = 0;
    __kmp_global.place_core_offset = 0;
    __kmp_global.place_num_threads_per_core = 0;

    __kmp_global.tasking_mode = tskm_task_teams;
    __kmp_global.task_stealing_constraint = 1;   /* Constrain task stealing by default */
#if OMP_41_ENABLED
    __kmp_global.max_task_priority = 0;
#endif

    __kmp_global.settings = FALSE;
    __kmp_global.duplicate_library_ok = 0;
    __kmp_global.force_reduction_method = reduction_method_not_defined;
    __kmp_global.determ_red = FALSE;

    __kmp_global.cpuinfo = { 0 };

    /* ------------------------------------------------------------------------ */

    __kmp_global.init_serial   = FALSE;
    __kmp_global.init_gtid     = FALSE;
    __kmp_global.init_common   = FALSE;
    __kmp_global.init_middle   = FALSE;
    __kmp_global.init_parallel = FALSE;
    __kmp_global.init_runtime  = FALSE;

    __kmp_global.init_counter = 0;
    __kmp_global.root_counter = 0;
    __kmp_global.version = 0;

    /* list of address of allocated caches for commons */
    __kmp_global.threadpriv_cache_list = NULL;

    /* Global Locks */
/*
    ABT_mutex_create(&__kmp_global.stdio_lock);
    ABT_mutex_create(&__kmp_global.cat_lock);
    ABT_mutex_create(&__kmp_global.initz_lock);
    ABT_mutex_create(&__kmp_global.task_team_lock);
    for (i = 0; i < KMP_NUM_CRIT_LOCKS; i++) {
        ABT_mutex_create(&__kmp_global.crit_lock[i]);
    }
*/
    glt_mutex_create(&__kmp_global.stdio_lock);
    glt_mutex_create(&__kmp_global.cat_lock);
    glt_mutex_create(&__kmp_global.initz_lock);
    glt_mutex_create(&__kmp_global.task_team_lock);
    for (i = 0; i < KMP_NUM_CRIT_LOCKS; i++) {
        glt_mutex_create(&__kmp_global.crit_lock[i]);
    }

    __kmp_global.library = library_none;
    __kmp_global.sched = kmp_sch_default;  /* scheduling method for runtime scheduling */
    __kmp_global.sched_static = kmp_sch_static_greedy; /* default static scheduling method */
    __kmp_global.sched_guided = kmp_sch_guided_iterative_chunked; /* default guided scheduling method */
    __kmp_global.sched_auto = kmp_sch_guided_analytical_chunked; /* default auto scheduling method */
    __kmp_global.chunk = 0;

    __kmp_global.stksize    = KMP_DEFAULT_STKSIZE;
    __kmp_global.stkoffset  = KMP_DEFAULT_STKOFFSET;
    __kmp_global.stkpadding = KMP_MIN_STKPADDING;

    __kmp_global.malloc_pool_incr  = KMP_DEFAULT_MALLOC_POOL_INCR;
    __kmp_global.env_chunk       = FALSE;  /* KMP_CHUNK specified?     */
    __kmp_global.env_stksize     = FALSE;  /* KMP_STACKSIZE specified? */
    __kmp_global.env_omp_stksize = FALSE;  /* OMP_STACKSIZE specified? */
    __kmp_global.env_all_threads     = FALSE;/* KMP_ALL_THREADS or KMP_MAX_THREADS specified? */
    __kmp_global.env_omp_all_threads = FALSE;/* OMP_THREAD_LIMIT specified? */
    __kmp_global.env_checks      = FALSE;  /* KMP_CHECKS specified?    */
    __kmp_global.env_consistency_check  = FALSE;  /* KMP_CONSISTENCY_CHECK specified? */
    __kmp_global.generate_warnings = kmp_warnings_low;
    __kmp_global.reserve_warn = 0;

#ifdef DEBUG_SUSPEND
    __kmp_global.suspend_count = 0;
#endif

    /* ------------------------------------------------------------------------- */

    __kmp_global.allThreadsSpecified = 0;

    __kmp_global.align_alloc = CACHE_LINE;

    __kmp_global.xproc = 0;
    __kmp_global.avail_proc = 0;
    __kmp_global.sys_min_stksize = KMP_MIN_STKSIZE;
    __kmp_global.sys_max_nth = KMP_MAX_NTH;
    __kmp_global.max_nth = 0;
    __kmp_global.threads_capacity = 0;
    __kmp_global.dflt_team_nth = 0;
    __kmp_global.dflt_team_nth_ub = 0;
    __kmp_global.tp_capacity = 0;
    __kmp_global.tp_cached = 0;
    __kmp_global.dflt_nested = TRUE;
    __kmp_global.dflt_max_active_levels = KMP_MAX_ACTIVE_LEVELS_LIMIT; /* max_active_levels limit */
#ifdef KMP_DFLT_NTH_CORES
    __kmp_global.ncores = 0;
#endif
    __kmp_global.abort_delay = 0;

    /* Initialize the library data structures when we fork a child process, defaults to TRUE */
    __kmp_global.need_register_atfork = TRUE; /* At initialization, call pthread_atfork to install fork handler */
    __kmp_global.need_register_atfork_specified = TRUE;

    //__kmp_global.gtid_mode = 2; /* use TLS functions to store gtid */
    //__kmp_global.adjust_gtid_mode = FALSE;

    __kmp_global.tls_gtid_min = INT_MAX;
    __kmp_global.foreign_tp = TRUE;
#if KMP_ARCH_X86 || KMP_ARCH_X86_64
    __kmp_global.inherit_fp_control = TRUE;
    __kmp_global.init_x87_fpu_control_word = 0;
    __kmp_global.init_mxcsr = 0;
#endif /* KMP_ARCH_X86 || KMP_ARCH_X86_64 */

#if KMP_NESTED_HOT_TEAMS
    __kmp_global.hot_teams_mode      = 0; /* 0 - free extra threads when reduced */
                                          /* 1 - keep extra threads when reduced */
    __kmp_global.hot_teams_max_level = 1; /* nesting level of hot teams */
#endif

#if KMP_ARCH_X86_64 && (KMP_OS_LINUX || KMP_OS_WINDOWS)
    __kmp_global.mic_type = non_mic;
#endif

#ifdef USE_LOAD_BALANCE
    __kmp_global.load_balance_interval   = 1.0;
#endif /* USE_LOAD_BALANCE */

    __kmp_global.nested_nth = { NULL, 0, 0 };

#if OMP_40_ENABLED
    __kmp_global.display_env           = FALSE;
    __kmp_global.display_env_verbose   = FALSE;
    __kmp_global.omp_cancellation      = FALSE;
#endif

    /* ------------------------------------------------------ */
    /* STATE mostly syncronized with global lock */
    /* data written to rarely by masters, read often by workers */
    __kmp_global.threads = NULL;
    __kmp_global.team_pool = NULL;
    __kmp_global.thread_pool = NULL;

    /* data read/written to often by masters */
    __kmp_global.nth = 0;
    __kmp_global.all_nth = 0;
    __kmp_global.thread_pool_nth = 0;
    __kmp_global.thread_pool_active_nth = 0;

    __kmp_global.root = NULL;
    /* ------------------------------------------------------ */

    __kmp_init_global = TRUE;
}

void
__kmp_global_destroy(void)
{
    int i;

    /*
    ABT_mutex_free(&__kmp_global.stdio_lock);
    ABT_mutex_free(&__kmp_global.cat_lock);
    ABT_mutex_free(&__kmp_global.initz_lock);
    ABT_mutex_free(&__kmp_global.task_team_lock);
    for (i = 0; i < KMP_NUM_CRIT_LOCKS; i++) {
        ABT_mutex_free(&__kmp_global.crit_lock[i]);
    }

    ABT_finalize();
    */
    glt_mutex_free(&__kmp_global.stdio_lock);
    glt_mutex_free(&__kmp_global.cat_lock);
    glt_mutex_free(&__kmp_global.initz_lock);
    glt_mutex_free(&__kmp_global.task_team_lock);
    for (i = 0; i < KMP_NUM_CRIT_LOCKS; i++) {
        glt_mutex_free(&__kmp_global.crit_lock[i]);
    }

    glt_finalize();
    __kmp_init_global = FALSE;
}


/*
 * kmp_abt_util_Linux.c -- platform specific routines.
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
#include "kmp_glt_stats.h"

#include <unistd.h>
#include <math.h>               // HUGE_VAL.
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#if KMP_OS_LINUX && !KMP_OS_CNK
# include <sys/sysinfo.h>
#elif KMP_OS_DARWIN
# include <sys/sysctl.h>
# include <mach/mach.h>
#endif

/* TODO: Do we need to include pthread.h? */
# include <pthread.h>

#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>

//#define ABT_USE_MONITOR
#define ABT_USE_PRIVATE_POOLS
//#define ABT_USE_SCHED_SLEEP

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */




struct kmp_sys_timer {
    struct timespec     start;
};

// Convert timespec to nanoseconds.
#define TS2NS(timespec) (((timespec).tv_sec * 1e9) + (timespec).tv_nsec)

static struct kmp_sys_timer __kmp_sys_timer_data;

#if KMP_HANDLE_SIGNALS
    typedef void                            (* sig_func_t )( int );
    STATIC_EFI2_WORKAROUND struct sigaction    __kmp_sighldrs[ NSIG ];
    static sigset_t                            __kmp_sigset;
#endif

static int __kmp_fork_count = 0;

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static int __kmp_abt_sched_init(ABT_sched sched, ABT_sched_config config);
static void __kmp_abt_sched_run_es0(ABT_sched sched);
static void __kmp_abt_sched_run(ABT_sched sched);
static int __kmp_abt_sched_free(ABT_sched sched);


typedef struct kmp_abt {
/*    ABT_xstream *xstream;
    ABT_sched *sched;
    ABT_pool *priv_pool;
    ABT_pool *shared_pool;
    int num_xstreams;
    int num_pools;
*/
} kmp_abt_t;

static kmp_abt_t *__kmp_abt = NULL;

static inline
ABT_pool __kmp_abt_get_pool( int gtid )
{
  /*  KMP_DEBUG_ASSERT(__kmp_abt != NULL);
    KMP_DEBUG_ASSERT(gtid >= 0);

#ifdef ABT_USE_PRIVATE_POOLS
    if (gtid < __kmp_abt->num_xstreams)
        return __kmp_abt->priv_pool[gtid];
    else {
        int eid = gtid % __kmp_abt->num_xstreams;
        return __kmp_abt->shared_pool[eid];
    }
#else  ABT_USE_PRIVATE_POOLS 
    int eid = gtid % __kmp_abt->num_xstreams;
    return __kmp_abt->shared_pool[eid];
#endif  ABT_USE_PRIVATE_POOLS */
}

static inline
ABT_pool __kmp_abt_get_my_pool(int gtid)
{
    /*int eid;
    if (gtid < __kmp_abt->num_xstreams) {
        return __kmp_abt->shared_pool[gtid];
    } else {
        ABT_xstream_self_rank(&eid);
        return __kmp_abt->shared_pool[eid];
    }*/
}

static void __kmp_abt_initialize(void)
{
    /*int status;
    char *env;

    int num_xstreams, num_pools;
    int i, k;

     Is __kmp_global.xproc a reasonable value for the number of ESs? 
    env = getenv("KMP_ABT_NUM_ESS");
    if (env) {
        num_xstreams = atoi(env);
        if (num_xstreams < __kmp_global.xproc) __kmp_global.xproc = num_xstreams;
    } else {
        num_xstreams = __kmp_global.xproc;
    }
    num_pools = num_xstreams;
    KA_TRACE( 10, ("__kmp_abt_initialize: # of ESs = %d\n", num_xstreams ) );

    __kmp_abt = (kmp_abt_t *)__kmp_allocate(sizeof(kmp_abt_t));
    __kmp_abt->xstream = (ABT_xstream *)__kmp_allocate(num_xstreams * sizeof(ABT_xstream));
    __kmp_abt->sched = (ABT_sched *)__kmp_allocate(num_xstreams * sizeof(ABT_sched));
    __kmp_abt->priv_pool = (ABT_pool *)__kmp_allocate(num_pools * sizeof(ABT_pool));
    __kmp_abt->shared_pool = (ABT_pool *)__kmp_allocate(num_pools * sizeof(ABT_pool));
    __kmp_abt->num_xstreams = num_xstreams;
    __kmp_abt->num_pools = num_pools;

    /* Create private pools 
#ifdef ABT_USE_PRIVATE_POOLS
    for (i = 0; i < num_xstreams; i++) {
        status = ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPSC, ABT_TRUE,
                                       &__kmp_abt->priv_pool[i]);
        KMP_CHECK_SYSFAIL( "ABT_pool_create_basic", status );

        status = ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                                       &__kmp_abt->shared_pool[i]);
        KMP_CHECK_SYSFAIL( "ABT_pool_create_basic", status );
    }
#else  ABT_USE_PRIVATE_POOLS */
    /* NOTE: We create only one private pool for ES0. 
    status = ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPSC, ABT_TRUE,
                                   &__kmp_abt->priv_pool[0]);
    KMP_CHECK_SYSFAIL( "ABT_pool_create_basic", status );

    for (i = 0; i < num_xstreams; i++) {
        status = ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                                       &__kmp_abt->shared_pool[i]);
        KMP_CHECK_SYSFAIL( "ABT_pool_create_basic", status );
    }
#endif /* ABT_USE_PRIVATE_POOLS */

    /* Create a scheduler for ES0 
    ABT_sched_config_var cv_freq = {
        .idx = 0,
        .type = ABT_SCHED_CONFIG_INT
    };

    ABT_sched_config config;
    int freq = (num_xstreams < 100) ? 100 : num_xstreams;
    ABT_sched_config_create(&config, cv_freq, freq, ABT_sched_config_var_end);

    ABT_sched_def sched_def = {
        .type = ABT_SCHED_TYPE_ULT,
        .init = __kmp_abt_sched_init,
        .run  = __kmp_abt_sched_run_es0,
        .free = __kmp_abt_sched_free,
        .get_migr_pool = NULL
    };

    ABT_pool *my_pools = (ABT_pool *)malloc((num_xstreams+1) * sizeof(ABT_pool));
    my_pools[0] = __kmp_abt->priv_pool[0];
    for (k = 0; k < num_xstreams; k++) {
        my_pools[k+1] = __kmp_abt->shared_pool[k];
    }
    status = ABT_sched_create(&sched_def, num_xstreams+1, my_pools,
                              config, &__kmp_abt->sched[0]);
    KMP_CHECK_SYSFAIL( "ABT_sched_create", status );

    /* Create schedulers for other ESs 
    sched_def.run = __kmp_abt_sched_run;
#ifdef ABT_USE_PRIVATE_POOLS
    for (i = 1; i < num_xstreams; i++) {
        my_pools[0] = __kmp_abt->priv_pool[i];
        for (k = 0; k < num_xstreams; k++) {
            my_pools[k+1] = __kmp_abt->shared_pool[(i + k) % num_xstreams];
        }
        status = ABT_sched_create(&sched_def, num_xstreams+1, my_pools,
                                  config, &__kmp_abt->sched[i]);
        KMP_CHECK_SYSFAIL( "ABT_sched_create", status );
    }
#else /* ABT_USE_PRIVATE_POOLS 
    for (i = 1; i < num_xstreams; i++) {
        for (k = 0; k < num_xstreams; k++) {
            my_pools[k] = __kmp_abt->shared_pool[(i + k) % num_xstreams];
        }
        status = ABT_sched_create(&sched_def, num_xstreams, my_pools,
                                  config, &__kmp_abt->sched[i]);
        KMP_CHECK_SYSFAIL( "ABT_sched_create", status );
    }
#endif /* ABT_USE_PRIVATE_POOLS 

    free(my_pools);
    ABT_sched_config_free(&config);

    /* Create ESs 
    status = ABT_xstream_self(&__kmp_abt->xstream[0]);
    KMP_CHECK_SYSFAIL( "ABT_xstream_self", status );
    status = ABT_xstream_set_main_sched(__kmp_abt->xstream[0], __kmp_abt->sched[0]);
    KMP_CHECK_SYSFAIL( "ABT_xstream_set_main_sched", status );
    for (i = 1; i < num_xstreams; i++) {
        status = ABT_xstream_create(__kmp_abt->sched[i], &__kmp_abt->xstream[i]);
        KMP_CHECK_SYSFAIL( "ABT_xstream_create", status );
    }*/
}

static void __kmp_abt_finalize(void)
{
    int status;
    int i;
/*
    for (i = 1; i < __kmp_abt->num_xstreams; i++) {
        status = ABT_xstream_join(__kmp_abt->xstream[i]);
        KMP_CHECK_SYSFAIL( "ABT_xstream_join", status );
        status = ABT_xstream_free(&__kmp_abt->xstream[i]);
        KMP_CHECK_SYSFAIL( "ABT_xstream_free", status );
    }

    /* Free schedulers */
  /*  for (i = 1; i < __kmp_abt->num_xstreams; i++) {
        status = ABT_sched_free(&__kmp_abt->sched[i]);
        KMP_CHECK_SYSFAIL( "ABT_sched_free", status );
    }

    __kmp_free(__kmp_abt->xstream);
    __kmp_free(__kmp_abt->sched);
    __kmp_free(__kmp_abt->priv_pool);
    __kmp_free(__kmp_abt->shared_pool);
    __kmp_free(__kmp_abt);
    __kmp_abt = NULL;*/
}

typedef struct {
    uint32_t event_freq;
} __kmp_abt_sched_data_t;

static int __kmp_abt_sched_init(ABT_sched sched, ABT_sched_config config)
{
 /*   __kmp_abt_sched_data_t *p_data;
    p_data = (__kmp_abt_sched_data_t *)calloc(1, sizeof(__kmp_abt_sched_data_t));

    ABT_sched_config_read(config, 1, &p_data->event_freq);
    ABT_sched_set_data(sched, (void *)p_data);

    return ABT_SUCCESS;*/
}

static void __kmp_abt_sched_run_es0(ABT_sched sched)
{
/*    uint32_t work_count = 0;
    __kmp_abt_sched_data_t *p_data;
    int num_pools;
    ABT_pool *pools;
    ABT_unit unit;
    int target;
    ABT_bool stop;
    unsigned seed = time(NULL);
    size_t size;

    int run_cnt = 0;
#ifdef ABT_USE_SCHED_SLEEP
    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 128;
#endif

    ABT_sched_get_data(sched, (void **)&p_data);
    ABT_sched_get_num_pools(sched, &num_pools);
    pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, pools);

    while (1) {
        run_cnt = 0;

        /* Execute one work unit from the private pool */
  /*      ABT_pool_get_size(pools[0], &size);
        if (size > 0) {
            ABT_pool_pop(pools[0], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[0]);
                run_cnt++;
            }
        }

        /* shared pool */
    /*    ABT_pool_get_size(pools[1], &size);
        if (size > 0) {
            ABT_pool_pop(pools[1], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[1]);
                run_cnt++;
            }
        }

        if (run_cnt == 0 && num_pools > 2) {
            /* Steal a work unit from other pools */
      /*      target = rand_r(&seed) % (num_pools-2) + 2;
            ABT_pool_get_size(pools[target], &size);
            if (size > 0) {
                ABT_pool_pop(pools[target], &unit);
                if (unit != ABT_UNIT_NULL) {
                    ABT_unit_set_associated_pool(unit, pools[1]);
                    ABT_xstream_run_unit(unit, pools[1]);
                    run_cnt++;
                }
            }
        }

        if (++work_count >= p_data->event_freq) {
            ABT_xstream_check_events(sched);
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE) break;
            work_count = 0;
#ifdef ABT_USE_SCHED_SLEEP
            if (run_cnt == 0) {
                nanosleep(&sleep_time, NULL);
                if (sleep_time.tv_nsec < 1048576) {
                    sleep_time.tv_nsec <<= 2;
                }
            } else {
                sleep_time.tv_nsec = 128;
                run_cnt = 0;
            }
#endif /* ABT_USE_SCHED_SLEEP 
        }
    }

    free(pools);*/
}

static void __kmp_abt_sched_run(ABT_sched sched)
{
   /* uint32_t work_count = 0;
    __kmp_abt_sched_data_t *p_data;
    int num_pools;
    ABT_pool *pools;
    ABT_unit unit;
    int target;
    ABT_bool stop;
    unsigned seed = time(NULL);
    size_t size;

    int run_cnt = 0;
#ifdef ABT_USE_SCHED_SLEEP
    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 128;
#endif

    ABT_sched_get_data(sched, (void **)&p_data);
    ABT_sched_get_num_pools(sched, &num_pools);
    pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, pools);

    while (1) {
#ifdef ABT_USE_PRIVATE_POOLS
        run_cnt = 0;

        /* Execute one work unit from the private pool */
     /*   ABT_pool_get_size(pools[0], &size);
        if (size > 0) {
            ABT_pool_pop(pools[0], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[0]);
                run_cnt++;
            }
        }

        /* shared pool */
       /* ABT_pool_get_size(pools[1], &size);
        if (size > 0) {
            ABT_pool_pop(pools[1], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[1]);
                run_cnt++;
            }
        }

        /* Steal a work unit from other pools */
        /*if (run_cnt == 0) {
            target = rand_r(&seed) % (num_pools-2) + 2;
            ABT_pool_get_size(pools[target], &size);
            if (size > 0) {
                ABT_pool_pop(pools[target], &unit);
                if (unit != ABT_UNIT_NULL) {
                    ABT_unit_set_associated_pool(unit, pools[1]);
                    ABT_xstream_run_unit(unit, pools[1]);
                    run_cnt++;
                }
            }
        }
#else /* ABT_USE_PRIVATE_POOLS */
        /* Execute one work unit from the scheduler's pool */
       /* ABT_pool_get_size(pools[0], &size);
        if (size > 0) {
            ABT_pool_pop(pools[0], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[0]);
                run_cnt++;
            }
        }

        /* Steal a work unit from other pools */
       /* target = rand_r(&seed) % (num_pools-1) + 1;
        ABT_pool_get_size(pools[target], &size);
        if (size > 0) {
            ABT_pool_pop(pools[target], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_unit_set_associated_pool(unit, pools[0]);
                ABT_xstream_run_unit(unit, pools[0]);
                run_cnt++;
            }
        }
#endif /* ABT_USE_PRIVATE_POOLS */

       /* if (++work_count >= p_data->event_freq) {
            ABT_xstream_check_events(sched);
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE) break;
            work_count = 0;
#ifdef ABT_USE_SCHED_SLEEP
            if (run_cnt == 0) {
                nanosleep(&sleep_time, NULL);
                if (sleep_time.tv_nsec < 1048576) {
                    sleep_time.tv_nsec <<= 2;
                }
            } else {
                sleep_time.tv_nsec = 128;
                run_cnt = 0;
            }
#endif /* ABT_USE_SCHED_SLEEP */
       /* }
    }

    free(pools);*/
}

static int __kmp_abt_sched_free(ABT_sched sched)
{
 /*   __kmp_abt_sched_data_t *p_data;

    ABT_sched_get_data(sched, (void **)&p_data);
    free(p_data);

    return ABT_SUCCESS;*/
}

//#if KMP_DEBUG
//void __kmp_abt_print_thread( kmp_info_t *th, const char *msg )
//{
 /*   if (kmp_a_debug == 0) return;

    int gtid = th->th.th_info.ds.ds_gtid;
    ABT_thread thread = th->th.th_info.ds.ds_thread;
    ABT_thread self;
    ABT_thread_id thread_id;
    ABT_xstream xstream;
    ABT_pool pool;
    int es_rank, pool_id;
    size_t pool_size, pool_total_size;

    ABT_thread_self(&self);
    ABT_thread_get_id(self, &thread_id);

    ABT_xstream_self(&xstream);
    ABT_xstream_self_rank(&es_rank);
    ABT_thread_get_last_pool(self, &pool);
    ABT_pool_get_id(pool, &pool_id);
    ABT_pool_get_size(pool, &pool_size);
    ABT_pool_get_total_size(pool, &pool_total_size);

    printf("%s: T#%d - U%u (%p) on ES%d (%p) from P%d (%p,%lu,%lu)\n",
           msg, gtid, (unsigned)thread_id, self,
           es_rank, xstream,
           pool_id, pool, pool_size, pool_total_size);
    fflush(stdout);*/
//}
//#endif


static int
__kmp_get_xproc( void ) {

    int r = 0;

    #if KMP_OS_LINUX || KMP_OS_FREEBSD || KMP_OS_NETBSD

        r = sysconf( _SC_NPROCESSORS_ONLN );

    #elif KMP_OS_DARWIN

        // Bug C77011 High "OpenMP Threads and number of active cores".

        // Find the number of available CPUs.
        kern_return_t          rc;
        host_basic_info_data_t info;
        mach_msg_type_number_t num = HOST_BASIC_INFO_COUNT;
        rc = host_info( mach_host_self(), HOST_BASIC_INFO, (host_info_t) & info, & num );
        if ( rc == 0 && num == HOST_BASIC_INFO_COUNT ) {
            // Cannot use KA_TRACE() here because this code works before trace support is
            // initialized.
            r = info.avail_cpus;
        } else {
            KMP_WARNING( CantGetNumAvailCPU );
            KMP_INFORM( AssumedNumCPU );
        }; // if

    #else

        #error "Unknown or unsupported OS."

    #endif

    return r > 0 ? r : 2; /* guess value of 2 if OS told us 0 */

} // __kmp_get_xproc


void
__kmp_runtime_initialize( void )
{
    int status;

    if (__kmp_global.init_runtime) return;
    
    #if ( KMP_ARCH_X86 || KMP_ARCH_X86_64 )
        if ( ! __kmp_global.cpuinfo.initialized ) {
            __kmp_query_cpuid( &__kmp_global.cpuinfo );
        }; // if
    #endif /* KMP_ARCH_X86 || KMP_ARCH_X86_64 */

    __kmp_global.xproc = __kmp_get_xproc();

    if ( sysconf( _SC_THREADS ) ) {

        /* Query the maximum number of threads */
        __kmp_global.sys_max_nth = sysconf( _SC_THREAD_THREADS_MAX );
        if ( __kmp_global.sys_max_nth == -1 ) {
            /* Unlimited threads for NPTL */
            __kmp_global.sys_max_nth = INT_MAX;
        }
        else if ( __kmp_global.sys_max_nth <= 1 ) {
            /* Can't tell, just use PTHREAD_THREADS_MAX */
            __kmp_global.sys_max_nth = KMP_MAX_NTH;
        }

        /* Query the minimum stack size */
        __kmp_global.sys_min_stksize = sysconf( _SC_THREAD_STACK_MIN );
        if ( __kmp_global.sys_min_stksize <= 1 ) {
            __kmp_global.sys_min_stksize = KMP_MIN_STKSIZE;
        }
    }

    /* Set up minimum number of threads to switch to TLS gtid */
    __kmp_global.tls_gtid_min = KMP_TLS_GTID_MIN;

    /* use TLS functions to store gtid */
    //__kmp_global.gtid_mode = 2;
    
    __kmp_abt_initialize();

    // FIXME: Do we need a thread-specific key?
    //status = ABT_key_create( __kmp_internal_end_dest, &__kmp_global.gtid_threadprivate_key );
//    status = ABT_key_create( NULL, &__kmp_global.gtid_threadprivate_key );
//    KMP_CHECK_SYSFAIL( "ABT_key_create", status );

//    status = ABT_mutex_create( & __kmp_wait_mx.m_mutex );
//    KMP_CHECK_SYSFAIL( "ABT_mutex_create", status );
//    status = ABT_cond_create( & __kmp_wait_cv.c_cond );
//    KMP_CHECK_SYSFAIL( "ABT_cond_create", status );

    __kmp_global.init_runtime = TRUE;
}


void
__kmp_runtime_destroy( void )
{
    int status;

    if ( ! __kmp_global.init_runtime ) {
        return; // Nothing to do.
    };

//    status = ABT_key_free( & __kmp_global.gtid_threadprivate_key );
//    KMP_CHECK_SYSFAIL( "ABT_key_free", status );

//    status = ABT_mutex_free( & __kmp_wait_mx.m_mutex );
//    if ( status != ABT_SUCCESS ) {
//        KMP_SYSFAIL( "ABT_mutex_free", status );
//    }
//    status = ABT_cond_free( & __kmp_wait_cv.c_cond );
//    if ( status != ABT_SUCCESS ) {
//        KMP_SYSFAIL( "ABT_cond_free", status );
//    }
//    #if KMP_AFFINITY_SUPPORTED
//        __kmp_affinity_uninitialize();
//    #endif

    __kmp_abt_finalize();

    __kmp_global.init_runtime = FALSE;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static inline
void __kmp_abt_free_task(kmp_info_t *th, kmp_taskdata_t *taskdata)
{
    int gtid = __kmp_gtid_from_thread(th);

    KA_TRACE(30, ("__kmp_free_task: (enter) T#%d - task %p\n", gtid, taskdata));

    /* [AC] we need those steps to mark the task as finished so the dependencies
     *  can be completed */
    taskdata -> td_flags.complete = 1;   // mark the task as completed
    __kmp_release_deps(gtid,taskdata);
    taskdata -> td_flags.executing = 0;  // suspend the finishing task
    // Check to make sure all flags and counters have the correct values
    //KMP_DEBUG_ASSERT( taskdata->td_flags.tasktype == TASK_EXPLICIT );
    //KMP_DEBUG_ASSERT( taskdata->td_flags.executing == 0 );
    //KMP_DEBUG_ASSERT( taskdata->td_flags.complete == 1 );
    //KMP_DEBUG_ASSERT( taskdata->td_flags.freed == 0 );
    //KMP_DEBUG_ASSERT( TCR_4(taskdata->td_allocated_child_tasks) == 0  || taskdata->td_flags.task_serial == 1);
    //KMP_DEBUG_ASSERT( TCR_4(taskdata->td_incomplete_child_tasks) == 0 );

    taskdata->td_flags.freed = 1;

    /* Free the task queue if it was allocated. */
    if (taskdata->td_task_queue) {
        KMP_DEBUG_ASSERT(taskdata->td_tq_cur_size == 0);
        KMP_INTERNAL_FREE(taskdata->td_task_queue);
    }

    // deallocate the taskdata and shared variable blocks associated with this task
    #if USE_FAST_MEMORY
        __kmp_fast_free( th, taskdata );
    #else /* ! USE_FAST_MEMORY */
        __kmp_thread_free( th, taskdata );
    #endif

    KA_TRACE(20, ("__kmp_free_task: (exit) T#%d - task %p\n", gtid, taskdata));
}

static void __kmp_execute_task(void *arg)
{
    int gtid;

    kmp_task_t *task = (kmp_task_t *)arg;
    kmp_taskdata_t *taskdata = KMP_TASK_TO_TASKDATA(task);
    kmp_info_t *th;

    /* [AC] we need to set some flags in the task data so the dependencies can 
     * be checked and fulfilled */
    taskdata -> td_flags.started = 1;
    taskdata -> td_flags.executing = 1;

    th = __kmp_bind_task_to_thread(taskdata->td_team, taskdata);
    gtid = __kmp_gtid_from_thread(th);

    KA_TRACE(20, ("__kmp_execute_task: T#%d before executing task %p.\n", gtid, task));

    /* [AC] Right now, we don't need to go throw OpenMP task management so we can
       just execute the task, don't we?*/
    //kmp_taskdata_t * current_task = __kmp_global.threads[ gtid ] -> th.th_current_task;
    //__kmp_invoke_task( gtid, task, current_task );
    (*(task->routine))(gtid, task);

    if (!taskdata->td_flags.tiedness) {
        /* If this task is an untied one, we need to retrieve kmp_info because
         * it may have been changed. */
        th = __kmp_get_self_info();
    }

    __kmp_abt_free_task(th, taskdata);

    /* Reset th's ownership */
    __kmp_release_info(th);

    KA_TRACE(20, ("__kmp_execute_task: T#%d after executing task %p.\n",
                  __kmp_gtid_from_thread(th), task));
}

int __kmp_create_task(kmp_info_t *th, kmp_task_t *task)
{
    int status;
    int gtid = __kmp_gtid_from_thread(th);
    ABT_pool dest = __kmp_abt_get_my_pool(gtid);

    KA_TRACE(20, ("__kmp_create_task: T#%d before creating task %p into the pool %p.\n",
                  gtid, task, dest));

    /* Check if the task queue has an empty slot */
    kmp_taskdata_t *td = th->th.th_current_task;
    if (td->td_tq_cur_size == td->td_tq_max_size) {
        size_t new_max_size;
        if (td->td_tq_max_size == 0) {
            /* Empty queue. We allocate 32 slots by default. */
            new_max_size = 32;
        } else {
            /* The task queue is full. Expand it if possible. */
            new_max_size = td->td_tq_max_size * 2;
            if (new_max_size > MAX_ABT_TASKS) {
                KA_TRACE(20, ("__kmp_create_task: T#%d - queue is full\n", gtid));
                return FALSE;
            }
        }

        void *queue = (void *)td->td_task_queue;
        size_t size = sizeof(kmp_abt_task_t) * new_max_size;
        td->td_task_queue = (kmp_abt_task_t *)KMP_INTERNAL_REALLOC(queue, size);
        td->td_tq_max_size = new_max_size;
    }

    status = ABT_thread_create(dest, __kmp_execute_task, (void *)task,
                               ABT_THREAD_ATTR_NULL,
                               &td->td_task_queue[td->td_tq_cur_size++]);
    KMP_ASSERT(status == ABT_SUCCESS);

    KA_TRACE(20, ("__kmp_create_task: T#%d after creating task %p into the pool %p.\n",
                  gtid, task, dest));

    return TRUE;
}

void __kmp_wait_child_tasks(kmp_info_t *th, int yield)
{
    KA_TRACE(20, ("__kmp_wait_child_tasks: T#%d enter\n", __kmp_gtid_from_thread(th)));

    int i, status;
    kmp_taskdata_t *taskdata = th->th.th_current_task;

    if (taskdata->td_tq_cur_size == 0) {
        /* leaf task case */
        if (yield) {
            __kmp_release_info(th);

            ABT_thread_yield();

            if (taskdata->td_flags.tiedness) {
                __kmp_acquire_info_for_task(th, taskdata);
            } else {
                __kmp_bind_task_to_thread(th->th.th_team, taskdata);
            }
        }
        return;
    }

    /* Let others, e.g., tasks, can use this kmp_info */
    __kmp_release_info(th);

    /* Give other tasks a chance for execution */
    if (yield) ABT_thread_yield();

    /* Wait until all child tasks are complete. */
    for (i = 0; i < taskdata->td_tq_cur_size; i++) {
        status = ABT_thread_free(&taskdata->td_task_queue[i]);
        KMP_ASSERT(status == ABT_SUCCESS);
    }
    taskdata->td_tq_cur_size = 0;

    if (taskdata->td_flags.tiedness) {
        /* Obtain kmp_info to continue the original task. */
        __kmp_acquire_info_for_task(th, taskdata);
    } else {
        th = __kmp_bind_task_to_thread(th->th.th_team, taskdata);
    }

    KA_TRACE(20, ("__kmp_wait_child_tasks: T#%d done\n", __kmp_gtid_from_thread(th)));
}

void __kmp_task_wait_a(kmp_int32 gtid, kmp_info_t * thread)
{
   /* KA_TRACE(20, ("__kmp_task_wait (enter): T#%d before checking.\n", gtid) );
    int i;
    int total_ntasks =  thread->th.tasks_in_the_queue;
    printf("__kmp_task_wait (enter): T#%d => %d tasks.\n", gtid,total_ntasks);
    for(i=0;i<total_ntasks;i++){
        printf("__kmp_task_wait (joining): T#%d => %d task.\n", gtid,i);
        ABT_thread_join(thread->th.th_task_queue[i]);
        printf("__kmp_task_wait (joined): T#%d => %d task.\n", gtid,i);
    }
    KA_TRACE(20, ("__kmp_task_wait (exit): T#%d.\n", gtid) );
*/
}

kmp_info_t *__kmp_bind_task_to_thread(kmp_team_t *team, kmp_taskdata_t *taskdata)
{
    int i, i_start, i_end;
    kmp_info_t *th = NULL;

    KA_TRACE(20, ("__kmp_bind_task_to_thread: (enter) task %p\n", taskdata));

    /* To handle gtid in the task code, we look for a suspended (blocked)
     * thread in the team and use its info to execute this task. */
    while (1) {
        if (team->t.t_level <= 1) {
            /* outermost team - we try to assign the thread that was executed on
             * the same ES first and then check other threads in the team.  */
            int rank;
            ABT_xstream_self_rank(&rank);
            if (rank < team->t.t_nproc) {
                /* [SM] I think this condition should always be true, but just in
                 * case I miss something we check this condition. */
                i_start = rank;
                i_end = team->t.t_nproc + rank;
            } else {
                i_start = 0;
                i_end = team->t.t_nproc;
            }

        } else {
            /* nested team - we ignore the ES info since threads in the nested team
             * may be executed by any ES. */
            i_start = 0;
            i_end = team->t.t_nproc;
        }

        /* TODO: This is a linear search. Can we do better? */
        for (i = i_start; i < i_end; i++) {
            int idx = (i < team->t.t_nproc) ? i : i % team->t.t_nproc;
            th = team->t.t_threads[idx];
            ABT_thread ult = th->th.th_info.ds.ds_thread;

            if (th->th.th_active == FALSE && ult != ABT_THREAD_NULL) {
                /* Try to take the ownership of kmp_info 'th' */
                if (KMP_COMPARE_AND_STORE_RET32(&th->th.th_active, FALSE, TRUE) == FALSE) {
                    /* Bind this task as if it is executed by 'th'. */
                    th->th.th_current_task = taskdata;
                    __kmp_set_self_info(th);
                    KA_TRACE(20, ("__kmp_bind_task_to_thread: (exit) task %p bound to T#%d\n",
                                  taskdata, __kmp_gtid_from_thread(th)));
                    return th;
                }
            }
        }

        /* We could not find an available kmp_info. Thus, this task yields
         * control to other work units and will try to find one later. */
        ABT_thread_yield();
    }

    return NULL;
}


/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static void
__kmp_launch_worker( void *thr )
{
    int status, old_type, old_state;
    int gtid;
    kmp_info_t *this_thr = (kmp_info_t *)thr;
    kmp_team_t *(*volatile pteam);

    gtid = this_thr->th.th_info.ds.ds_gtid;
    KMP_DEBUG_ASSERT( this_thr == __kmp_global.threads[ gtid ] );

///#if KMP_AFFINITY_SUPPORTED
///    __kmp_affinity_set_init_mask( gtid, FALSE );
///#endif

#if KMP_ARCH_X86 || KMP_ARCH_X86_64
    //
    // Set the FP control regs to be a copy of
    // the parallel initialization thread's.
    //
///    __kmp_clear_x87_fpu_status_word();
///    __kmp_load_x87_fpu_control_word( &__kmp_global.init_x87_fpu_control_word );
///    __kmp_load_mxcsr( &__kmp_global.init_mxcsr );
#endif /* KMP_ARCH_X86 || KMP_ARCH_X86_64 */

    KMP_MB();

    pteam = (kmp_team_t *(*))(& this_thr->th.th_team);
    if ( TCR_SYNC_PTR(*pteam) && !TCR_4(__kmp_global.g.g_done) ) {
        /* run our new task */
        if ( TCR_SYNC_PTR((*pteam)->t.t_pkfn) != NULL ) {
            int rc;
            KA_TRACE(20, ("__kmp_launch_worker: T#%d(%d:%d) invoke microtask = %p\n",
                          gtid, (*pteam)->t.t_id, __kmp_tid_from_gtid(gtid), (*pteam)->t.t_pkfn));

            //updateHWFPControl (*pteam);

            KMP_STOP_DEVELOPER_EXPLICIT_TIMER(USER_launch_thread_loop);
            {
                KMP_TIME_DEVELOPER_BLOCK(USER_worker_invoke);
                rc = (*pteam)->t.t_invoke( gtid );
            }
            KMP_START_DEVELOPER_EXPLICIT_TIMER(USER_launch_thread_loop);
            KMP_ASSERT( rc );

            KMP_MB();
            KA_TRACE(20, ("__kmp_launch_worker: T#%d(%d:%d) done microtask = %p\n",
                          gtid, (*pteam)->t.t_id, __kmp_tid_from_gtid(gtid), (*pteam)->t.t_pkfn));
        }
    }

    //this_thr->th.th_task_team = NULL;
    /* run the destructors for the threadprivate data for this thread */
    //__kmp_common_destroy_gtid( gtid );

    KA_TRACE( 10, ("__kmp_launch_worker: T#%d done\n", gtid) );

    /* [AC]*/
    __kmp_wait_child_tasks(this_thr, FALSE);

    /* Below is for the implicit task */
    kmp_taskdata_t *td = this_thr->th.th_current_task;
    if (td->td_task_queue) {
        KMP_DEBUG_ASSERT(td->td_tq_cur_size == 0);
        KMP_INTERNAL_FREE(td->td_task_queue);
        td->td_task_queue = NULL;
        td->td_tq_max_size = 0;
    }
}

#ifdef KMP_ABT_USE_TASKLET_TEAM
static void
__kmp_launch_tasklet_worker( void *thr )
{
    int gtid, tid;
    kmp_info_t *this_thr = (kmp_info_t *)thr;
    kmp_team_t *(*volatile pteam);

    gtid = this_thr->th.th_info.ds.ds_gtid;
    tid = this_thr->th.th_info.ds.ds_tid;

    KMP_MB();

    KA_TRACE( 10, ("__kmp_launch_tasklet_worker: T#%d:%d enter\n", gtid, tid) );

    pteam = (kmp_team_t *(*))(& this_thr->th.th_team);
    if ( TCR_SYNC_PTR(*pteam) && !TCR_4(__kmp_global.g.g_done) ) {
        /* run our new task */
        if ( TCR_SYNC_PTR((*pteam)->t.t_pkfn) != NULL ) {
            int rc;
            __kmp_run_before_invoked_task( gtid, tid, this_thr, *pteam );
            rc = __kmp_invoke_microtask( (microtask_t) TCR_SYNC_PTR((*pteam)->t.t_pkfn),
                    gtid, tid, (int)(*pteam)->t.t_argc, (void **)(*pteam)->t.t_argv);
            __kmp_run_after_invoked_task( gtid, tid, this_thr, *pteam );
            //rc = (*pteam)->t.t_invoke( gtid );
            KMP_ASSERT( rc );
        }
    }

    KMP_MB();

    KA_TRACE( 10, ("__kmp_launch_tasklet_worker: T#%d:%d done\n", gtid, tid) );
}

#else
static void
__kmp_launch_tasklet_worker( void *thr )
{
    int gtid;
    kmp_info_t *this_thr = (kmp_info_t *)thr;
    kmp_team_t *(*volatile pteam);

    gtid = this_thr->th.th_info.ds.ds_gtid;
    KMP_DEBUG_ASSERT( this_thr == __kmp_global.threads[ gtid ] );

    KMP_MB();

    pteam = (kmp_team_t *(*))(& this_thr->th.th_team);
    if ( TCR_SYNC_PTR(*pteam) && !TCR_4(__kmp_global.g.g_done) ) {
        /* run our new task */
        if ( TCR_SYNC_PTR((*pteam)->t.t_pkfn) != NULL ) {
            int rc;
            KA_TRACE(20, ("__kmp_launch_tasklet_worker: T#%d(%d:%d) invoke microtask = %p\n",
                          gtid, (*pteam)->t.t_id, __kmp_tid_from_gtid(gtid), (*pteam)->t.t_pkfn));

            KMP_STOP_DEVELOPER_EXPLICIT_TIMER(USER_launch_thread_loop);
            {
                KMP_TIME_DEVELOPER_BLOCK(USER_worker_invoke);
                rc = (*pteam)->t.t_invoke( gtid );
            }
            KMP_START_DEVELOPER_EXPLICIT_TIMER(USER_launch_thread_loop);
            KMP_ASSERT( rc );

            KMP_MB();
            KA_TRACE(20, ("__kmp_launch_tasklet_worker: T#%d(%d:%d) done microtask = %p\n",
                          gtid, (*pteam)->t.t_id, __kmp_tid_from_gtid(gtid), (*pteam)->t.t_pkfn));
        }
    }

    KA_TRACE( 10, ("__kmp_launch_tasklet_worker: T#%d done\n", gtid) );
}
#endif

void
__kmp_create_uber( int gtid, kmp_info_t *th, size_t stack_size )
{
    KMP_DEBUG_ASSERT( KMP_UBER_GTID(gtid) );
    KA_TRACE( 10, ("__kmp_create_uber: T#%d\n", gtid) );

    ABT_thread handle;
    ABT_thread_self( &handle );
    ABT_thread_set_arg(handle, (void *)th);
    th -> th.th_info.ds.ds_thread = handle;
}

void
__kmp_create_worker( int gtid, kmp_info_t *th, size_t stack_size )
{
    ABT_thread      handle;
    ABT_thread_attr thread_attr;
    int             status;

    // [SM] th->th.th_info.ds.ds_gtid is setup in __kmp_allocate_thread
    KMP_DEBUG_ASSERT( th->th.th_info.ds.ds_gtid == gtid );

    /* uber thread is created in __kmp_create_uber(). */
    KMP_DEBUG_ASSERT( !KMP_UBER_GTID(gtid) );

    KA_TRACE( 10, ("__kmp_create_worker: try to create T#%d\n", gtid) );

    status = ABT_thread_attr_create( &thread_attr );
    if ( status != ABT_SUCCESS ) {
        __kmp_msg(kmp_ms_fatal, KMP_MSG( CantInitThreadAttrs ), KMP_ERR( status ), __kmp_msg_null);
    }; // if

    KA_TRACE( 10, ( "__kmp_create_worker: T#%d, default stacksize = %lu bytes, "
                    "__kmp_global.stksize = %lu bytes, final stacksize = %lu bytes\n",
                    gtid, KMP_DEFAULT_STKSIZE, __kmp_global.stksize, stack_size ) );

    status = ABT_thread_attr_set_stacksize( thread_attr, stack_size );
    if ( status != ABT_SUCCESS ) {
        __kmp_msg(kmp_ms_fatal, KMP_MSG( CantSetWorkerStackSize, stack_size ), KMP_ERR( status ),
                  KMP_HNT( ChangeWorkerStackSize  ), __kmp_msg_null);
    }; // if

    // If this new thread is for nested parallel region, the new thread is
    // added to the shared pool of ES where the caller thread is running on.
    ABT_pool tar_pool;
    if (th->th.th_team->t.t_level > 1) {
        tar_pool = __kmp_abt_get_my_pool(gtid);
    } else {
        tar_pool = __kmp_abt_get_pool(gtid);
    }
    KA_TRACE( 10, ("__kmp_create_worker: T#%d, nesting level=%d, target pool=%p\n",
                   gtid, th->th.th_team->t.t_level, tar_pool) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    status = ABT_thread_create( tar_pool, __kmp_launch_worker, (void *)th, thread_attr, &handle );
    KMP_ASSERT( status == ABT_SUCCESS );

    th->th.th_info.ds.ds_thread = handle;

    status = ABT_thread_attr_free( & thread_attr );
    KMP_ASSERT( status == ABT_SUCCESS );

    KA_TRACE( 10, ("__kmp_create_worker: done creating T#%d\n", gtid) );

} // __kmp_create_worker

void
__kmp_create_tasklet_worker( int gtid, kmp_info_t *th )
{
    int status;

    // [SM] th->th.th_info.ds.ds_gtid is setup in __kmp_allocate_thread
    KMP_DEBUG_ASSERT( th->th.th_info.ds.ds_gtid == gtid );

    KA_TRACE( 10, ("__kmp_create_tasklet_worker: try to create T#%d\n", gtid) );

    // If this new tasklet is for nested parallel region, the new tasklet is
    // added to the shared pool of ES where the caller is running on.
    ABT_pool tar_pool;
    if (th->th.th_team->t.t_level > 1) {
        tar_pool = __kmp_abt_get_my_pool(gtid);
    } else {
        tar_pool = __kmp_abt_get_pool(gtid);
    }
    KA_TRACE( 10, ("__kmp_create_tasklet_worker: T#%d, nesting level=%d, target pool=%p\n",
                   gtid, th->th.th_team->t.t_level, tar_pool) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    status = ABT_task_create( tar_pool, __kmp_launch_tasklet_worker, (void *)th,
                              &th->th.th_info.ds.ds_tasklet );
    KMP_ASSERT( status == ABT_SUCCESS );

    KA_TRACE( 10, ("__kmp_create_tasklet_worker: done creating T#%d\n", gtid) );

} // __kmp_create_tasklet_worker

void
__kmp_revive_worker( kmp_info_t *th )
{
    int status;
    int gtid;
    ABT_pool tar_pool;

    gtid = th->th.th_info.ds.ds_gtid;

    if (th->th.th_team->t.t_level > 1) {
        tar_pool = __kmp_abt_get_my_pool(gtid);
    } else {
        tar_pool = __kmp_abt_get_pool(gtid);
    }

    KA_TRACE( 10, ("__kmp_revive_worker: recreate T#%d\n", gtid) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    status = ABT_thread_revive( tar_pool, __kmp_launch_worker, (void *)th,
                                &th->th.th_info.ds.ds_thread );
    KMP_ASSERT( status == ABT_SUCCESS );

    KA_TRACE( 10, ("__kmp_revive_worker: done recreating T#%d\n", gtid) );
}

void
__kmp_revive_tasklet_worker( kmp_info_t *th )
{
    int status;
    int gtid;
    ABT_pool tar_pool;

    gtid = th->th.th_info.ds.ds_gtid;

    if (th->th.th_team->t.t_level > 1) {
        tar_pool = __kmp_abt_get_my_pool(gtid);
    } else {
        tar_pool = __kmp_abt_get_pool(gtid);
    }

    KA_TRACE( 10, ("__kmp_revive_tasklet_worker: recreate T#%d\n", gtid) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    status = ABT_task_revive( tar_pool, __kmp_launch_worker, (void *)th,
                              &th->th.th_info.ds.ds_tasklet );
    KMP_ASSERT( status == ABT_SUCCESS );

    KA_TRACE( 10, ("__kmp_revive_tasklet_worker: done recreating T#%d\n", gtid) );
}

void
__kmp_join_worker( kmp_info_t *th )
{
    int status;

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    KA_TRACE( 10, ("__kmp_join_worker: try to join worker T#%d\n", th->th.th_info.ds.ds_gtid) );

#ifndef KMP_ABT_USE_TASKLET_TEAM
    if (get__tasklet(th)) {
        ABT_task ds_tasklet = th->th.th_info.ds.ds_tasklet;
        status = ABT_task_join(ds_tasklet);
        KMP_ASSERT( status == ABT_SUCCESS );
    } else {
#endif
        ABT_thread ds_thread = th->th.th_info.ds.ds_thread;
        status = ABT_thread_join(ds_thread);
        KMP_ASSERT( status == ABT_SUCCESS );
#ifndef KMP_ABT_USE_TASKLET_TEAM
    }
#endif

    KA_TRACE( 10, ("__kmp_join_worker: done joining worker T#%d\n", th->th.th_info.ds.ds_gtid) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */
} // __kmp_join_worker

void
__kmp_reap_worker( kmp_info_t *th )
{
    int          status;
    void        *exit_val;

    KMP_MB();       /* Flush all pending memory write invalidates.  */

    KA_TRACE( 10, ("__kmp_reap_worker: try to free worker T#%d\n", th->th.th_info.ds.ds_gtid ) );

    ABT_thread ds_thread = th->th.th_info.ds.ds_thread;
    if (ds_thread != ABT_THREAD_NULL) {
        status = ABT_thread_free( &ds_thread );
        KMP_ASSERT(status == ABT_SUCCESS);
    }

    ABT_task ds_tasklet = th->th.th_info.ds.ds_tasklet;
    if (ds_tasklet != ABT_TASK_NULL) {
        status = ABT_task_free( &ds_tasklet );
        KMP_ASSERT(status == ABT_SUCCESS);
    }

    KA_TRACE( 10, ("__kmp_reap_worker: done reaping T#%d\n", th->th.th_info.ds.ds_gtid ) );

    KMP_MB();       /* Flush all pending memory write invalidates.  */
}

int
__kmp_barrier( int gtid )
{
    register int tid = __kmp_tid_from_gtid(gtid);
    register kmp_info_t *this_thr = __kmp_global.threads[gtid];
    register kmp_team_t *team = this_thr->th.th_team;
    register int status = 0;
    ident_t *loc = this_thr->th.th_ident;
    int ret;

    KA_TRACE(15, ("__kmp_barrier: T#%d(%d:%d) has arrived\n",
                  gtid, __kmp_team_from_gtid(gtid)->t.t_id, __kmp_tid_from_gtid(gtid)));

    /* Complete and free all child tasks */
    __kmp_wait_child_tasks(this_thr, FALSE);

    if (!team->t.t_serialized) {
        KMP_MB();

        kmp_taskdata_t *taskdata = this_thr->th.th_current_task;

        __kmp_release_info(this_thr);

        if (KMP_MASTER_TID(tid)) {
            status = 0;
            ret = ABT_barrier_wait( team->t.t_bar );
            KMP_DEBUG_ASSERT( ret == ABT_SUCCESS );

        } else {
            status = 1;
            ret = ABT_barrier_wait( team->t.t_bar );
            KMP_DEBUG_ASSERT( ret == ABT_SUCCESS );
        }

        __kmp_acquire_info_for_task(this_thr, taskdata);
    } else { // Team is serialized.
        status = 0;
    }
    KA_TRACE(15, ("__kmp_barrier: T#%d(%d:%d) is leaving with return value %d\n",
                  gtid, __kmp_team_from_gtid(gtid)->t.t_id, __kmp_tid_from_gtid(gtid), status));

    return status;
}

int
__kmp_begin_split_barrier( int gtid )
{
    register int tid = __kmp_tid_from_gtid(gtid);
    register kmp_info_t *this_thr = __kmp_global.threads[gtid];
    register kmp_team_t *team = this_thr->th.th_team;
    register int status = 0;
    ident_t *loc = this_thr->th.th_ident;
    int ret;

    KA_TRACE(15, ("__kmp_begin_split_barrier: T#%d(%d:%d) has arrived\n",
                  gtid, __kmp_team_from_gtid(gtid)->t.t_id, __kmp_tid_from_gtid(gtid)));

    if (!team->t.t_serialized) {
        KMP_MB();

        if (KMP_MASTER_TID(tid)) {
            status = 0;
        } else {
            status = 1;
            ret = ABT_barrier_wait( team->t.t_bar );
            KMP_DEBUG_ASSERT( ret == ABT_SUCCESS );
        }

    } else { // Team is serialized.
        status = 0;
    }
    KA_TRACE(15, ("__kmp_begin_split_barrier: T#%d(%d:%d) is leaving with return value %d\n",
                  gtid, __kmp_team_from_gtid(gtid)->t.t_id, __kmp_tid_from_gtid(gtid), status));

    return status;
}

void
__kmp_end_split_barrier( int gtid )
{
    int tid = __kmp_tid_from_gtid(gtid);
    kmp_info_t *this_thr = __kmp_global.threads[gtid];
    kmp_team_t *team = this_thr->th.th_team;

    if (!team->t.t_serialized) {
        if (KMP_MASTER_GTID(gtid)) {
            int ret = ABT_barrier_wait( team->t.t_bar );
            KMP_DEBUG_ASSERT( ret == ABT_SUCCESS );
        }
    }
}

void
__kmp_init_nest_lock( kmp_lock_t *lck )
{
    ABT_mutex_attr mattr;

    ABT_mutex_attr_create( &mattr );
    ABT_mutex_attr_set_recursive( mattr, ABT_TRUE );
    ABT_mutex_create_with_attr( mattr, lck );
    ABT_mutex_attr_free( &mattr );
}

#ifdef KMP_ABT_USE_TASKLET_TEAM
int
__kmp_fork_join_tasklet_team(
    ident_t   * loc,
    int         gtid,
    enum fork_context_e  call_context, // Intel, GNU, ...
    kmp_int32   argc,
    microtask_t microtask,
    launch_t invoker,
/* TODO: revert workaround for Intel(R) 64 tracker #96 */
#if (KMP_ARCH_X86_64 || KMP_ARCH_ARM || KMP_ARCH_AARCH64) && KMP_OS_LINUX
    va_list   * ap
#else
    va_list     ap
#endif
    )
{
    void          **argv;
    int             i;
    int             master_tid;
    kmp_team_t     *parent_team;
    kmp_info_t     *master_th;
    int             nthreads;
    int             master_set_numthreads;
    int             level;

    kmp_team_t *team;
    int status;
    //printf("kmp_team_t=%lu\n", sizeof(kmp_team_t));

    KA_TRACE( 20, ("__kmp_fork_join_tasklet_team: enter T#%d\n", gtid ));

    /* initialize if needed */
    KMP_DEBUG_ASSERT( __kmp_global.init_serial ); // AC: potentially unsafe, not in sync with shutdown
    if( ! TCR_4(__kmp_global.init_parallel) )
        __kmp_parallel_initialize();

    /* setup current data */
    master_th     = __kmp_global.threads[ gtid ];
    parent_team   = master_th->th.th_team;
    master_tid    = master_th->th.th_info.ds.ds_tid;
    master_set_numthreads = master_th->th.th_set_nproc;

    level = parent_team->t.t_level + 1;

    /* setup the new team info */
    //kmp_team_t t_team;
    //team = &t_team;
    team = (kmp_team_t *)__kmp_allocate(sizeof(kmp_team_t));
    team->t.t_master_tid = master_tid;
    team->t.t_parent     = parent_team;

    team->t.t_argc = argc;
    team->t.t_argv = (void **)__kmp_allocate(sizeof(void *) * argc);
    argv = (void**)team->t.t_argv;
    for ( i=argc-1; i >= 0; --i ) {
        // TODO: revert workaround for Intel(R) 64 tracker #96
#if (KMP_ARCH_X86_64 || KMP_ARCH_ARM || KMP_ARCH_AARCH64) && KMP_OS_LINUX
        *argv++ = va_arg( *ap, void * );
#else
        *argv++ = va_arg( ap, void * );
#endif
    }

    nthreads = master_set_numthreads ? master_set_numthreads
             : get__nproc_2( parent_team, master_tid );
    team->t.t_nproc = nthreads;
    team->t.t_serialized = nthreads > 1 ? 0 : 1;

    TCW_SYNC_PTR(team->t.t_pkfn, microtask);
    team->t.t_invoke = invoker;

    team->t.t_dispatch = (kmp_disp_t *)__kmp_allocate(sizeof(kmp_disp_t) * nthreads);

    /* create tasklets */
    size_t size = sizeof(kmp_info_t) * nthreads;
    kmp_info_t *t_threads = (kmp_info_t *)__kmp_allocate(size);
    team->t.t_threads = &t_threads;

    //kmp_info_t *th = master_th; //&t_threads[0];
    kmp_info_t *th = &t_threads[0];
    th->th.th_team = team;
    th->th.th_info.ds.ds_tid = 0;
    th->th.th_info.ds.ds_gtid = gtid;
    th->th.th_dispatch = &team->t.t_dispatch[0];
    th->th.th_team_nproc = nthreads;

    for (i = 1; i < nthreads; i++) {
        th = &t_threads[i];
        th->th.th_team = team;
        th->th.th_info.ds.ds_tid = i;
        int my_gtid = gtid + i;
        th->th.th_info.ds.ds_gtid = my_gtid;
        th->th.th_dispatch = &team->t.t_dispatch[i];
        th->th.th_team_nproc = nthreads;

        ABT_pool tar_pool;
        if (level > 1) {
            tar_pool = __kmp_abt_get_my_pool(my_gtid);
        } else {
            tar_pool = __kmp_abt_get_pool(my_gtid);
        }
        status = ABT_task_create( tar_pool, __kmp_launch_tasklet_worker,
                                  (void *)th,
                                  &th->th.th_info.ds.ds_tasklet );
        KMP_ASSERT( status == ABT_SUCCESS );
    }

    /* make the master thread do the work */
    __kmp_set_self_info(&t_threads[0]);
    __kmp_launch_tasklet_worker((void *)&t_threads[0]);
    __kmp_set_self_info(master_th);

    /* restore the master_th information */
    master_th->th.th_set_nproc = 0;

    /* join tasklets */
    for (i = 1; i < nthreads; i++) {
        status = ABT_task_free(&t_threads[i].th.th_info.ds.ds_tasklet);
        KMP_ASSERT( status == ABT_SUCCESS );
    }

    __kmp_free(t_threads);
    __kmp_free(team->t.t_argv);
    __kmp_free(team->t.t_dispatch);
    __kmp_free(team);

    KA_TRACE( 20, ("__kmp_fork_join_tasklet_team: done T#%d\n", gtid ));

    return (nthreads > 1) ? TRUE : FALSE;
}
#endif

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void
__kmp_enable( int new_state )
{
    #ifdef KMP_CANCEL_THREADS
        //int status, old_state;
        //status = pthread_setcancelstate( new_state, & old_state );
        //KMP_CHECK_SYSFAIL( "pthread_setcancelstate", status );
        //KMP_DEBUG_ASSERT( old_state == PTHREAD_CANCEL_DISABLE );
    #endif
}

void
__kmp_disable( int * old_state )
{
    #ifdef KMP_CANCEL_THREADS
        //int status;
        //status = pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, old_state );
        //KMP_CHECK_SYSFAIL( "pthread_setcancelstate", status );
    #endif
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

#if (KMP_ARCH_X86 || KMP_ARCH_X86_64) && (! KMP_ASM_INTRINS)
/*
 * Only 32-bit "add-exchange" instruction on IA-32 architecture causes us to
 * use compare_and_store for these routines
 */

kmp_int8
__kmp_test_then_or8( volatile kmp_int8 *p, kmp_int8 d )
{
    kmp_int8 old_value, new_value;

    old_value = TCR_1( *p );
    new_value = old_value | d;

    while ( ! KMP_COMPARE_AND_STORE_REL8 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_1( *p );
        new_value = old_value | d;
    }
    return old_value;
}

kmp_int8
__kmp_test_then_and8( volatile kmp_int8 *p, kmp_int8 d )
{
    kmp_int8 old_value, new_value;

    old_value = TCR_1( *p );
    new_value = old_value & d;

    while ( ! KMP_COMPARE_AND_STORE_REL8 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_1( *p );
        new_value = old_value & d;
    }
    return old_value;
}

kmp_int32
__kmp_test_then_or32( volatile kmp_int32 *p, kmp_int32 d )
{
    kmp_int32 old_value, new_value;

    old_value = TCR_4( *p );
    new_value = old_value | d;

    while ( ! KMP_COMPARE_AND_STORE_REL32 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_4( *p );
        new_value = old_value | d;
    }
    return old_value;
}

kmp_int32
__kmp_test_then_and32( volatile kmp_int32 *p, kmp_int32 d )
{
    kmp_int32 old_value, new_value;

    old_value = TCR_4( *p );
    new_value = old_value & d;

    while ( ! KMP_COMPARE_AND_STORE_REL32 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_4( *p );
        new_value = old_value & d;
    }
    return old_value;
}

# if KMP_ARCH_X86 || KMP_ARCH_PPC64 || KMP_ARCH_AARCH64
kmp_int8
__kmp_test_then_add8( volatile kmp_int8 *p, kmp_int8 d )
{
    kmp_int8 old_value, new_value;

    old_value = TCR_1( *p );
    new_value = old_value + d;

    while ( ! KMP_COMPARE_AND_STORE_REL8 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_1( *p );
        new_value = old_value + d;
    }
    return old_value;
}

kmp_int64
__kmp_test_then_add64( volatile kmp_int64 *p, kmp_int64 d )
{
    kmp_int64 old_value, new_value;

    old_value = TCR_8( *p );
    new_value = old_value + d;

    while ( ! KMP_COMPARE_AND_STORE_REL64 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_8( *p );
        new_value = old_value + d;
    }
    return old_value;
}
# endif /* KMP_ARCH_X86 */

kmp_int64
__kmp_test_then_or64( volatile kmp_int64 *p, kmp_int64 d )
{
    kmp_int64 old_value, new_value;

    old_value = TCR_8( *p );
    new_value = old_value | d;
    while ( ! KMP_COMPARE_AND_STORE_REL64 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_8( *p );
        new_value = old_value | d;
    }
    return old_value;
}

kmp_int64
__kmp_test_then_and64( volatile kmp_int64 *p, kmp_int64 d )
{
    kmp_int64 old_value, new_value;

    old_value = TCR_8( *p );
    new_value = old_value & d;
    while ( ! KMP_COMPARE_AND_STORE_REL64 ( p, old_value, new_value ) )
    {
        KMP_CPU_PAUSE();
        old_value = TCR_8( *p );
        new_value = old_value & d;
    }
    return old_value;
}

#endif /* (KMP_ARCH_X86 || KMP_ARCH_X86_64) && (! KMP_ASM_INTRINS) */

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static void
__kmp_atfork_prepare (void)
{
    /*  nothing to do  */
}

static void
__kmp_atfork_parent (void)
{
    /*  nothing to do  */
}

/*
    Reset the library so execution in the child starts "all over again" with
    clean data structures in initial states.  Don't worry about freeing memory
    allocated by parent, just abandon it to be safe.
*/
static void
__kmp_atfork_child (void)
{
    /* TODO make sure this is done right for nested/sibling */
    // ATT:  Memory leaks are here? TODO: Check it and fix.
    /* KMP_ASSERT( 0 ); */

    ++__kmp_fork_count;

    __kmp_global.init_runtime = FALSE;
    __kmp_global.init_parallel = FALSE;
    __kmp_global.init_middle = FALSE;
    __kmp_global.init_serial = FALSE;
    TCW_4(__kmp_global.init_gtid, FALSE);
    __kmp_global.init_common = FALSE;

    __kmp_global.all_nth = 0;
    TCW_4(__kmp_global.nth, 0);

    /* Must actually zero all the *cache arguments passed to __kmpc_threadprivate here
       so threadprivate doesn't use stale data */
    KA_TRACE( 10, ( "__kmp_atfork_child: checking cache address list %p\n",
                 __kmp_global.threadpriv_cache_list ) );

    while ( __kmp_global.threadpriv_cache_list != NULL ) {

        if ( *__kmp_global.threadpriv_cache_list -> addr != NULL ) {
            KC_TRACE( 50, ( "__kmp_atfork_child: zeroing cache at address %p\n",
                        &(*__kmp_global.threadpriv_cache_list -> addr) ) );

            *__kmp_global.threadpriv_cache_list -> addr = NULL;
        }
        __kmp_global.threadpriv_cache_list = __kmp_global.threadpriv_cache_list -> next;
    }

    __kmp_global.init_runtime = FALSE;

    /* reset statically initialized locks */
    __kmp_init_bootstrap_lock( &__kmp_global.initz_lock );
    __kmp_init_bootstrap_lock( &__kmp_global.stdio_lock );

    /* This is necessary to make sure no stale data is left around */
    /* AC: customers complain that we use unsafe routines in the atfork
       handler. Mathworks: dlsym() is unsafe. We call dlsym and dlopen
       in dynamic_link when check the presence of shared tbbmalloc library.
       Suggestion is to make the library initialization lazier, similar
       to what done for __kmpc_begin(). */
    // TODO: synchronize all static initializations with regular library
    //       startup; look at kmp_global.c and etc.
    //__kmp_internal_begin ();

}

void
__kmp_register_atfork(void) {
    if ( __kmp_global.need_register_atfork ) {
        int status = pthread_atfork( __kmp_atfork_prepare, __kmp_atfork_parent, __kmp_atfork_child );
        KMP_CHECK_SYSFAIL( "pthread_atfork", status );
        __kmp_global.need_register_atfork = FALSE;
    }
}

void
__kmp_suspend_initialize( void )
{
}

static void
__kmp_suspend_initialize_thread( kmp_info_t *th )
{
///    if ( th->th.th_suspend_init_count <= __kmp_fork_count ) {
///        /* this means we haven't initialized the suspension ABT_thread objects for this thread
///           in this instance of the process */
///        int     status;
///        status = ABT_cond_create( &th->th.th_suspend_cv.c_cond );
///        KMP_CHECK_SYSFAIL( "ABT_cond_create", status );
///        status = ABT_mutex_create( &th->th.th_suspend_mx.m_mutex );
///        KMP_CHECK_SYSFAIL( "ABT_mutex_create", status );
///        *(volatile int*)&th->th.th_suspend_init_count = __kmp_fork_count + 1;
///    };
}

void
__kmp_suspend_uninitialize_thread( kmp_info_t *th )
{
///    if(th->th.th_suspend_init_count > __kmp_fork_count) {
///        /* this means we have initialize the suspension ABT_thread objects for this thread
///           in this instance of the process */
///        int status;
///
///        status = ABT_cond_free( &th->th.th_suspend_cv.c_cond );
///        if ( status != ABT_SUCCESS ) {
///            KMP_SYSFAIL( "ABT_cond_free", status );
///        };
///        status = ABT_mutex_free( &th->th.th_suspend_mx.m_mutex );
///        if ( status != ABT_SUCCESS ) {
///            KMP_SYSFAIL( "ABT_mutex_free", status );
///        };
///        --th->th.th_suspend_init_count;
///        KMP_DEBUG_ASSERT(th->th.th_suspend_init_count == __kmp_fork_count);
///    }
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

double
__kmp_read_cpu_time( void )
{
    /*clock_t   t;*/
    struct tms  buffer;

    /*t =*/  times( & buffer );

    return (buffer.tms_utime + buffer.tms_cutime) / (double) CLOCKS_PER_SEC;
}

int
__kmp_read_system_info( struct kmp_sys_info *info )
{
    int status;
    struct rusage r_usage;

    memset( info, 0, sizeof( *info ) );

    status = getrusage( RUSAGE_SELF, &r_usage);
    KMP_CHECK_SYSFAIL_ERRNO( "getrusage", status );

    info->maxrss  = r_usage.ru_maxrss;  /* the maximum resident set size utilized (in kilobytes)     */
    info->minflt  = r_usage.ru_minflt;  /* the number of page faults serviced without any I/O        */
    info->majflt  = r_usage.ru_majflt;  /* the number of page faults serviced that required I/O      */
    info->nswap   = r_usage.ru_nswap;   /* the number of times a process was "swapped" out of memory */
    info->inblock = r_usage.ru_inblock; /* the number of times the file system had to perform input  */
    info->oublock = r_usage.ru_oublock; /* the number of times the file system had to perform output */
    info->nvcsw   = r_usage.ru_nvcsw;   /* the number of times a context switch was voluntarily      */
    info->nivcsw  = r_usage.ru_nivcsw;  /* the number of times a context switch was forced           */

    return (status != 0);
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void
__kmp_read_system_time( double *delta )
{
    double              t_ns;
    struct timeval      tval;
    struct timespec     stop;
    int status;

    status = gettimeofday( &tval, NULL );
    KMP_CHECK_SYSFAIL_ERRNO( "gettimeofday", status );
    TIMEVAL_TO_TIMESPEC( &tval, &stop );
    t_ns = TS2NS(stop) - TS2NS(__kmp_sys_timer_data.start);
    *delta = (t_ns * 1e-9);
}

void
__kmp_clear_system_time( void )
{
    struct timeval tval;
    int status;
    status = gettimeofday( &tval, NULL );
    KMP_CHECK_SYSFAIL_ERRNO( "gettimeofday", status );
    TIMEVAL_TO_TIMESPEC( &tval, &__kmp_sys_timer_data.start );
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int
__kmp_read_from_file( char const *path, char const *format, ... )
{
    int result;
    va_list args;

    va_start(args, format);
    FILE *f = fopen(path, "rb");
    if ( f == NULL )
        return 0;
    result = vfscanf(f, format, args);
    fclose(f);

    return result;
}

/* Put the thread to sleep for a time period */
/* NOTE: not currently used anywhere */
void
__kmp_thread_sleep( int millis )
{
    sleep(  ( millis + 500 ) / 1000 );
}

/* Calculate the elapsed wall clock time for the user */
void
__kmp_elapsed( double *t )
{
    int status;
# ifdef FIX_SGI_CLOCK
    struct timespec ts;

    status = clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts );
    KMP_CHECK_SYSFAIL_ERRNO( "clock_gettime", status );
    *t = (double) ts.tv_nsec * (1.0 / (double) KMP_NSEC_PER_SEC) +
        (double) ts.tv_sec;
# else
    struct timeval tv;

    status = gettimeofday( & tv, NULL );
    KMP_CHECK_SYSFAIL_ERRNO( "gettimeofday", status );
    *t = (double) tv.tv_usec * (1.0 / (double) KMP_USEC_PER_SEC) +
        (double) tv.tv_sec;
# endif
}

/* Calculate the elapsed wall clock tick for the user */
void
__kmp_elapsed_tick( double *t )
{
    *t = 1 / (double) CLOCKS_PER_SEC;
}

/*
    Determine whether the given address is mapped into the current address space.
*/

int
__kmp_is_address_mapped( void * addr ) {

    int found = 0;
    int rc;

    #if KMP_OS_LINUX || KMP_OS_FREEBSD

        /*
            On Linux* OS, read the /proc/<pid>/maps pseudo-file to get all the address ranges mapped
            into the address space.
        */

        char * name = __kmp_str_format( "/proc/%d/maps", getpid() );
        FILE * file  = NULL;

        file = fopen( name, "r" );
        KMP_ASSERT( file != NULL );

        for ( ; ; ) {

            void * beginning = NULL;
            void * ending    = NULL;
            char   perms[ 5 ];

            rc = fscanf( file, "%p-%p %4s %*[^\n]\n", & beginning, & ending, perms );
            if ( rc == EOF ) {
                break;
            }; // if
            KMP_ASSERT( rc == 3 && KMP_STRLEN( perms ) == 4 ); // Make sure all fields are read.

            // Ending address is not included in the region, but beginning is.
            if ( ( addr >= beginning ) && ( addr < ending ) ) {
                perms[ 2 ] = 0;    // 3th and 4th character does not matter.
                if ( strcmp( perms, "rw" ) == 0 ) {
                    // Memory we are looking for should be readable and writable.
                    found = 1;
                }; // if
                break;
            }; // if

        }; // forever

        // Free resources.
        fclose( file );
        KMP_INTERNAL_FREE( name );

    #elif KMP_OS_DARWIN

        /*
            On OS X*, /proc pseudo filesystem is not available. Try to read memory using vm
            interface.
        */

        int       buffer;
        vm_size_t count;
        rc =
            vm_read_overwrite(
                mach_task_self(),           // Task to read memory of.
                (vm_address_t)( addr ),     // Address to read from.
                1,                          // Number of bytes to be read.
                (vm_address_t)( & buffer ), // Address of buffer to save read bytes in.
                & count                     // Address of var to save number of read bytes in.
            );
        if ( rc == 0 ) {
            // Memory successfully read.
            found = 1;
        }; // if

    #elif KMP_OS_FREEBSD || KMP_OS_NETBSD

        // FIXME(FreeBSD, NetBSD): Implement this
        found = 1;

    #else

        #error "Unknown or unsupported OS"

    #endif

    return found;

} // __kmp_is_address_mapped

#ifdef USE_LOAD_BALANCE


# if KMP_OS_DARWIN

// The function returns the rounded value of the system load average
// during given time interval which depends on the value of
// __kmp_global.load_balance_interval variable (default is 60 sec, other values
// may be 300 sec or 900 sec).
// It returns -1 in case of error.
int
__kmp_get_load_balance( int max )
{
    double averages[3];
    int ret_avg = 0;

    int res = getloadavg( averages, 3 );

    //Check __kmp_global.load_balance_interval to determine which of averages to use.
    // getloadavg() may return the number of samples less than requested that is
    // less than 3.
    if ( __kmp_global.load_balance_interval < 180 && ( res >= 1 ) ) {
        ret_avg = averages[0];// 1 min
    } else if ( ( __kmp_global.load_balance_interval >= 180
                  && __kmp_global.load_balance_interval < 600 ) && ( res >= 2 ) ) {
        ret_avg = averages[1];// 5 min
    } else if ( ( __kmp_global.load_balance_interval >= 600 ) && ( res == 3 ) ) {
        ret_avg = averages[2];// 15 min
    } else {// Error occurred
        return -1;
    }

    return ret_avg;
}

# else // Linux* OS

// The fuction returns number of running (not sleeping) threads, or -1 in case of error.
// Error could be reported if Linux* OS kernel too old (without "/proc" support).
// Counting running threads stops if max running threads encountered.
int
__kmp_get_load_balance( int max )
{
    static int permanent_error = 0;

    static int     glb_running_threads          = 0;  /* Saved count of the running threads for the thread balance algortihm */
    static double  glb_call_time = 0;  /* Thread balance algorithm call time */

    int running_threads = 0;              // Number of running threads in the system.

    DIR  *          proc_dir   = NULL;    // Handle of "/proc/" directory.
    struct dirent * proc_entry = NULL;

    kmp_str_buf_t   task_path;            // "/proc/<pid>/task/<tid>/" path.
    DIR  *          task_dir   = NULL;    // Handle of "/proc/<pid>/task/<tid>/" directory.
    struct dirent * task_entry = NULL;
    int             task_path_fixed_len;

    kmp_str_buf_t   stat_path;            // "/proc/<pid>/task/<tid>/stat" path.
    int             stat_file = -1;
    int             stat_path_fixed_len;

    int total_processes = 0;              // Total number of processes in system.
    int total_threads   = 0;              // Total number of threads in system.

    double call_time = 0.0;

    __kmp_str_buf_init( & task_path );
    __kmp_str_buf_init( & stat_path );

     __kmp_elapsed( & call_time );

    if ( glb_call_time &&
            ( call_time - glb_call_time < __kmp_global.load_balance_interval ) ) {
        running_threads = glb_running_threads;
        goto finish;
    }

    glb_call_time = call_time;

    // Do not spend time on scanning "/proc/" if we have a permanent error.
    if ( permanent_error ) {
        running_threads = -1;
        goto finish;
    }; // if

    if ( max <= 0 ) {
        max = INT_MAX;
    }; // if

    // Open "/proc/" directory.
    proc_dir = opendir( "/proc" );
    if ( proc_dir == NULL ) {
        // Cannot open "/prroc/". Probably the kernel does not support it. Return an error now and
        // in subsequent calls.
        running_threads = -1;
        permanent_error = 1;
        goto finish;
    }; // if

    // Initialize fixed part of task_path. This part will not change.
    __kmp_str_buf_cat( & task_path, "/proc/", 6 );
    task_path_fixed_len = task_path.used;    // Remember number of used characters.

    proc_entry = readdir( proc_dir );
    while ( proc_entry != NULL ) {
        // Proc entry is a directory and name starts with a digit. Assume it is a process'
        // directory.
        if ( proc_entry->d_type == DT_DIR && isdigit( proc_entry->d_name[ 0 ] ) ) {

            ++ total_processes;
            // Make sure init process is the very first in "/proc", so we can replace
            // strcmp( proc_entry->d_name, "1" ) == 0 with simpler total_processes == 1.
            // We are going to check that total_processes == 1 => d_name == "1" is true (where
            // "=>" is implication). Since C++ does not have => operator, let us replace it with its
            // equivalent: a => b == ! a || b.
            KMP_DEBUG_ASSERT( total_processes != 1 || strcmp( proc_entry->d_name, "1" ) == 0 );

            // Construct task_path.
            task_path.used = task_path_fixed_len;    // Reset task_path to "/proc/".
            __kmp_str_buf_cat( & task_path, proc_entry->d_name, KMP_STRLEN( proc_entry->d_name ) );
            __kmp_str_buf_cat( & task_path, "/task", 5 );

            task_dir = opendir( task_path.str );
            if ( task_dir == NULL ) {
                // Process can finish between reading "/proc/" directory entry and opening process'
                // "task/" directory. So, in general case we should not complain, but have to skip
                // this process and read the next one.
                // But on systems with no "task/" support we will spend lot of time to scan "/proc/"
                // tree again and again without any benefit. "init" process (its pid is 1) should
                // exist always, so, if we cannot open "/proc/1/task/" directory, it means "task/"
                // is not supported by kernel. Report an error now and in the future.
                if ( strcmp( proc_entry->d_name, "1" ) == 0 ) {
                    running_threads = -1;
                    permanent_error = 1;
                    goto finish;
                }; // if
            } else {
                 // Construct fixed part of stat file path.
                __kmp_str_buf_clear( & stat_path );
                __kmp_str_buf_cat( & stat_path, task_path.str, task_path.used );
                __kmp_str_buf_cat( & stat_path, "/", 1 );
                stat_path_fixed_len = stat_path.used;

                task_entry = readdir( task_dir );
                while ( task_entry != NULL ) {
                    // It is a directory and name starts with a digit.
                    if ( proc_entry->d_type == DT_DIR && isdigit( task_entry->d_name[ 0 ] ) ) {

                        ++ total_threads;

                        // Consruct complete stat file path. Easiest way would be:
                        //  __kmp_str_buf_print( & stat_path, "%s/%s/stat", task_path.str, task_entry->d_name );
                        // but seriae of __kmp_str_buf_cat works a bit faster.
                        stat_path.used = stat_path_fixed_len;    // Reset stat path to its fixed part.
                        __kmp_str_buf_cat( & stat_path, task_entry->d_name, KMP_STRLEN( task_entry->d_name ) );
                        __kmp_str_buf_cat( & stat_path, "/stat", 5 );

                        // Note: Low-level API (open/read/close) is used. High-level API
                        // (fopen/fclose)  works ~ 30 % slower.
                        stat_file = open( stat_path.str, O_RDONLY );
                        if ( stat_file == -1 ) {
                            // We cannot report an error because task (thread) can terminate just
                            // before reading this file.
                        } else {
                            /*
                                Content of "stat" file looks like:

                                    24285 (program) S ...

                                It is a single line (if program name does not include fanny
                                symbols). First number is a thread id, then name of executable file
                                name in paretheses, then state of the thread. We need just thread
                                state.

                                Good news: Length of program name is 15 characters max. Longer
                                names are truncated.

                                Thus, we need rather short buffer: 15 chars for program name +
                                2 parenthesis, + 3 spaces + ~7 digits of pid = 37.

                                Bad news: Program name may contain special symbols like space,
                                closing parenthesis, or even new line. This makes parsing "stat"
                                file not 100 % reliable. In case of fanny program names parsing
                                may fail (report incorrect thread state).

                                Parsing "status" file looks more promissing (due to different
                                file structure and escaping special symbols) but reading and
                                parsing of "status" file works slower.

                                -- ln
                            */
                            char buffer[ 65 ];
                            int len;
                            len = read( stat_file, buffer, sizeof( buffer ) - 1 );
                            if ( len >= 0 ) {
                                buffer[ len ] = 0;
                                // Using scanf:
                                //     sscanf( buffer, "%*d (%*s) %c ", & state );
                                // looks very nice, but searching for a closing parenthesis works a
                                // bit faster.
                                char * close_parent = strstr( buffer, ") " );
                                if ( close_parent != NULL ) {
                                    char state = * ( close_parent + 2 );
                                    if ( state == 'R' ) {
                                        ++ running_threads;
                                        if ( running_threads >= max ) {
                                            goto finish;
                                        }; // if
                                    }; // if
                                }; // if
                            }; // if
                            close( stat_file );
                            stat_file = -1;
                        }; // if
                    }; // if
                    task_entry = readdir( task_dir );
                }; // while
                closedir( task_dir );
                task_dir = NULL;
            }; // if
        }; // if
        proc_entry = readdir( proc_dir );
    }; // while

    //
    // There _might_ be a timing hole where the thread executing this
    // code get skipped in the load balance, and running_threads is 0.
    // Assert in the debug builds only!!!
    //
    KMP_DEBUG_ASSERT( running_threads > 0 );
    if ( running_threads <= 0 ) {
        running_threads = 1;
    }

    finish: // Clean up and exit.
        if ( proc_dir != NULL ) {
            closedir( proc_dir );
        }; // if
        __kmp_str_buf_free( & task_path );
        if ( task_dir != NULL ) {
            closedir( task_dir );
        }; // if
        __kmp_str_buf_free( & stat_path );
        if ( stat_file != -1 ) {
            close( stat_file );
        }; // if

    glb_running_threads = running_threads;

    return running_threads;

} // __kmp_get_load_balance

# endif // KMP_OS_DARWIN

#endif // USE_LOAD_BALANCE

#if !(KMP_ARCH_X86 || KMP_ARCH_X86_64 || KMP_MIC)

// we really only need the case with 1 argument, because CLANG always build
// a struct of pointers to shared variables referenced in the outlined function
int
__kmp_invoke_microtask( microtask_t pkfn,
                        int gtid, int tid,
                        int argc, void *p_argv[] 
) 
{

  switch (argc) {
  default:
    fprintf(stderr, "Too many args to microtask: %d!\n", argc);
    fflush(stderr);
    exit(-1);
  case 0:
    (*pkfn)(&gtid, &tid);
    break;
  case 1:
    (*pkfn)(&gtid, &tid, p_argv[0]);
    break;
  case 2:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1]);
    break;
  case 3:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2]);
    break;
  case 4:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3]);
    break;
  case 5:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4]);
    break;
  case 6:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5]);
    break;
  case 7:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6]);
    break;
  case 8:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7]);
    break;
  case 9:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8]);
    break;
  case 10:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9]);
    break;
  case 11:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9], p_argv[10]);
    break;
  case 12:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9], p_argv[10],
            p_argv[11]);
    break;
  case 13:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9], p_argv[10],
            p_argv[11], p_argv[12]);
    break;
  case 14:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9], p_argv[10],
            p_argv[11], p_argv[12], p_argv[13]);
    break;
  case 15:
    (*pkfn)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4],
            p_argv[5], p_argv[6], p_argv[7], p_argv[8], p_argv[9], p_argv[10],
            p_argv[11], p_argv[12], p_argv[13], p_argv[14]);
    break;
  }

  return 1;
}

#endif

// end of file //


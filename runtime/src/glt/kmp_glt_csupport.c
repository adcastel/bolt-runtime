/*
 * kmp_abt_csupport.c -- kfront linkage support for OpenMP.
 */


//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//


#include "omp.h"        /* extern "C" declarations of user-visible routines */
#include "kmp_glt.h"
#include "kmp_glt_i18n.h"
#include "kmp_glt_error.h"
#include "kmp_glt_stats.h"

#define MAX_MESSAGE 512

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/*  flags will be used in future, e.g., to implement */
/*  openmp_strict library restrictions               */

/*!
 * @ingroup STARTUP_SHUTDOWN
 * @param loc   in   source location information
 * @param flags in   for future use (currently ignored)
 *
 * Initialize the runtime library. This call is optional; if it is not made then
 * it will be implicitly called by attempts to use other library functions.
 *
 */
void
__kmpc_begin(ident_t *loc, kmp_int32 flags)
{
    // By default __kmp_ignore_mppbeg() returns TRUE.
    if (__kmp_ignore_mppbeg() == FALSE) {
        __kmp_internal_begin();

        KC_TRACE( 10, ("__kmpc_begin: called\n" ) );
    }
}

/*!
 * @ingroup STARTUP_SHUTDOWN
 * @param loc source location information
 *
 * Shutdown the runtime library. This is also optional, and even if called will not
 * do anything unless the `KMP_IGNORE_MPPEND` environment variable is set to zero.
  */
void
__kmpc_end(ident_t *loc)
{
    // By default, __kmp_ignore_mppend() returns TRUE which makes __kmpc_end() call no-op.
    // However, this can be overridden with KMP_IGNORE_MPPEND environment variable.
    // If KMP_IGNORE_MPPEND is 0, __kmp_ignore_mppend() returns FALSE and __kmpc_end()
    // will unregister this root (it can cause library shut down).
    if (__kmp_ignore_mppend() == FALSE) {
        KC_TRACE( 10, ("__kmpc_end: called\n" ) );
        KA_TRACE( 30, ("__kmpc_end\n" ));

        __kmp_internal_end_thread( -1 );
    }
}

/*!
@ingroup THREAD_STATES
@param loc Source location information.
@return The global thread index of the active thread.

This function can be called in any context.

If the runtime has ony been entered at the outermost level from a
single (necessarily non-OpenMP<sup>*</sup>) thread, then the thread number is that
which would be returned by omp_get_thread_num() in the outermost
active parallel construct. (Or zero if there is no active parallel
construct, since the master thread is necessarily thread zero).

If multiple non-OpenMP threads all enter an OpenMP construct then this
will be a unique thread identifier among all the threads created by
the OpenMP runtime (but the value cannote be defined in terms of
OpenMP thread ids returned by omp_get_thread_num()).

*/
kmp_int32
__kmpc_global_thread_num(ident_t *loc)
{
    kmp_int32 gtid = __kmp_entry_gtid();

    KC_TRACE( 10, ("__kmpc_global_thread_num: T#%d\n", gtid ) );

    return gtid;
}

/*!
@ingroup THREAD_STATES
@param loc Source location information.
@return The number of threads under control of the OpenMP<sup>*</sup> runtime

This function can be called in any context.
It returns the total number of threads under the control of the OpenMP runtime. That is
not a number that can be determined by any OpenMP standard calls, since the library may be
called from more than one non-OpenMP thread, and this reflects the total over all such calls.
Similarly the runtime maintains underlying threads even when they are not active (since the cost
of creating and destroying OS threads is high), this call counts all such threads even if they are not
waiting for work.
*/
kmp_int32
__kmpc_global_num_threads(ident_t *loc)
{
    KC_TRACE( 10, ("__kmpc_global_num_threads: num_threads = %d\n", __kmp_global.nth ) );

    return TCR_4(__kmp_global.nth);
}

/*!
@ingroup THREAD_STATES
@param loc Source location information.
@return The thread number of the calling thread in the innermost active parallel construct.

*/
kmp_int32
__kmpc_bound_thread_num(ident_t *loc)
{
    KC_TRACE( 10, ("__kmpc_bound_thread_num: called\n" ) );
    return __kmp_tid_from_gtid( __kmp_entry_gtid() );
}

/*!
@ingroup THREAD_STATES
@param loc Source location information.
@return The number of threads in the innermost active parallel construct.
*/
kmp_int32
__kmpc_bound_num_threads(ident_t *loc)
{
    KC_TRACE( 10, ("__kmpc_bound_num_threads: called\n" ) );

    return __kmp_entry_thread() -> th.th_team -> t.t_nproc;
}

/*!
 * @ingroup DEPRECATED
 * @param loc location description
 *
 * This function need not be called. It always returns TRUE.
 */
kmp_int32
__kmpc_ok_to_fork(ident_t *loc)
{
#ifndef KMP_DEBUG

    return TRUE;

#else

    const char *semi2;
    const char *semi3;
    int line_no;

    if (__kmp_par_range == 0) {
        return TRUE;
    }
    semi2 = loc->psource;
    if (semi2 == NULL) {
        return TRUE;
    }
    semi2 = strchr(semi2, ';');
    if (semi2 == NULL) {
        return TRUE;
    }
    semi2 = strchr(semi2 + 1, ';');
    if (semi2 == NULL) {
        return TRUE;
    }
    if (__kmp_par_range_filename[0]) {
        const char *name = semi2 - 1;
        while ((name > loc->psource) && (*name != '/') && (*name != ';')) {
            name--;
        }
        if ((*name == '/') || (*name == ';')) {
            name++;
        }
        if (strncmp(__kmp_par_range_filename, name, semi2 - name)) {
            return __kmp_par_range < 0;
        }
    }
    semi3 = strchr(semi2 + 1, ';');
    if (__kmp_par_range_routine[0]) {
        if ((semi3 != NULL) && (semi3 > semi2)
          && (strncmp(__kmp_par_range_routine, semi2 + 1, semi3 - semi2 - 1))) {
            return __kmp_par_range < 0;
        }
    }
    if (KMP_SSCANF(semi3 + 1, "%d", &line_no) == 1) {
        if ((line_no >= __kmp_par_range_lb) && (line_no <= __kmp_par_range_ub)) {
            return __kmp_par_range > 0;
        }
        return __kmp_par_range < 0;
    }
    return TRUE;

#endif /* KMP_DEBUG */

}

/*!
@ingroup THREAD_STATES
@param loc Source location information.
@return 1 if this thread is executing inside an active parallel region, zero if not.
*/
kmp_int32
__kmpc_in_parallel( ident_t *loc )
{
    return __kmp_entry_thread() -> th.th_root -> r.r_active;
}

/*!
@ingroup PARALLEL
@param loc source location information
@param global_tid global thread number
@param num_threads number of threads requested for this parallel construct

Set the number of threads to be used by the next fork spawned by this thread.
This call is only required if the parallel construct has a `num_threads` clause.
*/
void
__kmpc_push_num_threads(ident_t *loc, kmp_int32 global_tid, kmp_int32 num_threads )
{
    KA_TRACE( 20, ("__kmpc_push_num_threads: enter T#%d num_threads=%d\n",
      global_tid, num_threads ) );

    __kmp_push_num_threads( loc, global_tid, num_threads );
}

void
__kmpc_pop_num_threads(ident_t *loc, kmp_int32 global_tid )
{
    KA_TRACE( 20, ("__kmpc_pop_num_threads: enter\n" ) );

    /* the num_threads are automatically popped */
}


#if OMP_40_ENABLED

void
__kmpc_push_proc_bind(ident_t *loc, kmp_int32 global_tid, kmp_int32 proc_bind )
{
    KA_TRACE( 20, ("__kmpc_push_proc_bind: enter T#%d proc_bind=%d\n",
      global_tid, proc_bind ) );

    __kmp_push_proc_bind( loc, global_tid, (kmp_proc_bind_t)proc_bind );
}

#endif /* OMP_40_ENABLED */


/*!
@ingroup PARALLEL
@param loc  source location information
@param argc  total number of arguments in the ellipsis
@param microtask  pointer to callback routine consisting of outlined parallel construct
@param ...  pointers to shared variables that aren't global

Do the actual fork and call the microtask in the relevant number of threads.
*/
void
__kmpc_fork_call(ident_t *loc, kmp_int32 argc, kmpc_micro microtask, ...)
{
  int         gtid = __kmp_entry_gtid();

  // maybe to save thr_state is enough here
  {
    va_list     ap;
    va_start(   ap, microtask );

#ifdef KMP_GLT_USE_TASKLET_TEAM
    kmp_info_t *this_thr = __kmp_global.threads[ gtid ];
    if (get__tasklet(this_thr)) {
        set__tasklet(this_thr,FTN_FALSE);
        __kmp_fork_join_tasklet_team( loc, gtid, fork_context_intel,
                argc,
                VOLATILE_CAST(microtask_t) microtask, // "wrapped" task
                VOLATILE_CAST(launch_t)    __kmp_invoke_task_func,
    /* TODO: revert workaround for Intel(R) 64 tracker #96 */
#if (KMP_ARCH_X86_64 || KMP_ARCH_ARM || KMP_ARCH_AARCH64) && KMP_OS_LINUX
                &ap
#else
                ap
#endif
                );
    } else {
#endif

#if INCLUDE_SSC_MARKS
    SSC_MARK_FORKING();
#endif
    __kmp_fork_call( loc, gtid, fork_context_intel,
            argc,
            VOLATILE_CAST(microtask_t) microtask, // "wrapped" task
            VOLATILE_CAST(launch_t)    __kmp_invoke_task_func,
/* TODO: revert workaround for Intel(R) 64 tracker #96 */
#if (KMP_ARCH_X86_64 || KMP_ARCH_ARM || KMP_ARCH_AARCH64) && KMP_OS_LINUX
            &ap
#else
            ap
#endif
            );
#if INCLUDE_SSC_MARKS
    SSC_MARK_JOINING();
#endif

    __kmp_join_call( loc, gtid
    );
#ifdef KMP_GLT_USE_TASKLET_TEAM
    }
#endif

    va_end( ap );
  }

}

#if OMP_40_ENABLED
/*!
@ingroup PARALLEL
@param loc source location information
@param global_tid global thread number
@param num_teams number of teams requested for the teams construct
@param num_threads number of threads per team requested for the teams construct

Set the number of teams to be used by the teams construct.
This call is only required if the teams construct has a `num_teams` clause
or a `thread_limit` clause (or both).
*/
void
__kmpc_push_num_teams(ident_t *loc, kmp_int32 global_tid, kmp_int32 num_teams, kmp_int32 num_threads )
{
    KA_TRACE( 20, ("__kmpc_push_num_teams: enter T#%d num_teams=%d num_threads=%d\n",
      global_tid, num_teams, num_threads ) );

    __kmp_push_num_teams( loc, global_tid, num_teams, num_threads );
}

/*!
@ingroup PARALLEL
@param loc  source location information
@param argc  total number of arguments in the ellipsis
@param microtask  pointer to callback routine consisting of outlined teams construct
@param ...  pointers to shared variables that aren't global

Do the actual fork and call the microtask in the relevant number of threads.
*/
void
__kmpc_fork_teams(ident_t *loc, kmp_int32 argc, kmpc_micro microtask, ...)
{
    int         gtid = __kmp_entry_gtid();
    kmp_info_t *this_thr = __kmp_global.threads[ gtid ];
    va_list     ap;
    va_start(   ap, microtask );

    KMP_COUNT_BLOCK(OMP_TEAMS);

    // remember teams entry point and nesting level
    this_thr->th.th_teams_microtask = microtask;
    this_thr->th.th_teams_level = this_thr->th.th_team->t.t_level; // AC: can be >0 on host

    // check if __kmpc_push_num_teams called, set default number of teams otherwise
    if ( this_thr->th.th_teams_size.nteams == 0 ) {
        __kmp_push_num_teams( loc, gtid, 0, 0 );
    }
    KMP_DEBUG_ASSERT(this_thr->th.th_set_nproc >= 1);
    KMP_DEBUG_ASSERT(this_thr->th.th_teams_size.nteams >= 1);
    KMP_DEBUG_ASSERT(this_thr->th.th_teams_size.nth >= 1);

    __kmp_fork_call( loc, gtid, fork_context_intel,
            argc,
            VOLATILE_CAST(microtask_t) __kmp_teams_master, // "wrapped" task
            VOLATILE_CAST(launch_t)    __kmp_invoke_teams_master,
#if (KMP_ARCH_X86_64 || KMP_ARCH_ARM || KMP_ARCH_AARCH64) && KMP_OS_LINUX
            &ap
#else
            ap
#endif
            );

    __kmp_join_call( loc, gtid
    );

    this_thr->th.th_teams_microtask = NULL;
    this_thr->th.th_teams_level = 0;
    *(kmp_int64*)(&this_thr->th.th_teams_size) = 0L;
    va_end( ap );
}
#endif /* OMP_40_ENABLED */


//
// I don't think this function should ever have been exported.
// The __kmpc_ prefix was misapplied.  I'm fairly certain that no generated
// openmp code ever called it, but it's been exported from the RTL for so
// long that I'm afraid to remove the definition.
//
int
__kmpc_invoke_task_func( int gtid )
{
    return __kmp_invoke_task_func( gtid );
}

/*!
@ingroup PARALLEL
@param loc  source location information
@param global_tid  global thread number

Enter a serialized parallel construct. This interface is used to handle a
conditional parallel region, like this,
@code
#pragma omp parallel if (condition)
@endcode
when the condition is false.
*/
void
__kmpc_serialized_parallel(ident_t *loc, kmp_int32 global_tid)
{
    __kmp_serialized_parallel(loc, global_tid); /* The implementation is now in kmp_runtime.c so that it can share static functions with
                                                 * kmp_fork_call since the tasks to be done are similar in each case.
                                                 */
}

/*!
@ingroup PARALLEL
@param loc  source location information
@param global_tid  global thread number

Leave a serialized parallel construct.
*/
void
__kmpc_end_serialized_parallel(ident_t *loc, kmp_int32 global_tid)
{
    kmp_internal_control_t *top;
    kmp_info_t *this_thr;
    kmp_team_t *serial_team;

    KC_TRACE( 10, ("__kmpc_end_serialized_parallel: called by T#%d\n", global_tid ) );

    /* skip all this code for autopar serialized loops since it results in
       unacceptable overhead */
    if( loc != NULL && (loc->flags & KMP_IDENT_AUTOPAR ) )
        return;

    // Not autopar code
    if( ! TCR_4( __kmp_global.init_parallel ) )
        __kmp_parallel_initialize();

    this_thr    = __kmp_global.threads[ global_tid ];
    serial_team = this_thr->th.th_serial_team;

   #if OMP_41_ENABLED
   kmp_task_team_t *   task_team = this_thr->th.th_task_team;

   // we need to wait for the proxy tasks before finishing the thread
   if ( task_team != NULL && task_team->tt.tt_found_proxy_tasks )
        __kmp_task_team_wait(this_thr, serial_team, 1);
   #endif

    KMP_MB();
    KMP_DEBUG_ASSERT( serial_team );
    KMP_ASSERT(       serial_team -> t.t_serialized );
    KMP_DEBUG_ASSERT( this_thr -> th.th_team == serial_team );
    KMP_DEBUG_ASSERT( serial_team != this_thr->th.th_root->r.r_root_team );
    KMP_DEBUG_ASSERT( serial_team -> t.t_threads );
    KMP_DEBUG_ASSERT( serial_team -> t.t_threads[0] == this_thr );

    /* If necessary, pop the internal control stack values and replace the team values */
    top = serial_team -> t.t_control_stack_top;
    if ( top && top -> serial_nesting_level == serial_team -> t.t_serialized ) {
        copy_icvs( &serial_team -> t.t_threads[0] -> th.th_current_task -> td_icvs, top );
        serial_team -> t.t_control_stack_top = top -> next;
        __kmp_free(top);
    }

    //if( serial_team -> t.t_serialized > 1 )
    serial_team -> t.t_level--;

    /* pop dispatch buffers stack */
    KMP_DEBUG_ASSERT(serial_team->t.t_dispatch->th_disp_buffer);
    {
        dispatch_private_info_t * disp_buffer = serial_team->t.t_dispatch->th_disp_buffer;
        serial_team->t.t_dispatch->th_disp_buffer =
            serial_team->t.t_dispatch->th_disp_buffer->next;
        __kmp_free( disp_buffer );
    }

    -- serial_team -> t.t_serialized;
    if ( serial_team -> t.t_serialized == 0 ) {

        /* return to the parallel section */

#if KMP_ARCH_X86 || KMP_ARCH_X86_64
        if ( __kmp_global.inherit_fp_control && serial_team->t.t_fp_control_saved ) {
            __kmp_clear_x87_fpu_status_word();
            __kmp_load_x87_fpu_control_word( &serial_team->t.t_x87_fpu_control_word );
            __kmp_load_mxcsr( &serial_team->t.t_mxcsr );
        }
#endif /* KMP_ARCH_X86 || KMP_ARCH_X86_64 */

        this_thr -> th.th_team           = serial_team -> t.t_parent;
        this_thr -> th.th_info.ds.ds_tid = serial_team -> t.t_master_tid;

        /* restore values cached in the thread */
        this_thr -> th.th_team_nproc     = serial_team -> t.t_parent -> t.t_nproc;          /*  JPH */
        this_thr -> th.th_team_master    = serial_team -> t.t_parent -> t.t_threads[0];     /* JPH */
        this_thr -> th.th_team_serialized = this_thr -> th.th_team -> t.t_serialized;

        /* TODO the below shouldn't need to be adjusted for serialized teams */
        this_thr -> th.th_dispatch       = & this_thr -> th.th_team ->
            t.t_dispatch[ serial_team -> t.t_master_tid ];

        __kmp_pop_current_task_from_thread( this_thr );

        KMP_ASSERT( this_thr -> th.th_current_task -> td_flags.executing == 0 );
        this_thr -> th.th_current_task -> td_flags.executing = 1;

        if ( __kmp_global.tasking_mode != tskm_immediate_exec ) {
            // Copy the task team from the new child / old parent team to the thread.
            this_thr->th.th_task_team = this_thr->th.th_team->t.t_task_team[this_thr->th.th_task_state];
            KA_TRACE( 20, ( "__kmpc_end_serialized_parallel: T#%d restoring task_team %p / team %p\n",
                            global_tid, this_thr -> th.th_task_team, this_thr -> th.th_team ) );
        }
    } else {
        if ( __kmp_global.tasking_mode != tskm_immediate_exec ) {
            KA_TRACE( 20, ( "__kmpc_end_serialized_parallel: T#%d decreasing nesting depth of serial team %p to %d\n",
                            global_tid, serial_team, serial_team -> t.t_serialized ) );
        }
    }

    if ( __kmp_global.env_consistency_check )
        __kmp_pop_parallel( global_tid, NULL );
}

/*!
@ingroup SYNCHRONIZATION
@param loc  source location information.

Execute <tt>flush</tt>. This is implemented as a full memory fence. (Though
depending on the memory ordering convention obeyed by the compiler
even that may not be necessary).
*/
void
__kmpc_flush(ident_t *loc)
{
    KC_TRACE( 10, ("__kmpc_flush: called\n" ) );

    /* need explicit __mf() here since use volatile instead in library */
    KMP_MB();       /* Flush all pending memory write invalidates.  */

    #if ( KMP_ARCH_X86 || KMP_ARCH_X86_64 )
        #if KMP_MIC
            // fence-style instructions do not exist, but lock; xaddl $0,(%rsp) can be used.
            // We shouldn't need it, though, since the ABI rules require that
            // * If the compiler generates NGO stores it also generates the fence
            // * If users hand-code NGO stores they should insert the fence
            // therefore no incomplete unordered stores should be visible.
        #else
            // C74404
            // This is to address non-temporal store instructions (sfence needed).
            // The clflush instruction is addressed either (mfence needed).
            // Probably the non-temporal load monvtdqa instruction should also be addressed.
            // mfence is a SSE2 instruction. Do not execute it if CPU is not SSE2.
            if ( ! __kmp_global.cpuinfo.initialized ) {
                __kmp_query_cpuid( & __kmp_global.cpuinfo );
            }; // if
            if ( ! __kmp_global.cpuinfo.sse2 ) {
                // CPU cannot execute SSE2 instructions.
            } else {
                #if KMP_COMPILER_ICC || KMP_COMPILER_MSVC
                _mm_mfence();
                #else
                __sync_synchronize();
                #endif // KMP_COMPILER_ICC
            }; // if
        #endif // KMP_MIC
    #elif (KMP_ARCH_ARM || KMP_ARCH_AARCH64)
        // Nothing to see here move along
    #elif KMP_ARCH_PPC64
        // Nothing needed here (we have a real MB above).
        #if KMP_OS_CNK
        // The flushing thread needs to yield here; this prevents a
       // busy-waiting thread from saturating the pipeline. flush is
          // often used in loops like this:
           // while (!flag) {
           //   #pragma omp flush(flag)
           // }
       // and adding the yield here is good for at least a 10x speedup
          // when running >2 threads per core (on the NAS LU benchmark).
            __kmp_yield(TRUE);
        #endif
    #else
        #error Unknown or unsupported architecture
    #endif

}

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
static __forceinline int
__kmp_global_get_crit_lock_id( kmp_critical_name *crit ) {
    unsigned id = *(unsigned *)crit;
    return ((id & (id >> 16)) % KMP_NUM_CRIT_LOCKS);
}

static __forceinline void
__kmp_global_enter_critical_section( kmp_int32 global_tid, kmp_critical_name * crit ) {

    int lock_id = __kmp_global_get_crit_lock_id( crit );
    __kmp_acquire_lock( &__kmp_global.crit_lock[lock_id], global_tid );
    KA_TRACE( 20, ( "__kmp_global_enter_critical_section(): T#%d: acquired lock#%d\n",
                    global_tid, lock_id ) );

}

static __forceinline void
__kmp_global_end_critical_section( kmp_int32 global_tid, kmp_critical_name * crit ) {

    int lock_id = __kmp_global_get_crit_lock_id( crit );
    __kmp_release_lock( &__kmp_global.crit_lock[lock_id], global_tid );
    KA_TRACE( 20, ( "__kmp_global_end_critical_section(): T#%d: released lock#%d\n",
                    global_tid, lock_id ) );

}

static __forceinline int
__kmp_team_get_lock_id( kmp_critical_name *crit ) {
    unsigned id = *(unsigned *)crit;
    return ((id & (id >> 16)) % KMP_TEAM_NUM_LOCKS);
}

static __forceinline void
__kmp_team_enter_critical_section( kmp_int32 global_tid, kmp_critical_name * crit ) {

    int lock_id = __kmp_team_get_lock_id( crit );
    kmp_team_t *team = __kmp_team_from_gtid( global_tid );
    __kmp_acquire_lock( &team->t.t_lock[lock_id], global_tid );
    KA_TRACE( 20, ( "__kmp_team_enter_critical_section(): team %d (T#%d): acquired lock#%d\n",
                    team->t.t_id, global_tid, lock_id ) );

}

static __forceinline void
__kmp_team_end_critical_section( kmp_int32 global_tid, kmp_critical_name * crit ) {

    int lock_id = __kmp_team_get_lock_id( crit );
    kmp_team_t *team = __kmp_team_from_gtid( global_tid );
    __kmp_release_lock( &team->t.t_lock[lock_id], global_tid );
    KA_TRACE( 20, ( "__kmp_team_end_critical_section(): team %d (T#%d): released lock#%d\n",
                    team->t.t_id, global_tid, lock_id ) );

}
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid thread id.

Execute a barrier.
*/
void
__kmpc_barrier(ident_t *loc, kmp_int32 global_tid)
{
    KMP_COUNT_BLOCK(OMP_BARRIER);
    KMP_TIME_BLOCK(OMP_barrier);
    KC_TRACE( 10, ("__kmpc_barrier: called T#%d\n", global_tid ) );

    if (! TCR_4(__kmp_global.init_parallel))
        __kmp_parallel_initialize();

    if ( __kmp_global.env_consistency_check ) {
        if ( loc == 0 ) {
            KMP_WARNING( ConstructIdentInvalid ); // ??? What does it mean for the user?
        }; // if

        __kmp_check_barrier( global_tid, ct_barrier, loc );
    }

    __kmp_global.threads[ global_tid ]->th.th_ident = loc;
    // TODO: explicit barrier_wait_id:
    //   this function is called when 'barrier' directive is present or
    //   implicit barrier at the end of a worksharing construct.
    // 1) better to add a per-thread barrier counter to a thread data structure
    // 2) set to 0 when a new team is created
    // 4) no sync is required

    __kmp_barrier( global_tid );
}

/* The BARRIER for a MASTER section is always explicit   */
/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param global_tid  global thread number .
@return 1 if this thread should execute the <tt>master</tt> block, 0 otherwise.
*/
kmp_int32
__kmpc_master(ident_t *loc, kmp_int32 global_tid)
{
    int status = 0;

    KC_TRACE( 10, ("__kmpc_master: called T#%d\n", global_tid ) );

    if( ! TCR_4( __kmp_global.init_parallel ) )
        __kmp_parallel_initialize();

    if( KMP_MASTER_GTID( global_tid )) {
        KMP_COUNT_BLOCK(OMP_MASTER);
        KMP_START_EXPLICIT_TIMER(OMP_master);
        status = 1;
    }

    if ( __kmp_global.env_consistency_check ) {
        if (status)
            __kmp_push_sync( global_tid, ct_master, loc, NULL );
        else
            __kmp_check_sync( global_tid, ct_master, loc, NULL );
    }

    return status;
}

/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param global_tid  global thread number .

Mark the end of a <tt>master</tt> region. This should only be called by the thread
that executes the <tt>master</tt> region.
*/
void
__kmpc_end_master(ident_t *loc, kmp_int32 global_tid)
{
    KC_TRACE( 10, ("__kmpc_end_master: called T#%d\n", global_tid ) );

    KMP_DEBUG_ASSERT( KMP_MASTER_GTID( global_tid ));
    KMP_STOP_EXPLICIT_TIMER(OMP_master);

    if ( __kmp_global.env_consistency_check ) {
        if( global_tid < 0 )
            KMP_WARNING( ThreadIdentInvalid );

        if( KMP_MASTER_GTID( global_tid ))
            __kmp_pop_sync( global_tid, ct_master, loc );
    }
}

/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param gtid  global thread number.

Start execution of an <tt>ordered</tt> construct.
*/
void
__kmpc_ordered( ident_t * loc, kmp_int32 gtid )
{
    int cid = 0;
    kmp_info_t *th;
    KMP_DEBUG_ASSERT( __kmp_global.init_serial );

    KC_TRACE( 10, ("__kmpc_ordered: called T#%d\n", gtid ));

    if (! TCR_4(__kmp_global.init_parallel))
        __kmp_parallel_initialize();

    th = __kmp_global.threads[ gtid ];

    if ( th -> th.th_dispatch -> th_deo_fcn != 0 )
        (*th->th.th_dispatch->th_deo_fcn)( & gtid, & cid, loc );
    else
        __kmp_parallel_deo( & gtid, & cid, loc );
}

/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param gtid  global thread number.

End execution of an <tt>ordered</tt> construct.
*/
void
__kmpc_end_ordered( ident_t * loc, kmp_int32 gtid )
{
    int cid = 0;
    kmp_info_t *th;

    KC_TRACE( 10, ("__kmpc_end_ordered: called T#%d\n", gtid ) );

    th = __kmp_global.threads[ gtid ];

    if ( th -> th.th_dispatch -> th_dxo_fcn != 0 )
        (*th->th.th_dispatch->th_dxo_fcn)( & gtid, & cid, loc );
    else
        __kmp_parallel_dxo( & gtid, & cid, loc );
}


/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param global_tid  global thread number .
@param crit identity of the critical section. This could be a pointer to a lock associated with the critical section, or
some other suitably unique value.

Enter code protected by a `critical` construct.
This function blocks until the executing thread can enter the critical section.
*/
void
__kmpc_critical( ident_t * loc, kmp_int32 global_tid, kmp_critical_name * crit )
{
    KMP_COUNT_BLOCK(OMP_CRITICAL);

    KC_TRACE( 10, ("__kmpc_critical: called T#%d\n", global_tid ) );

    /* since the critical directive binds to all threads, not just
     * the current team we have to check this even if we are in a
     * serialized team */
    /* also, even if we are the uber thread, we still have to conduct the lock,
     * as we have to contend with sibling threads */

    // Value of 'crit' should be good for using as a critical_id of the critical section directive.
    __kmp_global_enter_critical_section( global_tid, crit );

    KA_TRACE( 15, ("__kmpc_critical: done T#%d\n", global_tid ));
}


/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param global_tid  global thread number .
@param crit identity of the critical section. This could be a pointer to a lock associated with the critical section, or
some other suitably unique value.

Leave a critical section, releasing any lock that was held during its execution.
*/
void
__kmpc_end_critical(ident_t *loc, kmp_int32 global_tid, kmp_critical_name *crit)
{
    KC_TRACE( 10, ("__kmpc_end_critical: called T#%d\n", global_tid ));

    // Value of 'crit' should be good for using as a critical_id of the critical section directive.
    __kmp_global_end_critical_section( global_tid, crit );

    KA_TRACE( 15, ("__kmpc_end_critical: done T#%d\n", global_tid ));
}

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid thread id.
@return one if the thread should execute the master block, zero otherwise

Start execution of a combined barrier and master. The barrier is executed inside this function.
*/
kmp_int32
__kmpc_barrier_master(ident_t *loc, kmp_int32 global_tid)
{
    int status;

    KC_TRACE( 10, ("__kmpc_barrier_master: called T#%d\n", global_tid ) );

    if (! TCR_4(__kmp_global.init_parallel))
        __kmp_parallel_initialize();

    if ( __kmp_global.env_consistency_check )
        __kmp_check_barrier( global_tid, ct_barrier, loc );

    status = __kmp_begin_split_barrier( global_tid );

    return (status != 0) ? 0 : 1;
}

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid thread id.

Complete the execution of a combined barrier and master. This function should
only be called at the completion of the <tt>master</tt> code. Other threads will
still be waiting at the barrier and this call releases them.
*/
void
__kmpc_end_barrier_master(ident_t *loc, kmp_int32 global_tid)
{
    KC_TRACE( 10, ("__kmpc_end_barrier_master: called T#%d\n", global_tid ));

    __kmp_end_split_barrier ( global_tid );
}

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid thread id.
@return one if the thread should execute the master block, zero otherwise

Start execution of a combined barrier and master(nowait) construct.
The barrier is executed inside this function.
There is no equivalent "end" function, since the
*/
kmp_int32
__kmpc_barrier_master_nowait( ident_t * loc, kmp_int32 global_tid )
{
    kmp_int32 ret;

    KC_TRACE( 10, ("__kmpc_barrier_master_nowait: called T#%d\n", global_tid ));

    if (! TCR_4(__kmp_global.init_parallel))
        __kmp_parallel_initialize();

    if ( __kmp_global.env_consistency_check ) {
        if ( loc == 0 ) {
            KMP_WARNING( ConstructIdentInvalid ); // ??? What does it mean for the user?
        }
        __kmp_check_barrier( global_tid, ct_barrier, loc );
    }

    __kmp_barrier( global_tid );

    ret = __kmpc_master (loc, global_tid);

    if ( __kmp_global.env_consistency_check ) {
        /*  there's no __kmpc_end_master called; so the (stats) */
        /*  actions of __kmpc_end_master are done here          */

        if ( global_tid < 0 ) {
            KMP_WARNING( ThreadIdentInvalid );
        }
        if (ret) {
            /* only one thread should do the pop since only */
            /* one did the push (see __kmpc_master())       */

            __kmp_pop_sync( global_tid, ct_master, loc );
        }
    }

    return (ret);
}

/* The BARRIER for a SINGLE process section is always explicit   */
/*!
@ingroup WORK_SHARING
@param loc  source location information
@param global_tid  global thread number
@return One if this thread should execute the single construct, zero otherwise.

Test whether to execute a <tt>single</tt> construct.
There are no implicit barriers in the two "single" calls, rather the compiler should
introduce an explicit barrier if it is required.
*/

kmp_int32
__kmpc_single(ident_t *loc, kmp_int32 global_tid)
{
    kmp_int32 rc = __kmp_enter_single( global_tid, loc, TRUE );

    if (rc) {
        // We are going to execute the single statement, so we should count it.
        KMP_COUNT_BLOCK(OMP_SINGLE);
        KMP_START_EXPLICIT_TIMER(OMP_single);
    }

    return rc;
}

/*!
@ingroup WORK_SHARING
@param loc  source location information
@param global_tid  global thread number

Mark the end of a <tt>single</tt> construct.  This function should
only be called by the thread that executed the block of code protected
by the `single` construct.
*/
void
__kmpc_end_single(ident_t *loc, kmp_int32 global_tid)
{
    __kmp_exit_single( global_tid );
    KMP_STOP_EXPLICIT_TIMER(OMP_single);
}

/*!
@ingroup WORK_SHARING
@param loc Source location
@param global_tid Global thread id

Mark the end of a statically scheduled loop.
*/
void
__kmpc_for_static_fini( ident_t *loc, kmp_int32 global_tid )
{
    KE_TRACE( 10, ("__kmpc_for_static_fini called T#%d\n", global_tid));

    if ( __kmp_global.env_consistency_check )
     __kmp_pop_workshare( global_tid, ct_pdo, loc );
}

/*
 * User routines which take C-style arguments (call by value)
 * different from the Fortran equivalent routines
 */

void
ompc_set_num_threads( int arg )
{
// !!!!! TODO: check the per-task binding
    __kmp_set_num_threads( arg, __kmp_entry_gtid() );
}

void
ompc_set_dynamic( int flag )
{
    kmp_info_t *thread;

    /* For the thread-private implementation of the internal controls */
    thread = __kmp_entry_thread();

    __kmp_save_internal_controls( thread );

    set__dynamic( thread, flag ? TRUE : FALSE );
}

void
ompc_set_nested( int flag )
{
    kmp_info_t *thread;

    /* For the thread-private internal controls implementation */
    thread = __kmp_entry_thread();

    __kmp_save_internal_controls( thread );

    set__nested( thread, flag ? TRUE : FALSE );
}

void
ompc_set_max_active_levels( int max_active_levels )
{
    /* TO DO */
    /* we want per-task implementation of this internal control */

    /* For the per-thread internal controls implementation */
    __kmp_set_max_active_levels( __kmp_entry_gtid(), max_active_levels );
}

void
ompc_set_schedule( omp_sched_t kind, int modifier )
{
// !!!!! TODO: check the per-task binding
    __kmp_set_schedule( __kmp_entry_gtid(), ( kmp_sched_t ) kind, modifier );
}

int
ompc_get_ancestor_thread_num( int level )
{
    return __kmp_get_ancestor_thread_num( __kmp_entry_gtid(), level );
}

int
ompc_get_team_size( int level )
{
    return __kmp_get_team_size( __kmp_entry_gtid(), level );
}

void
kmpc_set_stacksize( int arg )
{
    // __kmp_aux_set_stacksize initializes the library if needed
    __kmp_aux_set_stacksize( arg );
}

void
kmpc_set_stacksize_s( size_t arg )
{
    // __kmp_aux_set_stacksize initializes the library if needed
    __kmp_aux_set_stacksize( arg );
}

void
kmpc_set_blocktime( int arg )
{
    int gtid, tid;
    kmp_info_t *thread;

    gtid = __kmp_entry_gtid();
    tid = __kmp_tid_from_gtid(gtid);
    thread = __kmp_thread_from_gtid(gtid);

    __kmp_aux_set_blocktime( arg, thread, tid );
}

void
kmpc_set_library( int arg )
{
    // __kmp_user_set_library initializes the library if needed
    __kmp_user_set_library( (enum library_type)arg );
}

void
kmpc_set_defaults( char const * str )
{
    // __kmp_aux_set_defaults initializes the library if needed
    __kmp_aux_set_defaults( str, KMP_STRLEN( str ) );
}

int
kmpc_set_affinity_mask_proc( int proc, void **mask )
{
#if defined(KMP_STUB) || !KMP_AFFINITY_SUPPORTED
    return -1;
#else
    if ( ! TCR_4(__kmp_init_middle) ) {
        __kmp_middle_initialize();
    }
    return __kmp_aux_set_affinity_mask_proc( proc, mask );
#endif
}

int
kmpc_unset_affinity_mask_proc( int proc, void **mask )
{
#if defined(KMP_STUB) || !KMP_AFFINITY_SUPPORTED
    return -1;
#else
    if ( ! TCR_4(__kmp_init_middle) ) {
        __kmp_middle_initialize();
    }
    return __kmp_aux_unset_affinity_mask_proc( proc, mask );
#endif
}

int
kmpc_get_affinity_mask_proc( int proc, void **mask )
{
#if defined(KMP_STUB) || !KMP_AFFINITY_SUPPORTED
    return -1;
#else
    if ( ! TCR_4(__kmp_init_middle) ) {
        __kmp_middle_initialize();
    }
    return __kmp_aux_get_affinity_mask_proc( proc, mask );
#endif
}


/* -------------------------------------------------------------------------- */
/*!
@ingroup THREADPRIVATE
@param loc       source location information
@param gtid      global thread number
@param cpy_size  size of the cpy_data buffer
@param cpy_data  pointer to data to be copied
@param cpy_func  helper function to call for copying data
@param didit     flag variable: 1=single thread; 0=not single thread

__kmpc_copyprivate implements the interface for the private data broadcast needed for
the copyprivate clause associated with a single region in an OpenMP<sup>*</sup> program (both C and Fortran).
All threads participating in the parallel region call this routine.
One of the threads (called the single thread) should have the <tt>didit</tt> variable set to 1
and all other threads should have that variable set to 0.
All threads pass a pointer to a data buffer (cpy_data) that they have built.

The OpenMP specification forbids the use of nowait on the single region when a copyprivate
clause is present. However, @ref __kmpc_copyprivate implements a barrier internally to avoid
race conditions, so the code generation for the single region should avoid generating a barrier
after the call to @ref __kmpc_copyprivate.

The <tt>gtid</tt> parameter is the global thread id for the current thread.
The <tt>loc</tt> parameter is a pointer to source location information.

Internal implementation: The single thread will first copy its descriptor address (cpy_data)
to a team-private location, then the other threads will each call the function pointed to by
the parameter cpy_func, which carries out the copy by copying the data using the cpy_data buffer.

The cpy_func routine used for the copy and the contents of the data area defined by cpy_data
and cpy_size may be built in any fashion that will allow the copy to be done. For instance,
the cpy_data buffer can hold the actual data to be copied or it may hold a list of pointers
to the data. The cpy_func routine must interpret the cpy_data buffer appropriately.

The interface to cpy_func is as follows:
@code
void cpy_func( void *destination, void *source )
@endcode
where void *destination is the cpy_data pointer for the thread being copied to
and void *source is the cpy_data pointer for the thread being copied from.
*/
void
__kmpc_copyprivate( ident_t *loc, kmp_int32 gtid, size_t cpy_size, void *cpy_data, void(*cpy_func)(void*,void*), kmp_int32 didit )
{
    void **data_ptr;

    KC_TRACE( 10, ("__kmpc_copyprivate: called T#%d\n", gtid ));

    KMP_MB();

    data_ptr = & __kmp_team_from_gtid( gtid )->t.t_copypriv_data;

    if ( __kmp_global.env_consistency_check ) {
        if ( loc == 0 ) {
            KMP_WARNING( ConstructIdentInvalid );
        }
    }

    /* ToDo: Optimize the following two barriers into some kind of split barrier */

    if (didit) *data_ptr = cpy_data;

    /* This barrier is not a barrier region boundary */
    __kmp_barrier( gtid );

    if (! didit) (*cpy_func)( cpy_data, *data_ptr );

    /* Consider next barrier the user-visible barrier for barrier region boundaries */
    /* Nesting checks are already handled by the single construct checks */

    __kmp_barrier( gtid );
}

/* -------------------------------------------------------------------------- */

/*
 * TODO: Make check abort messages use location info & pass it
 * into with_checks routines
 */

/* initialize the lock */
void
__kmpc_init_lock( ident_t * loc, kmp_int32 gtid,  void ** user_lock ) {

    static char const * const func = "omp_init_lock";
    kmp_lock_t lck;
    KMP_DEBUG_ASSERT( __kmp_global.init_serial );

    if ( __kmp_global.env_consistency_check ) {
        if ( user_lock == NULL ) {
            KMP_FATAL( LockIsUninitialized, func );
        }
    }

    __kmp_init_lock( &lck );
    *(kmp_lock_t *)user_lock = lck;

} // __kmpc_init_lock

/* initialize the lock */
void
__kmpc_init_nest_lock( ident_t * loc, kmp_int32 gtid, void ** user_lock ) {

    static char const * const func = "omp_init_nest_lock";
    kmp_lock_t lck;
    KMP_DEBUG_ASSERT( __kmp_global.init_serial );

    if ( __kmp_global.env_consistency_check ) {
        if ( user_lock == NULL ) {
            KMP_FATAL( LockIsUninitialized, func );
        }
    }

    __kmp_init_nest_lock( &lck );
    *(kmp_lock_t *)user_lock = lck;

} // __kmpc_init_nest_lock

void
__kmpc_destroy_lock( ident_t * loc, kmp_int32 gtid, void ** user_lock ) {

    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_destroy_lock( &lck );

} // __kmpc_destroy_lock

/* destroy the lock */
void
__kmpc_destroy_nest_lock( ident_t * loc, kmp_int32 gtid, void ** user_lock ) {

    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_destroy_lock( &lck );

} // __kmpc_destroy_nest_lock

void
__kmpc_set_lock( ident_t * loc, kmp_int32 gtid, void ** user_lock )
{
    KMP_COUNT_BLOCK(OMP_set_lock);
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_acquire_lock( &lck, gtid );
}

void
__kmpc_set_nest_lock( ident_t * loc, kmp_int32 gtid, void ** user_lock )
{
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_acquire_lock( &lck, gtid );
}

void
__kmpc_unset_lock( ident_t *loc, kmp_int32 gtid, void **user_lock )
{
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_release_lock( &lck, gtid );
}

/* release the lock */
void
__kmpc_unset_nest_lock( ident_t *loc, kmp_int32 gtid, void **user_lock )
{
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    __kmp_release_lock( &lck, gtid );
}

/* try to acquire the lock */
int
__kmpc_test_lock( ident_t *loc, kmp_int32 gtid, void **user_lock )
{
    KMP_COUNT_BLOCK(OMP_test_lock);

    int rc;
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    rc = __kmp_test_lock( &lck, gtid );
    return ( rc == GLT_SUCCESS ? FTN_TRUE : FTN_FALSE );
}

/* try to acquire the lock */
int
__kmpc_test_nest_lock( ident_t *loc, kmp_int32 gtid, void **user_lock )
{
    int rc;
    kmp_lock_t lck = *(kmp_lock_t *)user_lock;
    rc = __kmp_test_lock( &lck, gtid );
    return ( rc == GLT_SUCCESS ? FTN_TRUE : FTN_FALSE );
}


/*--------------------------------------------------------------------------------------------------------------------*/

/*
 * Interface to fast scalable reduce methods routines
 */

// keep the selected method in a thread local structure for cross-function usage: will be used in __kmpc_end_reduce* functions;
// another solution: to re-determine the method one more time in __kmpc_end_reduce* functions (new prototype required then)
// AT: which solution is better?
#define __KMP_SET_REDUCTION_METHOD(gtid,rmethod) \
                   ( ( __kmp_global.threads[ ( gtid ) ] -> th.th_local.packed_reduction_method ) = ( rmethod ) )

#define __KMP_GET_REDUCTION_METHOD(gtid) \
                   ( __kmp_global.threads[ ( gtid ) ] -> th.th_local.packed_reduction_method )

// description of the packed_reduction_method variable: look at the macros in kmp.h


/* 2.a.i. Reduce Block without a terminating barrier */
/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid global thread number
@param num_vars number of items (variables) to be reduced
@param reduce_size size of data in bytes to be reduced
@param reduce_data pointer to data to be reduced
@param reduce_func callback function providing reduction operation on two operands and returning result of reduction in lhs_data
@param lck pointer to the unique lock data structure
@result 1 for the master thread, 0 for all other team threads, 2 for all team threads if atomic reduction needed

The nowait version is used for a reduce clause with the nowait argument.
*/
kmp_int32
__kmpc_reduce_nowait(
    ident_t *loc, kmp_int32 global_tid,
    kmp_int32 num_vars, size_t reduce_size, void *reduce_data, void (*reduce_func)(void *lhs_data, void *rhs_data),
    kmp_critical_name *lck ) {

    KMP_COUNT_BLOCK(REDUCE_nowait);
    int retval = 0;
    PACKED_REDUCTION_METHOD_T packed_reduction_method;
#if OMP_40_ENABLED
    kmp_team_t *team;
    kmp_info_t *th;
    int teams_swapped = 0, task_state;
#endif
    KA_TRACE( 10, ( "__kmpc_reduce_nowait() enter: called T#%d\n", global_tid ) );

    // why do we need this initialization here at all?
    // Reduction clause can not be used as a stand-alone directive.

    // do not call __kmp_serial_initialize(), it will be called by __kmp_parallel_initialize() if needed
    // possible detection of false-positive race by the threadchecker ???
    if( ! TCR_4( __kmp_global.init_parallel ) )
        __kmp_parallel_initialize();

    // check correctness of reduce block nesting
    if ( __kmp_global.env_consistency_check )
        __kmp_push_sync( global_tid, ct_reduce, loc, NULL );

#if OMP_40_ENABLED
    th = __kmp_thread_from_gtid(global_tid);
    if( th->th.th_teams_microtask ) {   // AC: check if we are inside the teams construct?
        team = th->th.th_team;
        if( team->t.t_level == th->th.th_teams_level ) {
            // this is reduction at teams construct
            KMP_DEBUG_ASSERT(!th->th.th_info.ds.ds_tid);  // AC: check that tid == 0
            // Let's swap teams temporarily for the reduction barrier
            teams_swapped = 1;
            th->th.th_info.ds.ds_tid = team->t.t_master_tid;
            th->th.th_team = team->t.t_parent;
            th->th.th_team_nproc = th->th.th_team->t.t_nproc;
            th->th.th_task_team = th->th.th_team->t.t_task_team[0];
            task_state = th->th.th_task_state;
            th->th.th_task_state = 0;
        }
    }
#endif // OMP_40_ENABLED

    // packed_reduction_method value will be reused by __kmp_end_reduce* function, the value should be kept in a variable
    // the variable should be either a construct-specific or thread-specific property, not a team specific property
    //     (a thread can reach the next reduce block on the next construct, reduce method may differ on the next construct)
    // an ident_t "loc" parameter could be used as a construct-specific property (what if loc == 0?)
    //     (if both construct-specific and team-specific variables were shared, then unness extra syncs should be needed)
    // a thread-specific variable is better regarding two issues above (next construct and extra syncs)
    // a thread-specific "th_local.reduction_method" variable is used currently
    // each thread executes 'determine' and 'set' lines (no need to execute by one thread, to avoid unness extra syncs)

    packed_reduction_method = __kmp_determine_reduction_method( loc, global_tid, num_vars, reduce_size, reduce_data, reduce_func, lck );
    __KMP_SET_REDUCTION_METHOD( global_tid, packed_reduction_method );

    /* [SM] TODO: supporting tree reduction? */

    if( packed_reduction_method == critical_reduce_block ) {

        __kmp_team_enter_critical_section( global_tid, lck );
        retval = 1;

    } else if( packed_reduction_method == empty_reduce_block ) {

        // usage: if team size == 1, no synchronization is required ( Intel platforms only )
        retval = 1;

    } else if( packed_reduction_method == atomic_reduce_block ) {

        retval = 2;

        // all threads should do this pop here (because __kmpc_end_reduce_nowait() won't be called by the code gen)
        //     (it's not quite good, because the checking block has been closed by this 'pop',
        //      but atomic operation has not been executed yet, will be executed slightly later, literally on next instruction)
        if ( __kmp_global.env_consistency_check )
            __kmp_pop_sync( global_tid, ct_reduce, loc );

    } else {

        // should never reach this block
        KMP_ASSERT( 0 ); // "unexpected method"

    }
#if OMP_40_ENABLED
    if( teams_swapped ) {
        // Restore thread structure
        th->th.th_info.ds.ds_tid = 0;
        th->th.th_team = team;
        th->th.th_team_nproc = team->t.t_nproc;
        th->th.th_task_team = team->t.t_task_team[task_state];
        th->th.th_task_state = task_state;
    }
#endif
    KA_TRACE( 10, ( "__kmpc_reduce_nowait() exit: called T#%d: method %08x, returns %08x\n", global_tid, packed_reduction_method, retval ) );

    return retval;
}

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid global thread id.
@param lck pointer to the unique lock data structure

Finish the execution of a reduce nowait.
*/
void
__kmpc_end_reduce_nowait( ident_t *loc, kmp_int32 global_tid, kmp_critical_name *lck ) {

    PACKED_REDUCTION_METHOD_T packed_reduction_method;

    KA_TRACE( 10, ( "__kmpc_end_reduce_nowait() enter: called T#%d\n", global_tid ) );

    packed_reduction_method = __KMP_GET_REDUCTION_METHOD( global_tid );

    if( packed_reduction_method == critical_reduce_block ) {

        __kmp_team_end_critical_section( global_tid, lck );

    } else if( packed_reduction_method == empty_reduce_block ) {

        // usage: if team size == 1, no synchronization is required ( on Intel platforms only )

    } else if( packed_reduction_method == atomic_reduce_block ) {

        // neither master nor other workers should get here
        //     (code gen does not generate this call in case 2: atomic reduce block)
        // actually it's better to remove this elseif at all;
        // after removal this value will checked by the 'else' and will assert

    } else {

        // should never reach this block
        KMP_ASSERT( 0 ); // "unexpected method"

    }

    if ( __kmp_global.env_consistency_check )
        __kmp_pop_sync( global_tid, ct_reduce, loc );

    KA_TRACE( 10, ( "__kmpc_end_reduce_nowait() exit: called T#%d: method %08x\n", global_tid, packed_reduction_method ) );

    return;
}

/* 2.a.ii. Reduce Block with a terminating barrier */

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid global thread number
@param num_vars number of items (variables) to be reduced
@param reduce_size size of data in bytes to be reduced
@param reduce_data pointer to data to be reduced
@param reduce_func callback function providing reduction operation on two operands and returning result of reduction in lhs_data
@param lck pointer to the unique lock data structure
@result 1 for the master thread, 0 for all other team threads, 2 for all team threads if atomic reduction needed

A blocking reduce that includes an implicit barrier.
*/
kmp_int32
__kmpc_reduce(
    ident_t *loc, kmp_int32 global_tid,
    kmp_int32 num_vars, size_t reduce_size, void *reduce_data,
    void (*reduce_func)(void *lhs_data, void *rhs_data),
    kmp_critical_name *lck )
{
    KMP_COUNT_BLOCK(REDUCE_wait);
    int retval = 0;
    PACKED_REDUCTION_METHOD_T packed_reduction_method;

    KA_TRACE( 10, ( "__kmpc_reduce() enter: called T#%d\n", global_tid ) );

    // why do we need this initialization here at all?
    // Reduction clause can not be a stand-alone directive.

    // do not call __kmp_serial_initialize(), it will be called by __kmp_parallel_initialize() if needed
    // possible detection of false-positive race by the threadchecker ???
    if( ! TCR_4( __kmp_global.init_parallel ) )
        __kmp_parallel_initialize();

    // check correctness of reduce block nesting
    if ( __kmp_global.env_consistency_check )
        __kmp_push_sync( global_tid, ct_reduce, loc, NULL );

    packed_reduction_method = __kmp_determine_reduction_method( loc, global_tid, num_vars, reduce_size, reduce_data, reduce_func, lck );
    __KMP_SET_REDUCTION_METHOD( global_tid, packed_reduction_method );

    /* [SM] TODO: supporting tree reduction? */

    if( packed_reduction_method == critical_reduce_block ) {

        __kmp_team_enter_critical_section( global_tid, lck );
        retval = 1;

    } else if( packed_reduction_method == empty_reduce_block ) {

        // usage: if team size == 1, no synchronization is required ( Intel platforms only )
        retval = 1;

    } else if( packed_reduction_method == atomic_reduce_block ) {

        retval = 2;

    } else {

        // should never reach this block
        KMP_ASSERT( 0 ); // "unexpected method"

    }

    KA_TRACE( 10, ( "__kmpc_reduce() exit: called T#%d: method %08x, returns %08x\n", global_tid, packed_reduction_method, retval ) );

    return retval;
}

/*!
@ingroup SYNCHRONIZATION
@param loc source location information
@param global_tid global thread id.
@param lck pointer to the unique lock data structure

Finish the execution of a blocking reduce.
The <tt>lck</tt> pointer must be the same as that used in the corresponding start function.
*/
void
__kmpc_end_reduce( ident_t *loc, kmp_int32 global_tid, kmp_critical_name *lck ) {

    PACKED_REDUCTION_METHOD_T packed_reduction_method;

    KA_TRACE( 10, ( "__kmpc_end_reduce() enter: called T#%d\n", global_tid ) );

    packed_reduction_method = __KMP_GET_REDUCTION_METHOD( global_tid );

    // this barrier should be visible to a customer and to the threading profile tool
    //              (it's a terminating barrier on constructs if NOWAIT not specified)

    if( packed_reduction_method == critical_reduce_block ) {

        __kmp_team_end_critical_section( global_tid, lck );

        // TODO: implicit barrier: should be exposed
        __kmp_barrier( global_tid );

    } else if( packed_reduction_method == empty_reduce_block ) {

        // usage: if team size == 1, no synchronization is required ( Intel platforms only )

        // TODO: implicit barrier: should be exposed
        __kmp_barrier( global_tid );

    } else if( packed_reduction_method == atomic_reduce_block ) {

        // TODO: implicit barrier: should be exposed
        __kmp_barrier( global_tid );

    } else {

        // should never reach this block
        KMP_ASSERT( 0 ); // "unexpected method"

    }

    if ( __kmp_global.env_consistency_check )
        __kmp_pop_sync( global_tid, ct_reduce, loc );

    KA_TRACE( 10, ( "__kmpc_end_reduce() exit: called T#%d: method %08x\n", global_tid, packed_reduction_method ) );

    return;
}

#undef __KMP_GET_REDUCTION_METHOD
#undef __KMP_SET_REDUCTION_METHOD

/*-- end of interface to fast scalable reduce routines ---------------------------------------------------------------*/

kmp_uint64
__kmpc_get_taskid() {

    kmp_int32    gtid;
    kmp_info_t * thread;

    gtid = __kmp_get_gtid();
    if ( gtid < 0 ) {
        return 0;
    }; // if
    thread = __kmp_thread_from_gtid( gtid );
    return thread->th.th_current_task->td_task_id;

} // __kmpc_get_taskid


kmp_uint64
__kmpc_get_parent_taskid() {

    kmp_int32        gtid;
    kmp_info_t *     thread;
    kmp_taskdata_t * parent_task;

    gtid = __kmp_get_gtid();
    if ( gtid < 0 ) {
        return 0;
    }; // if
    thread      = __kmp_thread_from_gtid( gtid );
    parent_task = thread->th.th_current_task->td_parent;
    return ( parent_task == NULL ? 0 : parent_task->td_task_id );

} // __kmpc_get_parent_taskid

void __kmpc_place_threads(int nS, int sO, int nC, int cO, int nT)
{
    if ( ! __kmp_global.init_serial ) {
        __kmp_serial_initialize();
    }
    __kmp_global.place_num_sockets = nS;
    __kmp_global.place_socket_offset = sO;
    __kmp_global.place_num_cores = nC;
    __kmp_global.place_core_offset = cO;
    __kmp_global.place_num_threads_per_core = nT;
}

#if OMP_41_ENABLED
/*!
@ingroup WORK_SHARING
@param loc  source location information.
@param gtid  global thread number.
@param num_dims  number of associated doacross loops.
@param dims  info on loops bounds.

Initialize doacross loop information.
Expect compiler send us inclusive bounds,
e.g. for(i=2;i<9;i+=2) lo=2, up=8, st=2.
*/
void
__kmpc_doacross_init(ident_t *loc, int gtid, int num_dims, struct kmp_dim * dims)
{
    int j, idx;
    kmp_int64 last, trace_count;
    kmp_info_t *th = __kmp_global.threads[gtid];
    kmp_team_t *team = th->th.th_team;
    kmp_uint32 *flags;
    kmp_disp_t *pr_buf = th->th.th_dispatch;
    dispatch_shared_info_t *sh_buf;

    KA_TRACE(20,("__kmpc_doacross_init() enter: called T#%d, num dims %d, active %d\n",
                 gtid, num_dims, !team->t.t_serialized));
    KMP_DEBUG_ASSERT(dims != NULL);
    KMP_DEBUG_ASSERT(num_dims > 0);

    if( team->t.t_serialized ) {
        KA_TRACE(20,("__kmpc_doacross_init() exit: serialized team\n"));
        return; // no dependencies if team is serialized
    }
    KMP_DEBUG_ASSERT(team->t.t_nproc > 1);
    idx = pr_buf->th_doacross_buf_idx++; // Increment index of shared buffer for the next loop
    sh_buf = &team->t.t_disp_buffer[idx % KMP_MAX_DISP_BUF];

    // Save bounds info into allocated private buffer
    KMP_DEBUG_ASSERT(pr_buf->th_doacross_info == NULL);
    pr_buf->th_doacross_info =
        (kmp_int64*)__kmp_thread_malloc(th, sizeof(kmp_int64)*(4 * num_dims + 1));
    KMP_DEBUG_ASSERT(pr_buf->th_doacross_info != NULL);
    pr_buf->th_doacross_info[0] = (kmp_int64)num_dims; // first element is number of dimensions
    // Save also address of num_done in order to access it later without knowing the buffer index
    pr_buf->th_doacross_info[1] = (kmp_int64)&sh_buf->doacross_num_done;
    pr_buf->th_doacross_info[2] = dims[0].lo;
    pr_buf->th_doacross_info[3] = dims[0].up;
    pr_buf->th_doacross_info[4] = dims[0].st;
    last = 5;
    for( j = 1; j < num_dims; ++j ) {
        kmp_int64 range_length; // To keep ranges of all dimensions but the first dims[0]
        if( dims[j].st == 1 ) { // most common case
            // AC: should we care of ranges bigger than LLONG_MAX? (not for now)
            range_length = dims[j].up - dims[j].lo + 1;
        } else {
            if( dims[j].st > 0 ) {
                KMP_DEBUG_ASSERT(dims[j].up > dims[j].lo);
                range_length = (kmp_uint64)(dims[j].up - dims[j].lo) / dims[j].st + 1;
            } else {            // negative increment
                KMP_DEBUG_ASSERT(dims[j].lo > dims[j].up);
                range_length = (kmp_uint64)(dims[j].lo - dims[j].up) / (-dims[j].st) + 1;
            }
        }
        pr_buf->th_doacross_info[last++] = range_length;
        pr_buf->th_doacross_info[last++] = dims[j].lo;
        pr_buf->th_doacross_info[last++] = dims[j].up;
        pr_buf->th_doacross_info[last++] = dims[j].st;
    }

    // Compute total trip count.
    // Start with range of dims[0] which we don't need to keep in the buffer.
    if( dims[0].st == 1 ) { // most common case
        trace_count = dims[0].up - dims[0].lo + 1;
    } else if( dims[0].st > 0 ) {
        KMP_DEBUG_ASSERT(dims[0].up > dims[0].lo);
        trace_count = (kmp_uint64)(dims[0].up - dims[0].lo) / dims[0].st + 1;
    } else {   // negative increment
        KMP_DEBUG_ASSERT(dims[0].lo > dims[0].up);
        trace_count = (kmp_uint64)(dims[0].lo - dims[0].up) / (-dims[0].st) + 1;
    }
    for( j = 1; j < num_dims; ++j ) {
        trace_count *= pr_buf->th_doacross_info[4 * j + 1]; // use kept ranges
    }
    KMP_DEBUG_ASSERT(trace_count > 0);

    // Check if shared buffer is not occupied by other loop (idx - KMP_MAX_DISP_BUF)
    if( idx != sh_buf->doacross_buf_idx ) {
        // Shared buffer is occupied, wait for it to be free
        __kmp_wait_yield_4( (kmp_uint32*)&sh_buf->doacross_buf_idx, idx, __kmp_eq_4, NULL );
    }
    // Check if we are the first thread. After the CAS the first thread gets 0,
    // others get 1 if initialization is in progress, allocated pointer otherwise.
    flags = (kmp_uint32*)KMP_COMPARE_AND_STORE_RET64(
        (kmp_int64*)&sh_buf->doacross_flags,NULL,(kmp_int64)1);
    if( flags == NULL ) {
        // we are the first thread, allocate the array of flags
        kmp_int64 size = trace_count / 8 + 8; // in bytes, use single bit per iteration
        sh_buf->doacross_flags = (kmp_uint32*)__kmp_thread_calloc(th, size, 1);
    } else if( (kmp_int64)flags == 1 ) {
        // initialization is still in progress, need to wait
        while( (volatile kmp_int64)sh_buf->doacross_flags == 1 ) {
            KMP_YIELD(TRUE);
        }
    }
    KMP_DEBUG_ASSERT((kmp_int64)sh_buf->doacross_flags > 1); // check value of pointer
    pr_buf->th_doacross_flags = sh_buf->doacross_flags;      // save private copy in order to not
                                                             // touch shared buffer on each iteration
    KA_TRACE(20,("__kmpc_doacross_init() exit: T#%d\n", gtid));
}

void
__kmpc_doacross_wait(ident_t *loc, int gtid, long long *vec)
{
    kmp_int32 shft, num_dims, i;
    kmp_uint32 flag;
    kmp_int64 iter_number; // iteration number of "collapsed" loop nest
    kmp_info_t *th = __kmp_global.threads[gtid];
    kmp_team_t *team = th->th.th_team;
    kmp_disp_t *pr_buf;
    kmp_int64 lo, up, st;

    KA_TRACE(20,("__kmpc_doacross_wait() enter: called T#%d\n", gtid));
    if( team->t.t_serialized ) {
        KA_TRACE(20,("__kmpc_doacross_wait() exit: serialized team\n"));
        return; // no dependencies if team is serialized
    }

    // calculate sequential iteration number and check out-of-bounds condition
    pr_buf = th->th.th_dispatch;
    KMP_DEBUG_ASSERT(pr_buf->th_doacross_info != NULL);
    num_dims = pr_buf->th_doacross_info[0];
    lo = pr_buf->th_doacross_info[2];
    up = pr_buf->th_doacross_info[3];
    st = pr_buf->th_doacross_info[4];
    if( st == 1 ) { // most common case
        if( vec[0] < lo || vec[0] > up ) {
            KA_TRACE(20,(
                "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                gtid, vec[0], lo, up));
            return;
        }
        iter_number = vec[0] - lo;
    } else if( st > 0 ) {
        if( vec[0] < lo || vec[0] > up ) {
            KA_TRACE(20,(
                "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                gtid, vec[0], lo, up));
            return;
        }
        iter_number = (kmp_uint64)(vec[0] - lo) / st;
    } else {        // negative increment
        if( vec[0] > lo || vec[0] < up ) {
            KA_TRACE(20,(
                "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                gtid, vec[0], lo, up));
            return;
        }
        iter_number = (kmp_uint64)(lo - vec[0]) / (-st);
    }
    for( i = 1; i < num_dims; ++i ) {
        kmp_int64 iter, ln;
        kmp_int32 j = i * 4;
        ln = pr_buf->th_doacross_info[j + 1];
        lo = pr_buf->th_doacross_info[j + 2];
        up = pr_buf->th_doacross_info[j + 3];
        st = pr_buf->th_doacross_info[j + 4];
        if( st == 1 ) {
            if( vec[i] < lo || vec[i] > up ) {
                KA_TRACE(20,(
                    "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                    gtid, vec[i], lo, up));
                return;
            }
            iter = vec[i] - lo;
        } else if( st > 0 ) {
            if( vec[i] < lo || vec[i] > up ) {
                KA_TRACE(20,(
                    "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                    gtid, vec[i], lo, up));
                return;
            }
            iter = (kmp_uint64)(vec[i] - lo) / st;
        } else {   // st < 0
            if( vec[i] > lo || vec[i] < up ) {
                KA_TRACE(20,(
                    "__kmpc_doacross_wait() exit: T#%d iter %lld is out of bounds [%lld,%lld]\n",
                    gtid, vec[i], lo, up));
                return;
            }
            iter = (kmp_uint64)(lo - vec[i]) / (-st);
        }
        iter_number = iter + ln * iter_number;
    }
    shft = iter_number % 32; // use 32-bit granularity
    iter_number >>= 5;       // divided by 32
    flag = 1 << shft;
    while( (flag & pr_buf->th_doacross_flags[iter_number]) == 0 ) {
        KMP_YIELD(TRUE);
    }
    KA_TRACE(20,("__kmpc_doacross_wait() exit: T#%d wait for iter %lld completed\n",
                 gtid, (iter_number<<5)+shft));
}

void
__kmpc_doacross_post(ident_t *loc, int gtid, long long *vec)
{
    kmp_int32 shft, num_dims, i;
    kmp_uint32 flag;
    kmp_int64 iter_number; // iteration number of "collapsed" loop nest
    kmp_info_t *th = __kmp_global.threads[gtid];
    kmp_team_t *team = th->th.th_team;
    kmp_disp_t *pr_buf;
    kmp_int64 lo, st;

    KA_TRACE(20,("__kmpc_doacross_post() enter: called T#%d\n", gtid));
    if( team->t.t_serialized ) {
        KA_TRACE(20,("__kmpc_doacross_post() exit: serialized team\n"));
        return; // no dependencies if team is serialized
    }

    // calculate sequential iteration number (same as in "wait" but no out-of-bounds checks)
    pr_buf = th->th.th_dispatch;
    KMP_DEBUG_ASSERT(pr_buf->th_doacross_info != NULL);
    num_dims = pr_buf->th_doacross_info[0];
    lo = pr_buf->th_doacross_info[2];
    st = pr_buf->th_doacross_info[4];
    if( st == 1 ) { // most common case
        iter_number = vec[0] - lo;
    } else if( st > 0 ) {
        iter_number = (kmp_uint64)(vec[0] - lo) / st;
    } else {        // negative increment
        iter_number = (kmp_uint64)(lo - vec[0]) / (-st);
    }
    for( i = 1; i < num_dims; ++i ) {
        kmp_int64 iter, ln;
        kmp_int32 j = i * 4;
        ln = pr_buf->th_doacross_info[j + 1];
        lo = pr_buf->th_doacross_info[j + 2];
        st = pr_buf->th_doacross_info[j + 4];
        if( st == 1 ) {
            iter = vec[i] - lo;
        } else if( st > 0 ) {
            iter = (kmp_uint64)(vec[i] - lo) / st;
        } else {   // st < 0
            iter = (kmp_uint64)(lo - vec[i]) / (-st);
        }
        iter_number = iter + ln * iter_number;
    }
    shft = iter_number % 32; // use 32-bit granularity
    iter_number >>= 5;       // divided by 32
    flag = 1 << shft;
    if( (flag & pr_buf->th_doacross_flags[iter_number]) == 0 )
        KMP_TEST_THEN_OR32( (kmp_int32*)&pr_buf->th_doacross_flags[iter_number], (kmp_int32)flag );
    KA_TRACE(20,("__kmpc_doacross_post() exit: T#%d iter %lld posted\n",
                 gtid, (iter_number<<5)+shft));
}

void
__kmpc_doacross_fini(ident_t *loc, int gtid)
{
    kmp_int64 num_done;
    kmp_info_t *th = __kmp_global.threads[gtid];
    kmp_team_t *team = th->th.th_team;
    kmp_disp_t *pr_buf = th->th.th_dispatch;

    KA_TRACE(20,("__kmpc_doacross_fini() enter: called T#%d\n", gtid));
    if( team->t.t_serialized ) {
        KA_TRACE(20,("__kmpc_doacross_fini() exit: serialized team %p\n", team));
        return; // nothing to do
    }
    num_done = KMP_TEST_THEN_INC64((kmp_int64*)pr_buf->th_doacross_info[1]) + 1;
    if( num_done == th->th.th_team_nproc ) {
        // we are the last thread, need to free shared resources
        int idx = pr_buf->th_doacross_buf_idx - 1;
        dispatch_shared_info_t *sh_buf = &team->t.t_disp_buffer[idx % KMP_MAX_DISP_BUF];
        KMP_DEBUG_ASSERT(pr_buf->th_doacross_info[1] == (kmp_int64)&sh_buf->doacross_num_done);
        KMP_DEBUG_ASSERT(num_done == (kmp_int64)sh_buf->doacross_num_done);
        KMP_DEBUG_ASSERT(idx == sh_buf->doacross_buf_idx);
        __kmp_thread_free(th, (void*)sh_buf->doacross_flags);
        sh_buf->doacross_flags = NULL;
        sh_buf->doacross_num_done = 0;
        sh_buf->doacross_buf_idx += KMP_MAX_DISP_BUF; // free buffer for future re-use
    }
    // free private resources (need to keep buffer index forever)
    __kmp_thread_free(th, (void*)pr_buf->th_doacross_info);
    pr_buf->th_doacross_info = NULL;
    KA_TRACE(20,("__kmpc_doacross_fini() exit: T#%d\n", gtid));
}
#endif

// end of file //


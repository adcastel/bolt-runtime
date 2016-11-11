/*! \file */
/*
 * kmp_abt_inline.h -- header file for inline functions
 */


//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//


#ifndef KMP_ABT_INLINE_H
#define KMP_ABT_INLINE_H

/* ------------------------------------------------------------------------ */

static inline void
__kmp_set_self_info( kmp_info_t *th )
{
    KMP_ASSERT( __kmp_global.init_runtime );

    ABT_self_set_arg((void *)th);
}

static inline kmp_info_t *
__kmp_get_self_info( void )
{
    KMP_ASSERT( __kmp_global.init_runtime );

    kmp_info_t *th;
    int ret = ABT_self_get_arg((void **)&th);
    KMP_ASSERT( ret == ABT_SUCCESS );
    return th;
}

static inline void
__kmp_gtid_set_specific( int gtid )
{
    ABT_thread self;
    kmp_info_t *th;
    KMP_ASSERT( __kmp_global.init_runtime );

    ABT_thread_self(&self);
    ABT_thread_get_arg(self, (void **)&th);
    KMP_ASSERT( th != NULL );
    th->th.th_info.ds.ds_gtid = gtid;
}

static inline int
__kmp_gtid_get_specific(void)
{
    ABT_thread self;
    kmp_info_t *th;
    int gtid;

    //if ( !__kmp_global.init_runtime ) {
    //    KA_TRACE( 50, ("__kmp_get_specific: runtime shutdown, returning KMP_GTID_SHUTDOWN\n" ) );
    //    return KMP_GTID_SHUTDOWN;
    //}

    ABT_thread_self(&self);
    ABT_thread_get_arg(self, (void **)&th);
    if (th == NULL) {
        gtid = KMP_GTID_DNE;
    } else {
        gtid = th->th.th_info.ds.ds_gtid;
    }
    KA_TRACE( 50, ("__kmp_gtid_get_specific: ULT:%p gtid:%d\n", self, gtid ));

    return gtid;
}

static inline void
__kmp_release_info(kmp_info_t *th)
{
    KMP_DEBUG_ASSERT(th->th.th_active == TRUE);
    TCW_4(th->th.th_active, FALSE);
}

static inline void
__kmp_acquire_info_for_task(kmp_info_t *th, kmp_taskdata_t *taskdata)
{
    while (KMP_COMPARE_AND_STORE_RET32(&th->th.th_active, FALSE, TRUE) != FALSE) {
        ABT_thread_yield();
    }
    th->th.th_current_task = taskdata;
}

/* ------------------------------------------------------------------------ */

#endif /* KMP_ABT_INLINE_H */

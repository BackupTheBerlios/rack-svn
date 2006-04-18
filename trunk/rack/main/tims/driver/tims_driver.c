/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 2 
 * of the License, or (at your option) any later version.
 *
 * Authors
 *      Joerg Langenberg  <joerg.langenberg@gmx.net>
 *      Sebastian Smolorz <Sebastian.Smolorz@stud.uni-hannover.de>
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/highmem.h>

#include <rtdm/rtdm_driver.h>
#include <native/pipe.h>

#include <main/tims/tims.h>
#include <main/tims/router/tims_msg_router.h>

#include <rack_config.h>    // CONFIG_TIMS_USE_RTNET

#define DRIVER_AUTHOR   "Joerg Langenberg - joerg.langenberg@gmx.net"
#define DRIVER_VERSION  "0.0.4"
#define DRIVER_DESC     "Tiny Messaging Service (TIMS)"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#ifdef CONFIG_TIMS_USE_RTNET

#include <net/ip.h>
#include <rtnet.h>
#include <rtmac.h>

#define TIMS_MSG_ROUTER_PORT          0x2000
#define TIME_REFERENCE_DEV            "TDMA0"

#endif   // CONFIG_TIMS_USE_RTNET

//
// state flags
//
#define TIMS_STATE_BIT_STARTING             0
#define TIMS_STATE_BIT_SHUTDOWN             1

//
// init flags
//
#define TIMS_INIT_BIT_RECV_TASK             0
#define TIMS_INIT_BIT_PIPE_TO_CLIENT        1
#define TIMS_INIT_BIT_PIPE_FROM_CLIENT      2
#define TIMS_INIT_BIT_REGISTERED            3
#define TIMS_INIT_BIT_MBX_CACHE             4

#ifdef CONFIG_TIMS_USE_RTNET
#define TIMS_INIT_BIT_RTNET_SOCKET          8
#define TIMS_INIT_BIT_RTNET_TDMA            9
#define TIMS_INIT_BIT_RTNET_MBXROUTE        10
#endif   // CONFIG_TIMS_USE_RTNET

//
// context flags
//
#define TIMS_CTX_BIT_BOUND                  0
#define TIMS_CTX_BIT_INITMBX                1
#define TIMS_CTX_BIT_MBX_CREATED            2
#define TIMS_CTX_BIT_CLOSING                3    // Mailbox is closing

//
// mailbox flags
//
#define TIMS_MBX_BIT_EXTERNBUFFER           0
#define TIMS_MBX_BIT_SLOT                   1
#define TIMS_MBX_BIT_FIFO                   2
#define TIMS_MBX_BIT_READERWAIT             3
#define TIMS_MBX_BIT_USRSPCBUFFER           4

//
// Module parameter
//

static int dbglevel = 1;    // default TIMS_LEVEL_ERROR

MODULE_PARM(dbglevel, "0-5i");
MODULE_PARM_DESC(dbglevel, "TiMS debug level, 0 = silent, 1 = error, 2 = info, "
                           "3 = warn, 4 = debug info, 5 = debug detail");

static int max_msg_size = 64; // default max message size in kb

MODULE_PARM(max_msg_size, "i");
MODULE_PARM_DESC(max_msg_size, "Max message size which can be sent by tims, "
                               "default = 64 (in kb)");

static int max_msg_slots = 2; // max number of message slots (send over tcp/pipe)

MODULE_PARM(max_msg_slots, "i");
MODULE_PARM_DESC(max_msg_slots, "Max number of messgage slots for sendig messages "
                                "over TCP/Pipe. default = 2");

#ifdef CONFIG_TIMS_USE_RTNET

static int rtnet_buffers = 200;
MODULE_PARM(rtnet_buffers, "i");
MODULE_PARM_DESC(rtnet_buffers, "number of RTnet buffers for incoming "
                                "and outgoing packets");

#endif   /* !CONFIG_TIMS_USE_RTNET */

//
// TIMS debug
//

#define TIMS_LEVEL_PRINT          0
#define TIMS_LEVEL_ERROR          1
#define TIMS_LEVEL_INFO           2
#define TIMS_LEVEL_WARN           3
#define TIMS_LEVEL_DBG_INFO       4
#define TIMS_LEVEL_DBG_DETAIL     5

#define TIMS_LEVEL_MAX            TIMS_LEVEL_DBG_DETAIL


#define tims_print(fmt, ... )    \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_PRINT)          \
                                    rtdm_printk("TIMS: <0x%p>: "fmt,         \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_print_0(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_PRINT)          \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

#define tims_error(fmt, ... )   \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_ERROR)          \
                                    rtdm_printk("TIMS: <0x%p>: ERROR: "fmt,  \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_error_0(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_ERROR)          \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

#define tims_info(fmt, ... )    \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_INFO)           \
                                    rtdm_printk("TIMS: <0x%p>: INFO:  "fmt,  \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_info_0(fmt, ... )  \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_INFO)           \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

#define tims_warn(fmt, ... )    \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_WARN)           \
                                    rtdm_printk("TIMS: <0x%p>: WARN:  "fmt,  \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_warn_0(fmt, ... )  \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_WARN)           \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

#define tims_dbginfo(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_DBG_INFO)       \
                                    rtdm_printk("TIMS: <0x%p>: DBG_I: "fmt,  \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_dbginfo_0(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_DBG_INFO)       \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

#define tims_dbgdetail(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_DBG_DETAIL)     \
                                    rtdm_printk("TIMS: <0x%p>: DBG_D: "fmt,  \
                                                current, ##__VA_ARGS__);     \
                                } while(0)

#define tims_dbgdetail_0(fmt, ... ) \
                                do {                                         \
                                  if (dbglevel >= TIMS_LEVEL_DBG_DETAIL)     \
                                    rtdm_printk(fmt, ##__VA_ARGS__);         \
                                } while(0)

//
// context data structures
//
typedef struct slot_state {
    int free;
    int write;
    int peek;
    int read;
} slot_state_t;

typedef struct tims_map_info {
    unsigned long virtual;
    unsigned char mapped;
} timsMapInfo_t;

typedef struct tims_mbx_slot {
    timsMsgHead*            p_head;         // message pointers
    unsigned long           p_head_map;     // mapped virtual kernel address
    struct list_head        mbx_list;       // to hold it in free|read|writelist

    int                     map_idx;        // start index of page table
    struct tims_mailbox*    p_mbx;          // pointer to mailbox
} timsMbxSlot;

typedef struct tims_mailbox {
    timsMbxSlot*            slot;           // mailbox slots
    rtdm_sem_t              readSem;        // semaphore for the mbx reader
                                            // (waiting for the next message)

    rtdm_lock_t             list_lock;      // lock for mbx_lists
    struct list_head        free_list;      // list of all free mailbox slots
    struct list_head        write_list;     // list of all mailbox write slots
    struct list_head        read_list;      // list of all mailbox read slots
    timsMbxSlot*            p_peek;         // save peek slot

    int64_t                 timeout_ns;     // timeout for peek/receive
    unsigned long           flags;          // mailbox state
    unsigned int            address;        // address of this mailbox

    // config values
    unsigned int            slot_count;     // 0  => FIFO Queueing
                                            // > 0 => Priority Queueing
    slot_state_t            slot_state;     // tims slot state
    unsigned int            msg_size;       // bytes per message
    void*                   buffer;         // NULL: kernel-located
    unsigned long           buffer_pages;   // number of buffer pages
    struct page**           pp_pages;       // pointer to page list
    timsMapInfo_t*          p_mapInfo;      // virtual addresses of page list
    size_t                  buffer_size;

} timsMbx;

typedef struct tims_context { // context will be created in rt_tims_socket by RTDM
    struct list_head        ctx_list;       // list of all registered contextes
    int                     protocol;       // tims protocol
    struct tims_sockaddr    sock;           // socket address
    timsMbx*                p_mbx;          // mailbox
    int                     use_counter;    // context use counter
    unsigned long           flags;          // context flags
} timsCtx;

typedef struct tims_ctx_cache { // fast search for context
    timsCtx*                p_ctx;         	// pointer to context
    unsigned int            mbxAdr;        	// mbx address
    uint64_t                timestamp;     	// last access time
} timsCtxCache;

#define CONFIG_TIMS_CTX_CACHE_SIZE      64

//
// driver values
//

struct tims_driver {
    unsigned long           init_flags;     // init flags
    unsigned long           state_flags;    // module state flags

    rtdm_lock_t             ctx_lock;       // context lock
    struct list_head        ctx_list;       // list of all registered contextes
    timsCtxCache            ctxCache[CONFIG_TIMS_CTX_CACHE_SIZE];

    int                     mbx_num;        // number of registered mailboxes

    rtdm_task_t             pipeRecvTask;   // Pipe receive task
    RT_PIPE                 pipeToClient;   // Pipe to Client
    RT_PIPE                 pipeFromClient; // Pipe from Client
    int                     terminate;      // terminate signal
    atomic_t                taskCount;      // task counter

#ifdef CONFIG_TIMS_USE_RTNET

    int                     rtnet_fd;
    timsMsgRouter_MbxRoute* mbxRoute;
    unsigned int            mbxRouteNum;
    int                     tdma_disc;

#endif /* CONFIG_TIMS_USE_RTNET */

};

static struct tims_driver td;


// ****************************************************************************
//
//     debug functions
//
// ****************************************************************************

/*
static void print_read_list(timsMbx *p_mbx)
{
    timsMbxSlot *slot;
    timsMsgHead *p_head;
    timsMsgHead *p_head_map;

    char *p_data;
    int pos = 1;
    int i;
    unsigned long databytes     = 0;
    unsigned long show_data_max = 10;
    unsigned long akt_byte      = 0;

    struct list_head *p_list = &p_mbx->read_list;

    rtdm_printk("---=== TIMS READ LIST , MAILBOX %08x ===---\n", p_mbx->address);
    rtdm_printk(" Entry |   msg_ptr   (mapped)  | type |    dest    |     src    | prio | flags | data ->\n");

    list_for_each_entry(slot, p_list, read_list)
    {
        p_head_map = (timsMsgHead *)slot->p_head_map;
        p_head     = slot->p_head;

        // print head
        rtdm_printk(" %5d | %10p %10p | %4d | %10x | %10x | %4d | %5x |",
                    pos, p_head, p_head_map, p_head_map->type, p_head_map->dest, p_head_map->src,
                    p_head_map->priority, p_head_map->flags);

        // print data
        databytes = p_head_map->msglen - TIMS_HEADLEN;
        if (databytes)
        {
            akt_byte = databytes > show_data_max ? show_data_max : databytes;
            p_data = p_head_map->data;
            for ( i=0; i<akt_byte; i++)
            {
                rtdm_printk(" %02x", p_data[i]);
            }
        }
        rtdm_printk("\n");
        pos++;
    }
}


static void print_write_list(timsMbx *p_mbx)
{
    timsMbxSlot *slot;
    timsMsgHead *p_head;
    timsMsgHead *p_head_map;

    unsigned char *p_data;
    int pos = 1;
    int i;
    unsigned long databytes     = 0;
    unsigned long show_data_max = 10;
    unsigned long akt_byte      = 0;

    struct list_head *p_list = &p_mbx->write_list;

    rtdm_printk("---=== TIMS WRITE LIST , MAILBOX %08x ===---\n",
                p_mbx->address);
    rtdm_printk(" Entry |   msg_ptr   (mapped)  | type |    dest    |"
                "     src    | prio | flags | data ->\n");

    list_for_each_entry(slot, p_list, write_list)
    {
        p_head_map = (timsMsgHead *)slot->p_head_map;
        p_head     = slot->p_head;

        // print head
        rtdm_printk(" %5d | %10p %10p | %4d | %10x | %10x | %4d | %5x |",
                    pos, p_head, p_head_map, p_head_map->type, p_head_map->dest, p_head_map->src,
                    p_head_map->priority, p_head_map->flags);

        // print data
        databytes = p_head_map->msglen - TIMS_HEADLEN;
        if (databytes)
        {
            akt_byte = databytes > show_data_max ? show_data_max : databytes;
            p_data = p_head_map->data;
            for( i=0; i<akt_byte; i++)
            {
                rtdm_printk(" %02x", p_data[i]);
            }
        }
        rtdm_printk("\n");
        pos++;
    }
}
*/

// ****************************************************************************
//
//     internal context LIST functions
//
//    -> all functions have to be called while holding context lock
//
// ****************************************************************************


static inline void _ctx_list_add(timsCtx *p_ctx)
{
    list_add_tail(&p_ctx->ctx_list, &td.ctx_list);
}


static inline timsCtx* _ctx_list_get(unsigned int mbxAddress)
{
    timsCtx*            p_ctx  = NULL;
    struct list_head*    p_list = NULL;

    p_list = td.ctx_list.next; // first element

    while (p_list != &td.ctx_list)
    {
        p_ctx = list_entry(p_list, timsCtx, ctx_list);

        if (p_ctx->sock.id == mbxAddress)
            return p_ctx;

        p_list = p_list->next;
    }
    return NULL;
}


static inline void _ctx_list_remove(timsCtx *p_ctx)
{
    list_del_init(&p_ctx->ctx_list);
}


// ****************************************************************************
//
//     internal context CACHE functions
//
//    -> all functions have to be called while holding context lock
//
// ****************************************************************************

//TODO Context hash table


static inline void _cache_add(timsCtx *p_ctx)
{
    int               i, old_idx  = 0;
    unsigned int    old_adr     = 0;
    uint64_t        now         = rtdm_clock_read();
    uint64_t        oldest_time = now;

    for (i=0; i< CONFIG_TIMS_CTX_CACHE_SIZE; i++)
    {
        if (!td.ctxCache[i].p_ctx)
        {
            td.ctxCache[i].p_ctx     = p_ctx;
            td.ctxCache[i].mbxAdr    = p_ctx->p_mbx->address;
            td.ctxCache[i].timestamp = now;

            tims_info("[CACHE] insert context %x in slot[%d] \n",
                      p_ctx->p_mbx->address, i);
            return;
        }
        else
        {
            if (td.ctxCache[i].timestamp < oldest_time)
            {
                old_idx     = i;
                old_adr     = td.ctxCache[i].mbxAdr;
                oldest_time = td.ctxCache[i].timestamp;
            }
        }
    }

    // cache is full -> replace oldest entry

    td.ctxCache[old_idx].p_ctx     = p_ctx;
    td.ctxCache[old_idx].mbxAdr    = p_ctx->p_mbx->address;
    td.ctxCache[old_idx].timestamp = now;

    tims_info("[CACHE] replace old context %x in slot[%d] with %x \n",
              p_ctx->p_mbx->address, old_idx, old_adr);
}


static inline timsCtx* _cache_get(unsigned int mbxAdr)
{
    int         i = 0;
    timsCtx*    p_ctx = NULL;

    for (i=0; i< CONFIG_TIMS_CTX_CACHE_SIZE; i++)
    {
        if (mbxAdr == td.ctxCache[i].mbxAdr)
        {
            p_ctx = td.ctxCache[i].p_ctx;
            return p_ctx;
        }
    }

    p_ctx = _ctx_list_get(mbxAdr);
    if (!p_ctx)
        return NULL;

    // context found in list -> add it to cache now
    _cache_add(p_ctx);

    return p_ctx;
}


static inline void _cache_remove(timsCtx *p_ctx)
{
    int i;

    for (i=0; i< CONFIG_TIMS_CTX_CACHE_SIZE; i++)
    {
        if (p_ctx == td.ctxCache[i].p_ctx)
        {
            memset(&td.ctxCache[i], 0, sizeof(timsCtxCache));
            tims_info("[CACHE] Remove context %x in slot[%d] \n",
                      p_ctx->p_mbx->address, i);
            return;
        }
    }
}


// ****************************************************************************
//
//     context functions
//
// ****************************************************************************


static inline void tims_ctx_remove(timsCtx *p_ctx)
{
    rtdm_lockctx_t lock_ctx;
    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx);

    _cache_remove(p_ctx);
    _ctx_list_remove(p_ctx);

    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
}


static inline void tims_ctx_add(timsCtx *p_ctx)
{
    rtdm_lockctx_t lock_ctx;
    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx);

    _ctx_list_add(p_ctx);

    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
}


static inline int tims_ctx_put(timsCtx *ctx)
{
    rtdm_lockctx_t lock_ctx;
    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx);

    ctx->use_counter--;

    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
    return 0;
}


static inline timsCtx* tims_ctx_get_rtdm(struct rtdm_dev_context *context)
{
    timsCtx* ctx;
    rtdm_lockctx_t lock_ctx;

    // don't return context while module shutdown
    if (test_bit(TIMS_STATE_BIT_SHUTDOWN, &td.state_flags))
    {
        tims_error("Tims shutdown\n");
        return NULL;
    }

    // do not get context while closing the mailbox
    if (test_bit(RTDM_CLOSING, &context->context_flags))
    {
        tims_error("Mbx closing\n");
        return NULL;
    }

    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx);

    ctx = (timsCtx*)context->dev_private;
    ctx->use_counter++;

    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);

    return ctx;
}


static inline timsCtx* tims_ctx_get(uint32_t mbxaddr)
{
    timsCtx* ctx;
    rtdm_lockctx_t lock_ctx;

    // don't return context while module shutdown
    if (test_bit(TIMS_STATE_BIT_SHUTDOWN, &td.state_flags))
    {
        tims_error("Tims shutdown\n");
        return NULL;
    }

    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx); // context lock

    // looking for context pointer in cache (and list)
    ctx = _cache_get(mbxaddr);
    if (!ctx)    // not found
    {
        rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
        return NULL;
    }

    if (test_bit(TIMS_CTX_BIT_CLOSING, &ctx->flags))
    {
        tims_error("Mbx closing\n");
        return NULL;
    }

    ctx->use_counter++;
    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
    return ctx;
}


// ****************************************************************************
//
//     mailbox list functions
//
// ****************************************************************************


// add slot in read list (ordered by priority)
// messages with the same priority ordered by the receive time
// (newest message at the end !!!)
static void _move_write_to_read(timsMbx *p_mbx, timsMbxSlot *write_slot)
{
    timsMbxSlot*    p_slot     = NULL;
    timsMsgHead*    p_head     = NULL;
    timsMsgHead*    p_head_new = (timsMsgHead *)write_slot->p_head_map;
    int             pos        = 1;
    struct list_head* p_list   = p_mbx->read_list.next; // first element

    while (p_list != &p_mbx->read_list)
    {
        p_slot = list_entry(p_list, timsMbxSlot, mbx_list);
        p_head = (void *)p_slot->p_head_map;

        if (p_head->priority < p_head_new->priority)
        {
            break;
        }
        p_list = p_list->next;
        pos++;
    }

    list_move(&write_slot->mbx_list, p_list->prev);
    p_mbx->slot_state.write--;
    p_mbx->slot_state.read++;
}


static void _move_write_to_free(timsMbx *p_mbx, timsMbxSlot *write_slot)
{
    list_move_tail(&write_slot->mbx_list, &p_mbx->free_list);
    p_mbx->slot_state.write--;
    p_mbx->slot_state.free++;
}


static void _move_read_to_free(timsMbx *p_mbx, timsMbxSlot *read_slot)
{
    if (list_empty(&p_mbx->read_list))
        return;

    list_move_tail(&read_slot->mbx_list, &p_mbx->free_list);
    p_mbx->slot_state.read--;
    p_mbx->slot_state.free++;

    // IMPORTANT: decrement semaphore counter
    rtdm_sem_timeddown(&p_mbx->readSem, TIMS_NONBLOCK, NULL);
    return;
}


static timsMbxSlot * _move_free_to_write(timsMbx *p_mbx, __s8 prio_new)
{
    timsMbxSlot         *free_slot = NULL;
    __s8                prio_old;
    timsMbxSlot         *read_slot;

    if (list_empty(&p_mbx->free_list)) {
        /* Check if a message with lower or equal priority can be dropped. */

        if (list_empty(&p_mbx->read_list))
            /* No msg in read list */
            return NULL;

        free_slot = list_entry(p_mbx->read_list.prev, timsMbxSlot, mbx_list);
        prio_old = ((timsMsgHead *)free_slot->p_head_map)->priority;
        if (prio_old > prio_new)
            /* Prio of new msg ist lower than lowest in read list -> drop */
            return NULL;

        /* Get oldest slot with lowest priority. */
        while (free_slot->mbx_list.prev != &p_mbx->read_list) {
            read_slot = list_entry(free_slot->mbx_list.prev, timsMbxSlot,
                                                             mbx_list);
            if (((timsMsgHead *)read_slot->p_head_map)->priority != prio_old)
                break;
            free_slot = read_slot;
        }

        _move_read_to_free(p_mbx, free_slot);
        tims_info("Oldest msg with prio %d dropped in favour of new msg "
                  "with prio %d.\n", prio_old, prio_new);
    } else
        free_slot = list_entry(p_mbx->free_list.next, timsMbxSlot, mbx_list);

    list_move_tail(&free_slot->mbx_list, &p_mbx->write_list);
    p_mbx->slot_state.free--;
    p_mbx->slot_state.write++;

    return free_slot;
}

static timsMbxSlot* _move_read_to_peek(timsMbx *p_mbx)
{
    timsMbxSlot *read_slot = NULL;

    if (list_empty(&p_mbx->read_list))
        return NULL;

    if (p_mbx->p_peek)
          return NULL;

    // get slot with highest priority (first entry)
    read_slot = list_entry(p_mbx->read_list.next, timsMbxSlot, mbx_list);

    p_mbx->p_peek = read_slot;

    list_del_init(&read_slot->mbx_list);
    p_mbx->slot_state.read--;
    p_mbx->slot_state.peek++;

    return read_slot;
}


static void _move_peek_to_free(timsMbx *p_mbx)
{
    list_add_tail(&p_mbx->p_peek->mbx_list, &p_mbx->free_list);
    p_mbx->p_peek = NULL;

    p_mbx->slot_state.peek--;
    p_mbx->slot_state.free++;
}


// ****************************************************************************
//
//     slot functions
//
//    -> realtime and non realtime context
//
// ****************************************************************************


static timsMbxSlot* tims_get_write_slot(timsMbx *p_mbx, __s8 prio_new)
{
    timsMbxSlot *slot = NULL;
    rtdm_lockctx_t lock_ctx;

    rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);
    slot = _move_free_to_write(p_mbx, prio_new);
    rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);

    if (!slot)
        return NULL;

    tims_dbgdetail("Get write slot 0x%p (0x%lx) in mailbox %08x\n",
                    slot->p_head, slot->p_head_map, p_mbx->address);

    return slot;
}


static void tims_put_write_slot(timsMbx *p_mbx, timsMbxSlot *slot)
{
    rtdm_lockctx_t lock_ctx;

    if (!slot)
        return;

    tims_dbgdetail("Move write slot 0x%p (0x%lx) into read list in mailbox %08x\n",
                    slot->p_head, slot->p_head_map, p_mbx->address);

    rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);
    _move_write_to_read(p_mbx, slot);
    rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);

    rtdm_sem_up(&p_mbx->readSem);

    tims_dbgdetail("Wake up reader of mailbox %08x \n", p_mbx->address);
}


static void tims_put_write_slot_error(timsMbx *p_mbx, timsMbxSlot *slot)
{
    rtdm_lockctx_t lock_ctx;

    if (!slot)
        return;

    tims_dbgdetail("Cancel slot %lx in mailbox %08x\n",
                    slot->p_head_map, p_mbx->address);

    rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);
    _move_write_to_free(p_mbx, slot);
    rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);
}


static timsMbxSlot* tims_get_peek_slot(timsMbx *p_mbx)
{
    timsMbxSlot *slot = NULL;
    rtdm_lockctx_t lock_ctx;

    rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);
    slot = _move_read_to_peek(p_mbx);
    rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);

    return slot;
}


static void tims_put_peek_slot(timsMbx *p_mbx)
{
    rtdm_lockctx_t lock_ctx;
    rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);
    _move_peek_to_free(p_mbx);
    rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);
}


// ****************************************************************************
//
//     internal peek functions
//
//    -> !!! realtime context !!!
//
// ****************************************************************************


static int tims_peek_intern(timsMbx *p_mbx, timsMbxSlot **p_slot,
                            int64_t *p_timeout)
{
    int             ret  = 0;
    timsMbxSlot*    slot = NULL;

    // calculating timeout
    int64_t timeout_ns = p_mbx->timeout_ns; // from ioctl TIMS_RTIOC_TIMEOUT

    if (p_timeout) // from recvmsg()
        timeout_ns = *p_timeout;

    if (test_and_set_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags))
    {
        tims_error("Can't peek on mbx %x\n", p_mbx->address);
        return -EBUSY;
    }

    tims_dbgdetail("Waiting for msg in mbx %08x (%llu ns) ... \n",
                   p_mbx->address, timeout_ns);

    ret = rtdm_sem_timeddown(&p_mbx->readSem, timeout_ns, NULL);
    if (unlikely(ret))
    {
        switch (-ret)
        {
            case EWOULDBLOCK:
                tims_dbgdetail("Receive would block - no message in read list\n");
                break;

            case ETIMEDOUT:
                tims_dbgdetail("Receive timeout - no message in read list\n");
                break;

            default:
                tims_error("Unexpected wake up or can't sleep on read sem "
                           "of mailbox %08x, code = %d \n", p_mbx->address, ret);
        }

        clear_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags);
          return ret;
    }

    tims_dbgdetail("Incoming msg in mbx %08x ... \n", p_mbx->address);

    // waked up by mailbox writer -> new message(s) available in read_list

    // get slot with the most important message, remove the slot from the
    // read_list and save the slot pointer in p_mbx->p_peek

    //print_read_list(p_mbx);

    slot = tims_get_peek_slot(p_mbx);
    if (!slot)
    {
        clear_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags);
        tims_error("Waked up without new msg in mailbox %08x \n",
                    p_mbx->address);
        return -EFAULT;
    }

    clear_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags);

    *p_slot = slot;

    tims_dbgdetail("Get peek slot 0x%p (0x%lx) in mailbox %08x\n",
                   slot->p_head, slot->p_head_map, p_mbx->address);
    return 0;
}


static void tims_peek_end_intern(timsMbx *p_mbx)
{
    tims_dbgdetail("Put peek slot 0x%p (0x%lx) in mailbox %08x\n",
                   p_mbx->p_peek->p_head, p_mbx->p_peek->p_head_map,
                   p_mbx->address);

    tims_put_peek_slot(p_mbx);
}

//
// additional sendmsg, peek and recvmsg- functions
//

// realtime or non realtime context (xenomai task or linux)
static int tims_sendmsg_global(rtdm_user_info_t *user_info,
                               const struct msghdr *msg)
{
    int             i;
    int             ret;
    RT_PIPE_MSG*    sendMsg;
    void*           sendBuffer = NULL;
    timsMsgHead*    p_head = msg->msg_iov[0].iov_base;

#ifdef CONFIG_TIMS_USE_RTNET

    struct msghdr       rtnet_msg;
    struct sockaddr_in  dest_addr;

    for (i = 0; i < td.mbxRouteNum; i++)
    {
        if (td.mbxRoute[i].mbx == p_head->dest)
        {
            dest_addr.sin_family          = AF_INET;
            dest_addr.sin_port            = htons(TIMS_MSG_ROUTER_PORT);
            dest_addr.sin_addr.s_addr     = td.mbxRoute[i].ip;

            rtnet_msg.msg_name            = &dest_addr;
            rtnet_msg.msg_namelen         = sizeof(dest_addr);
            rtnet_msg.msg_iov             = msg->msg_iov;
            rtnet_msg.msg_iovlen          = msg->msg_iovlen;
            rtnet_msg.msg_control         = NULL;
            rtnet_msg.msg_controllen      = 0;
            rtnet_msg.msg_flags           = 0;

            ret = rtdm_sendmsg(td.rtnet_fd, &rtnet_msg, 0);
            if (ret < rtnet_msg.msg_namelen + p_head->msglen)
            {
                tims_error("%x -> %x: Can't forward message, "
                           "type %i, msglen %i. "
                           "rt_socket_sendto(), code = %d\n",
                           p_head->src, p_head->dest, p_head->type,
                           p_head->msglen, ret);

                return ret;
            }
            return 0;
        }
    }

#endif

    // no real-time route available -> forward to TCP-client
    sendMsg = rt_pipe_alloc(&td.pipeToClient, p_head->msglen);
    if (sendMsg == 0)
    {
        tims_error("Can't allocate pipe buffer (%u bytes), code = %d\n",
                   p_head->msglen, (int)sendMsg);
        return -ENOMEM;
    }

    sendBuffer = P_MSGPTR(sendMsg);

    // copy message data into sendbuffer
    for (i=0; i< msg->msg_iovlen; i++)
    {
        if (user_info) // source data in userspace (user->kernel)
        {
            ret = rtdm_read_user_ok(user_info, msg->msg_iov[i].iov_base,
                                    msg->msg_iov[i].iov_len);
            if (!ret)
            {
                tims_error("Source pointer %p is not in userspace\n",
                           msg->msg_iov[i].iov_base);
                goto send_error;
            }

            ret = rtdm_copy_from_user(user_info, sendBuffer,
                                      msg->msg_iov[i].iov_base,
                                      msg->msg_iov[i].iov_len);
            if (ret)
            {
                tims_error("Can't copy %d bytes from userspace pointer %p, "
                           "code = %d\n", msg->msg_iov[i].iov_len,
                           msg->msg_iov[i].iov_base, ret);
                goto send_error;
            }

            sendBuffer += msg->msg_iov[i].iov_len;
        }
        else    // source data in kernelspace (kernel->kernel)
        {
            memcpy(sendBuffer, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
            sendBuffer += msg->msg_iov[i].iov_len;
        }
    }

    ret = rt_pipe_send(&td.pipeToClient, sendMsg, p_head->msglen, P_NORMAL);
    if ( ret != p_head->msglen)
    {
        tims_error("Can't forward msg from %x to %x type %i msglen %i \n",
                   p_head->src, p_head->dest, p_head->type, p_head->msglen);
        goto send_error;
    }

    return p_head->msglen;

send_error:
    rt_pipe_free(&td.pipeToClient, sendMsg);
    return ret;
}


static int register_mbx_tcp(unsigned int mbxAdr)
{
    int ret = 0;
    unsigned int cpysize = sizeof(timsMsgRouter_MbxMsg);
    timsMsgRouter_MbxMsg mbxMsg;

    tims_fillhead(&mbxMsg.head, TIMS_MSG_ROUTER_MBX_INIT,
                  0, 0, 0, 0, 0, cpysize);

    mbxMsg.mbx = mbxAdr;

    ret = rt_pipe_write(&td.pipeToClient, &mbxMsg, cpysize, P_NORMAL);
    if ( ret != cpysize)
    {
        tims_error("ERROR: can't register mailbox %x @ TcpTimsMsgRouter, "
                   "code = %d\n",mbxAdr, ret);
        return ret;
    }
    return 0;
}

static int unregister_mbx_tcp(unsigned int mbxAdr)
{
    int ret = 0;
    unsigned int cpysize = sizeof(timsMsgRouter_MbxMsg);
    timsMsgRouter_MbxMsg mbxMsg;

    tims_fillhead(&mbxMsg.head, TIMS_MSG_ROUTER_MBX_DELETE,
                  0, 0, 0, 0, 0, cpysize);

    mbxMsg.mbx = mbxAdr;

    ret = rt_pipe_write(&td.pipeToClient, &mbxMsg, cpysize, P_NORMAL);
    if ( ret != cpysize)
    {
        tims_error("ERROR: can't unregister mailbox %x @ TcpTimsMsgRouter, code = %d\n",mbxAdr, ret);
        return ret;
    }
    return 0;
}

//
// waiting functions
//

// realtime or non realtime context (xenomai task or linux)
static inline int tims_wait_for_writers(timsCtx *p_ctx)
{
    int             listempty = 0;
    int             try = 100;
    rtdm_lockctx_t  lock_ctx;
    timsMbx*        p_mbx = p_ctx->p_mbx;

    while (!listempty)
    {
        // get list_lock
        rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);

        // looking for messages in write_list
        if (list_empty(&p_mbx->write_list))
        {
            listempty = 1;
        }

        // put list_lock
        rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);

        // if !list_empty -> sleep some ms
        if (!listempty && try--)
        {
            if (rtdm_in_rt_context())  // realtime context
            {
                rtdm_task_sleep(1000000); // 1ms
            }
            else // non realtime context
            {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(HZ/10);
            }
        }

        if (!try)
        {
            return -2;
        }
    }
    return 0;
}

// realtime or non realtime context (xenomai task or linux)
static inline int tims_wake_up_reader(timsCtx *p_ctx)
{
    int             loops = 10;
    int             peek = 1;
    int             loop = loops;
    rtdm_lockctx_t  lock_ctx;
    timsMbx*        p_mbx = p_ctx->p_mbx;

    // wake up reader, if he is still waiting for a new message

    if (test_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags))
    {
        tims_dbgdetail("Wake up reader in mailbox %x\n", p_mbx->address);
        rtdm_sem_up(&p_mbx->readSem);

        while (test_bit(TIMS_MBX_BIT_READERWAIT, &p_mbx->flags) && loop--)
        {
            if (rtdm_in_rt_context())  // realtime context
            {
                rtdm_task_sleep(1000000); // 1ms
            }
            else // non realtime context
            {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(HZ/10);
            }
        }
        return 0;
    }

    // waiting for peek end

    loop = loops;

    while (peek && loop--)
    {
        rtdm_lock_get_irqsave(&p_mbx->list_lock, lock_ctx);

        if (!p_mbx->p_peek)
            peek = 0;

        rtdm_lock_put_irqrestore(&p_mbx->list_lock, lock_ctx);

        if (peek)
        {
            if (rtdm_in_rt_context())  // realtime context
            {
                rtdm_task_sleep(1000000); // 1ms
            }
            else // non realtime context
            {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(HZ/10);
            }
        }
    }

    if (!loop)
        return -1;

    return 0;
}

//
// local init functions
//

// non realtime context (init - linux)
static void init_context(timsCtx *p_ctx)
{
    memset(p_ctx, 0, sizeof(timsCtx) );
}

// non realtime context (init - linux)
static int init_fifo_mailbox(timsMbx *p_mbx)
{
  // allocate memory for fifo slots
  p_mbx->slot = (timsMbxSlot *)rtdm_malloc( sizeof(timsMbxSlot) );
  if (!p_mbx->slot) {
    return -ENOMEM;
  }
  tims_dbgdetail("Fifo mbx msg pointer created @ %p (%d byte) \n",
                p_mbx->slot, sizeof(timsMbxSlot));

  // create message buffer
  if (!p_mbx->buffer) { // create kernel buffer

    p_mbx->slot->p_head = (timsMsgHead *)rtdm_malloc(TIMS_STD_FIFOMBX_SIZE);
    if (!p_mbx->slot->p_head) {
      rtdm_free(p_mbx->slot);
      return -ENOMEM;
    }

    tims_dbgdetail("fifo mailbox buffer created @ %p (%d byte) \n",
                p_mbx->slot->p_head, TIMS_STD_FIFOMBX_SIZE);

  } else { // use another buffer

    p_mbx->slot->p_head = p_mbx->buffer;

    tims_dbgdetail("Use fifo mailbox buffer @ %p, %s (%d byte) \n",
                   p_mbx->buffer,
                   test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags) ?
                   "userspace" : "kernelspace", p_mbx->buffer_size);

  }

  set_bit(TIMS_MBX_BIT_FIFO, &p_mbx->flags);
  return 0;
}

static inline int get_max_pages(unsigned long buffersize)
{
    return (((buffersize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
            >> PAGE_SHIFT) + 1;
}

// non realtime context (init - linux)
static int init_slot_mailbox(timsMbx *p_mbx)
{
      void*             p_buffer            = NULL;
      unsigned long     p_buffer_map        = 0;
      unsigned long     buffer_page         = 0;
      unsigned long     buffer_offset        = 0;

      void*             p_head                 = NULL;
      unsigned long     p_head_map             = 0;    // mapped

      unsigned long     message_size           = p_mbx->msg_size;
      unsigned long     free_bytes_in_page    = 0;
      unsigned long     akt_bytes              = 0;
      unsigned long     slot_pages             = 0;

      int i       = 0;
      int si      = 0;   // slot index
      int pi      = 0;   // page index
      int ret     = 0;


      // allocate memory for message pointers
      p_mbx->slot = (timsMbxSlot *)rtdm_malloc(p_mbx->slot_count *
                                               sizeof(timsMbxSlot));
      if (!p_mbx->slot)
      {
        ret = -ENOMEM;
        goto init_error;
      }

      tims_dbgdetail("slot mailbox Entr%s created @ %p "
                   "(%d slots, %d bytes/slot, %d byte) \n",
                   p_mbx->slot_count > 0 ? "ies" : "y", p_mbx->slot,
                   p_mbx->slot_count, sizeof(timsMbxSlot),
                     p_mbx->slot_count * sizeof(timsMbxSlot));

      if (!p_mbx->buffer) // create kernel buffer
      {
           p_buffer = rtdm_malloc(p_mbx->slot_count * p_mbx->msg_size);
        if (!p_buffer)
        {
              ret = -ENOMEM;
              goto init_error;
        }

        tims_dbgdetail("slot mailbox buffer created @ %p "
                       "(%u slots, %u byte/slot, %d byte) \n",
                       p_buffer, p_mbx->slot_count, p_mbx->msg_size,
                       p_mbx->slot_count * p_mbx->msg_size);

      }
      else // use another buffer
      {
        p_buffer = p_mbx->buffer; // external kernel or userspace buffer

        tims_dbgdetail("use slot mailbox buffer @ %p "
                       "(%u slots, %u byte/slot, %d byte, %s) \n",
                       p_buffer, p_mbx->slot_count, p_mbx->msg_size,
                       p_mbx->slot_count * p_mbx->msg_size,
                       test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags) ?
                       "userspace" : "kernelspace");
      }

      if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags))
      {
          //
        // allocate mem for page table and mapInfo
        //

        // get number of needed pages
        p_mbx->buffer_pages = get_max_pages(p_mbx->buffer_size);

        p_mbx->buffer_pages = (unsigned long)
            (((p_mbx->buffer_size + PAGE_SIZE - 1) &
            ~(PAGE_SIZE - 1)) >> PAGE_SHIFT) + 1;

        tims_dbgdetail("need to map %lu pages \n", p_mbx->buffer_pages);

        // create table of all page pointers
        p_mbx->pp_pages = (struct page **)rtdm_malloc(sizeof(struct page*) *
                                                      p_mbx->buffer_pages);
        if (!p_mbx->pp_pages)
        {
              ret = -ENOMEM;
              goto init_error;
        }
        tims_dbgdetail("page list created @ %p (%lu entries) \n",
                       p_mbx->pp_pages, p_mbx->buffer_pages);

        // create list of all mapInfo
        p_mbx->p_mapInfo = (timsMapInfo_t *)rtdm_malloc(sizeof(timsMapInfo_t) *
                                                        p_mbx->buffer_pages);
        if (!p_mbx->p_mapInfo)
        {
              ret = -ENOMEM;
              goto init_error;
        }
        tims_dbgdetail("mapInfo list created @ %p (%lu entries) \n",
                       p_mbx->p_mapInfo, p_mbx->buffer_pages);

        // init tables
        for (i=0; i< p_mbx->buffer_pages; i++)
        {
              p_mbx->pp_pages[i]             = NULL;
              p_mbx->p_mapInfo[i].virtual = 0;
              p_mbx->p_mapInfo[i].mapped     = 0;
        }

          //
        // get mailbox pages and map them to kernel
        //

        buffer_page = ((unsigned long)p_buffer & ~(PAGE_SIZE - 1));
        buffer_offset = (unsigned long)p_buffer - buffer_page;

        tims_dbgdetail("buffer: 0x%p, buffer_page: 0x%lx, buffer_offset: 0x%lx, "
                       "pages: 0x%lx \n", p_buffer, buffer_page, buffer_offset,
                       p_mbx->buffer_pages);

        // get user pages
        down_read(&current->mm->mmap_sem);
        ret = get_user_pages(current,             // Task*
                             current->mm,         // memory map
                             buffer_page,         // start pointer (page aligned)
                             p_mbx->buffer_pages, // len
                             1,                   // write access
                             0,                   // force
                             p_mbx->pp_pages,      // save here all pages
                             NULL);

        up_read(&current->mm->mmap_sem);
        if (!ret)
        {
              tims_error("ERROR: no page returned :-( \n");
              ret = -EFAULT;
              goto init_error;
        }

        if (ret < 0)
        {
              tims_error("ERROR: get_user_pages, code = %d \n", ret);
              ret = -EFAULT;
              goto init_error;
        }
        tims_dbgdetail("%d/%lu pages returned \n", ret, p_mbx->buffer_pages);

          // map pages to kernel (kmap)
        for (i=0; i<ret; i++)
        {
              p_buffer_map = (unsigned long)kmap(p_mbx->pp_pages[i]);
              if (!p_buffer_map)
              {
                ret = -EFAULT;
                goto init_error;
              }

              p_mbx->p_mapInfo[i].virtual = p_buffer_map;
              p_mbx->p_mapInfo[i].mapped  = 1;

              p_mbx->buffer_pages = i + 1;

              tims_dbgdetail("page %03d: index: %lu, flags: %lu, "
                             "mapped to %lx\n", i, p_mbx->pp_pages[i]->index,
                             p_mbx->pp_pages[i]->flags, p_buffer_map);

        }

        // init p_buffer
        p_buffer_map = p_mbx->p_mapInfo[0].virtual + buffer_offset;
    }

      p_head     = p_buffer;
      p_head_map = p_buffer_map;  // mapped

    while (si < p_mbx->slot_count)
      {
        p_mbx->slot[si].p_head = p_head;
        p_mbx->slot[si].p_mbx  = p_mbx;

        if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags))
        {
              p_mbx->slot[si].p_head_map = p_head_map;  // start at p_head_map / p_head
              p_mbx->slot[si].map_idx    = pi;

              slot_pages = ((((p_head_map & ~PAGE_MASK) + p_mbx->msg_size) &
                           PAGE_MASK) >> PAGE_SHIFT);

              message_size  = p_mbx->msg_size;

              if (slot_pages) // more than one page for this message slot
              {
                while (message_size > 0)
                {
                      free_bytes_in_page = (p_head_map & PAGE_MASK) +
                                            PAGE_SIZE - p_head_map;

                      akt_bytes = message_size > free_bytes_in_page ?
                                  free_bytes_in_page : message_size;
                      message_size -= akt_bytes;
                      memset((void *)p_head_map, 0, akt_bytes);

                      if ( ((p_head_map + akt_bytes -1 ) & ~PAGE_MASK) ==
                           ~PAGE_MASK)   // ending with 0xFFF
                      {
                        pi++;    // next page
                        p_head_map = p_mbx->p_mapInfo[pi].virtual;
                        free_bytes_in_page = PAGE_SIZE -
                                             (message_size - akt_bytes);
                      }
                      else
                      {
                        p_head_map         += akt_bytes;
                        free_bytes_in_page -= akt_bytes;
                       }

                      p_head += akt_bytes;

                }

              }
              else   // only one page in message slot
            {
                akt_bytes = p_mbx->msg_size;
                message_size -= akt_bytes;

                if (((p_head_map + akt_bytes -1 ) & ~PAGE_MASK) == ~PAGE_MASK)
                {
                    // ending with 0xFFF
                      pi++;    // next page
                      p_head_map = p_mbx->p_mapInfo[pi].virtual;
                }
                else
                {
                      p_head_map += akt_bytes;
                }

                p_head += akt_bytes;
              }

        }
        else // !(TIMS_MBX_BIT_USRSPCBUFFER)
        {
              p_mbx->slot[si].p_head_map = (unsigned long)p_mbx->slot[si].p_head;
              p_mbx->slot[si].map_idx    = 0;

              akt_bytes = p_mbx->msg_size;
              message_size -= akt_bytes;

              memset(p_head, 0, akt_bytes);
              p_head += akt_bytes;
        }

        INIT_LIST_HEAD(&p_mbx->slot[si].mbx_list);

        tims_dbgdetail("slot[%d]: user: 0x%p, mapped: 0x%lx (%d bytes) \n", si,
                       p_mbx->slot[si].p_head, p_mbx->slot[si].p_head_map,
                       p_mbx->msg_size);

        // add slot to free_list
        list_add_tail(&p_mbx->slot[si].mbx_list, &p_mbx->free_list);
        si++;
      }

      set_bit(TIMS_MBX_BIT_SLOT, &p_mbx->flags);
      return 0;

init_error:

    if (p_mbx->buffer_pages)
    {
        for (i=0; i<p_mbx->buffer_pages; i++)
        {
              if (p_mbx->p_mapInfo[i].mapped)
                kunmap(p_mbx->pp_pages[i]);
        }
      }

      if (p_mbx->p_mapInfo)
        rtdm_free(p_mbx->p_mapInfo);

      if (p_mbx->pp_pages)
        rtdm_free(p_mbx->pp_pages);

      if (!p_mbx->buffer)
        rtdm_free(p_buffer);

      if (p_mbx->slot)
        rtdm_free(p_mbx->slot);

      return ret;
}

// non realtime context (init - linux)
static int init_mailbox(timsCtx *p_ctx, struct tims_mbx_cfg *p_cfg,
                        rtdm_user_info_t *user_info)
{
  int ret = 0;
  timsMbx *p_mbx = NULL;

  // check values
  if (p_cfg->msg_size == 0) { // no message size
    return -EINVAL;
  }

  // check slot mailbox size
  if (p_cfg->buffer &&      // local buffer
      p_cfg->slot_count &&  // slot mailbox
      p_cfg->buffer_size < (p_cfg->slot_count * p_cfg->msg_size)) {
    tims_error("slot-mailbox buffer too small (min %d byte)\n",
                 p_cfg->slot_count * p_cfg->msg_size);
    return -EINVAL;
  }

  // check fifo mailbox size
  if (p_cfg->buffer &&      // local buffer
      !p_cfg->slot_count && // fifo mailbox
      p_cfg->buffer_size < TIMS_MIN_FIFOMBX_SIZE) {
    tims_error("fifo-mailbox buffer too small (min %d byte)\n",
                 TIMS_MIN_FIFOMBX_SIZE);
    return -EINVAL;
  }

  // end of checks -> begin init

  set_bit(TIMS_CTX_BIT_INITMBX, &p_ctx->flags);

  // create tims mailbox
  p_mbx = (timsMbx *)rtdm_malloc(sizeof(timsMbx));
  if (!p_mbx) {
    ret = -ENOMEM;
    goto init_error;
  }
  p_ctx->p_mbx = p_mbx;
  tims_dbgdetail("mailbox created @ %p (%d byte) \n",
              p_mbx, sizeof(timsMbx));

  memset(p_mbx, 0, sizeof(timsMbx));

  // save config values
  p_mbx->slot_count    = p_cfg->slot_count;
  p_mbx->msg_size      = p_cfg->msg_size;
  p_mbx->buffer        = p_cfg->buffer;
  p_mbx->buffer_size   = p_cfg->buffer_size;

  p_mbx->flags   = 0;
  if (p_mbx->buffer) {
    set_bit(TIMS_MBX_BIT_EXTERNBUFFER, &p_mbx->flags);

    if (user_info) {
      set_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags);
    }
  }

  // init values
  rtdm_lock_init(&p_mbx->list_lock);
  INIT_LIST_HEAD(&p_mbx->free_list);
  INIT_LIST_HEAD(&p_mbx->write_list);
  INIT_LIST_HEAD(&p_mbx->read_list);

  rtdm_sem_init(&p_mbx->readSem, 0);

  p_mbx->timeout_ns = TM_INFINITE; // blocking peek by default

  // create slot or fifo mailbox
  if (p_mbx->slot_count == 0) { // fifo mailbox
    ret = init_fifo_mailbox(p_mbx);
  } else {                      // slot mailbox
    ret = init_slot_mailbox(p_mbx);

    p_mbx->slot_state.free  = p_mbx->slot_count;
    p_mbx->slot_state.write = 0;
    p_mbx->slot_state.read  = 0;
    p_mbx->slot_state.peek  = 0;

  }
  if (ret) {
    goto init_error;
  }

  td.mbx_num++;

  p_mbx->address = p_ctx->sock.id;

  set_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags);
  clear_bit(TIMS_CTX_BIT_INITMBX, &p_ctx->flags);

  // register mailbox @ tcp router
  register_mbx_tcp(p_mbx->address);

  tims_print("new mailbox %x created \n", p_mbx->address);

  return 0;

init_error:

  if (p_mbx) {
    rtdm_free(p_mbx);
  }

  clear_bit(TIMS_CTX_BIT_INITMBX, &p_ctx->flags);

  return ret;
}

// non realtime context (init - linux)
static void destroy_mailbox(timsCtx *p_ctx)
{
  timsMbx *p_mbx = NULL;
  if (!p_ctx) {
    return;
  }

  if (!test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags)) {
    tims_error("no mailbox available to free\n");
    return;
  }

  p_mbx = p_ctx->p_mbx;

  tims_dbgdetail("delete mailbox %x\n", p_mbx->address);

  if (!test_bit(TIMS_MBX_BIT_EXTERNBUFFER, &p_mbx->flags)) {
    tims_dbgdetail("delete mailbox buffer @ 0x%p\n", p_mbx->slot[0].p_head);
    rtdm_free(p_mbx->slot[0].p_head);
  } else {
    tims_dbgdetail("no mailbox buffer available to free\n");
  }

  if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_mbx->flags)) {

    int i;
    if (p_mbx->buffer_pages) {
      for (i=0; i<p_mbx->buffer_pages; i++) {
        if (p_mbx->p_mapInfo[i].mapped) {
          tims_dbgdetail("Unmap page %03d @ %lx\n",
                      i, p_mbx->p_mapInfo[i].virtual);
          kunmap(p_mbx->pp_pages[i]);
        }
      }
    }

    tims_dbgdetail("Delete page table @ 0x%p\n", p_mbx->pp_pages);
    rtdm_free(p_mbx->pp_pages);

    tims_dbgdetail("Delete timsMapInfo(s) @ 0x%p\n", p_mbx->p_mapInfo);
    rtdm_free(p_mbx->p_mapInfo);
  }

  tims_dbgdetail("Delete msg pointer(s) @ 0x%p\n", p_mbx->slot);
  rtdm_free(p_mbx->slot);

  tims_dbgdetail("Delete semaphore \n");
  rtdm_sem_destroy(&p_mbx->readSem);

  tims_dbgdetail("Delete tims-mailbox @ 0x%p\n", p_mbx);
  rtdm_free(p_mbx);

  td.mbx_num--;

  clear_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags);

  // unregister mailbox @ tcp router
  unregister_mbx_tcp(p_mbx->address);

  tims_print("Mbx %x deleted \n", p_mbx->address);

  return;
}

// realtime or non realtime context (xenomai task or linux)
static void tims_clean_mailbox(timsCtx *p_ctx)
{
    timsMbx*              p_mbx  = NULL;
    timsMbxSlot *       read_slot = NULL;
    struct list_head*   p_list = NULL;
    rtdm_lockctx_t      lock_ctx;
    int                 cleaned = 0;

    if (!p_ctx)
        return;

    /* get context lock
     * no write task can wake up the reader while deleting
     * the read list. If the read task has been waked up before
     * this lock, the reader will be blocked, too.
     */

    tims_dbgdetail("Cleaning mailbox %x ... \n", p_mbx->address);

    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx); // context lock
    p_mbx = p_ctx->p_mbx;

    p_list = p_mbx->read_list.next;
    while (p_list != &p_mbx->read_list)
    {
        read_slot = list_entry(p_list, timsMbxSlot, mbx_list);
        _move_read_to_free(p_mbx, read_slot);
        cleaned++;

        p_list = p_mbx->read_list.next;
    }

    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);

    tims_info("Mailbox %x has been cleaned (%d messages) \n",
              p_mbx->address, cleaned);

    return;
}


//
// copy functions
//

// realtime context
static unsigned long tims_copy_userslot_user(rtdm_user_info_t *user_info,
                                             timsMbxSlot *slot,
                                             const struct msghdr *msg)
{
  unsigned long akt_copy_size = 0;
  unsigned long copy_bytes    = 0;
  unsigned long bytes_copied  = 0;
  unsigned long bytes_in_page = 0;
  int i;

  unsigned long ret;
  unsigned long p_src_map     = slot->p_head_map;
  void*         p_src         = slot->p_head;
  void*         p_dest        = NULL;
  unsigned long src_page      = slot->map_idx;
  timsMsgHead*  p_head        = (timsMsgHead *)slot->p_head_map;
  unsigned long databytes     = p_head->msglen - TIMS_HEADLEN;

  for (i=0; i<2; i++) {

    if (i==0) {
      copy_bytes = TIMS_HEADLEN;
    } else {
      copy_bytes = databytes;
    }

    p_dest     = msg->msg_iov[i].iov_base;

    // check destination pointer
    ret = rtdm_rw_user_ok( user_info, p_dest, copy_bytes);
    if (!ret) {
      tims_error("ERROR: userspace destination 0x%p (%lu bytes) NOT OK \n",
                  p_dest, copy_bytes);
      return ret;
    }

    // copy data
    while (copy_bytes) {

      bytes_in_page = (p_src_map & PAGE_MASK) + PAGE_SIZE - p_src_map;
      akt_copy_size = copy_bytes > bytes_in_page ? bytes_in_page : copy_bytes ;

//      tims_dbginfo("copy userbuffer (0x%p, page: %lu, map: 0x%lx) --(%lu/%lu bytes)--> user (0x%p)\n",
//                   p_src, src_page, p_src_map, akt_copy_size, copy_bytes, p_dest);

      ret = rtdm_copy_to_user(user_info,
                              p_dest,
                              (void *)p_src_map,
                              akt_copy_size);
      if (ret) {
        if (ret < 0) {
          tims_error("ERROR while copy userbuffer -> user, code = %lu \n", ret);
        } else {
          tims_error("ERROR while copy userbuffer -> user, only %lu/%lu bytes were copied \n",
                    akt_copy_size - ret, akt_copy_size);
        }
        return ret;
      }

      bytes_in_page -= akt_copy_size;
      copy_bytes    -= akt_copy_size;
      p_src_map     += akt_copy_size;
      p_src         += akt_copy_size;
      p_dest        += akt_copy_size;
      bytes_copied  += akt_copy_size;

      if (!bytes_in_page) {
        src_page++;
        if (slot->p_mbx->p_mapInfo[src_page].mapped) {
          p_src_map = slot->p_mbx->p_mapInfo[src_page].virtual;
        } else {
          return -EFAULT;
        }
      }
    }
  }

  return 0;
}

// realtime context
static unsigned long tims_copy_kernelslot_user(rtdm_user_info_t *user_info,
                                               timsMbxSlot *slot,
                                               const struct msghdr *msg)
{
  void*         p_src      = slot->p_head;
  void*         p_dest     = NULL;
  int           veclen     = msg->msg_iovlen;
  unsigned long copy_bytes = 0;
  int           i          = 0;
  unsigned long ret        = 0;

  for (i=0; i<veclen; i++) {

    copy_bytes = msg->msg_iov[i].iov_len;
    p_dest     = msg->msg_iov[i].iov_base;

    // check destination pointer
    ret = rtdm_rw_user_ok( user_info, p_dest, copy_bytes);
    if (!ret) {
      tims_error("ERROR: userspace destination 0x%p (%lu bytes) NOT OK \n",
                  p_dest, copy_bytes);
      return ret;
    }

//    tims_dbginfo("copy kernelbuffer (0x%p) --(%lu bytes)--> user (0x%p)\n",
//                 p_src, copy_bytes, p_dest);

    // copy data
    ret = rtdm_copy_to_user(user_info, p_dest, p_src, copy_bytes);
    if (ret) {
      if (ret < 0) {
        tims_error("ERROR while copy kernelbuffer -> user, code = %lu \n", ret);
      } else {
        tims_error("ERROR while copy kernelbuffer -> user, only %lu/%lu bytes were copied \n",
                    copy_bytes - ret, copy_bytes);
      }
      return ret;
    }

    p_src += copy_bytes;

  }

  return 0;
}

// realtime context
static unsigned long tims_copy_userslot_kernel(rtdm_user_info_t *user_info,
                                               timsMbxSlot *slot,
                                               const struct msghdr *msg)
{
  unsigned long akt_copy_size = 0;
  unsigned long copy_bytes    = 0;
  unsigned long bytes_copied  = 0;
  unsigned long bytes_in_page = 0;
  int i;

  unsigned long p_src_map     = slot->p_head_map;
  void*         p_src         = slot->p_head;
  void*         p_dest        = NULL;
  unsigned long src_page      = slot->map_idx;

  int veclen   = msg->msg_iovlen;

  for (i=0; i<veclen; i++) {

    copy_bytes = msg->msg_iov[i].iov_len;
    p_dest     = msg->msg_iov[i].iov_base;

    // copy data
    while (copy_bytes) {

      bytes_in_page = (p_src_map & PAGE_MASK) + PAGE_SIZE - p_src_map;
      akt_copy_size = copy_bytes > bytes_in_page ? bytes_in_page : copy_bytes ;

//      tims_dbginfo("copy userbuffer (0x%p, page: %lu, map: 0x%lx) --(%lu/%lu bytes)--> kernel (0x%p)\n",
//                   p_src, src_page, p_src_map, akt_copy_size, copy_bytes, p_dest);

      memcpy( p_dest, (void *)p_src_map, akt_copy_size);

      bytes_in_page -= akt_copy_size;
      copy_bytes    -= akt_copy_size;
      p_src_map     += akt_copy_size;
      p_src         += akt_copy_size;
      p_dest        += akt_copy_size;
      bytes_copied  += akt_copy_size;

      if (!bytes_in_page) {
        src_page++;
        if (slot->p_mbx->p_mapInfo[src_page].mapped) {
          p_src_map = slot->p_mbx->p_mapInfo[src_page].virtual;
        } else {
          return -EFAULT;
        }
      }
    }
  }

  return 0;
}

// realtime context
static unsigned long tims_copy_kernelslot_kernel(rtdm_user_info_t *user_info,
                                                 timsMbxSlot *slot,
                                                 const struct msghdr *msg)
{
  unsigned long copy_bytes    = 0;
  int i;
  void*         p_src         = slot->p_head;
  void*         p_dest        = NULL;

  int veclen   = msg->msg_iovlen;

  for (i=0; i<veclen; i++) {

    copy_bytes = msg->msg_iov[i].iov_len;
    p_dest     = msg->msg_iov[i].iov_base;

//    tims_dbginfo("copy kernelbuffer (0x%p) --(%lu bytes)--> kernel (0x%p)\n",
//                 p_src, copy_bytes, p_dest);

    // copy data
    memcpy(p_dest, p_src, copy_bytes);

    p_src += copy_bytes;

  }
  return 0;
}


// realtime or non realtime context (xenomai task or linux)
static unsigned long tims_copy_user_userslot(rtdm_user_info_t *user_info,
                                             timsMbxSlot *slot,
                                             const struct msghdr *msg)
{
    unsigned long akt_copy_size = 0;
    unsigned long copy_bytes    = 0;
    unsigned long bytes_copied  = 0;
    unsigned long free_in_page  = 0;
    int i;

    unsigned long ret;
    unsigned long p_dest_map    = slot->p_head_map;
    void*         p_dest        = slot->p_head;
    void*         p_src         = NULL;
    unsigned long dest_page     = slot->map_idx;

    int veclen   = msg->msg_iovlen;
    for (i=0; i<veclen; i++)
    {
        copy_bytes = msg->msg_iov[i].iov_len;
        p_src      = msg->msg_iov[i].iov_base;

        // check source pointer
        ret = rtdm_read_user_ok(user_info, p_src, copy_bytes);
        if (!ret)
        {
            tims_error("Copy user -> user: user, src %p (%lu bytes) NOT OK \n",
                        p_src, copy_bytes);
        return -EINVAL;
        }

        // copy data
        while (copy_bytes)
        {
            free_in_page  = (p_dest_map & PAGE_MASK) + PAGE_SIZE - p_dest_map;
            akt_copy_size = free_in_page > copy_bytes ? copy_bytes : free_in_page;

            ret = rtdm_copy_from_user(user_info, (void *)p_dest_map, p_src,
                                      akt_copy_size);
            if (ret)
            {
                if (ret < 0)
                {
                    tims_error("Can't copy user -> user, code = %lu \n", ret);
                }
                else
                {
                    tims_error("Can't copy user -> user, only %lu/%lu bytes "
                               " have been copied \n", akt_copy_size - ret,
                               akt_copy_size);
                }
                return ret;
            }

            free_in_page -= akt_copy_size;
            copy_bytes   -= akt_copy_size;
            p_dest_map   += akt_copy_size;
            p_dest       += akt_copy_size;
            p_src        += akt_copy_size;
            bytes_copied += akt_copy_size;

            if (!free_in_page)
            {
                dest_page++;
                if (slot->p_mbx->p_mapInfo[dest_page].mapped)
                {
                    p_dest_map = slot->p_mbx->p_mapInfo[dest_page].virtual;
                }
                else
                {
                    return -EFAULT;
                }
            }
        }
    }
    return 0;
}

// realtime or non realtime context (xenomai task or linux)
static unsigned long tims_copy_kernel_userslot(rtdm_user_info_t *user_info,
                                               timsMbxSlot *slot,
                                               const struct msghdr *msg)
{
    unsigned long akt_copy_size = 0;
    unsigned long copy_bytes    = 0;
    unsigned long free_in_page  = 0;
    int i;

    unsigned long p_dest_map    = slot->p_head_map;
    void*         p_dest        = slot->p_head;
    void*         p_src         = NULL;
    unsigned long dest_page     = slot->map_idx;

    int veclen   = msg->msg_iovlen;
    for (i=0; i<veclen; i++)
    {
        copy_bytes = msg->msg_iov[i].iov_len;
        p_src      = msg->msg_iov[i].iov_base;

        // copy data
        while (copy_bytes)
        {
            free_in_page  = (p_dest_map & PAGE_MASK) + PAGE_SIZE - p_dest_map;
            akt_copy_size = free_in_page > copy_bytes ? copy_bytes : free_in_page;

            memcpy((void *)p_dest_map, p_src, akt_copy_size);

            free_in_page -= akt_copy_size;
            copy_bytes   -= akt_copy_size;
            p_dest_map   += akt_copy_size;
            p_dest       += akt_copy_size;
            p_src        += akt_copy_size;

            if (!free_in_page)
            {
                dest_page++;
                if (slot->p_mbx->p_mapInfo[dest_page].mapped)
                {
                    p_dest_map = slot->p_mbx->p_mapInfo[dest_page].virtual;
                }
                else
                {
                    return -EFAULT;
                }
            }
        }
    }
    return 0;
}

// realtime or non realtime context (xenomai task or linux)
static unsigned long tims_copy_user_kernelslot(rtdm_user_info_t *user_info,
                                               timsMbxSlot *slot,
                                               const struct msghdr *msg)
{
    unsigned long   copy_bytes  = 0;
    int             i           = 0;
    unsigned long   ret         = 0;
    void*           p_dest      = slot->p_head;
    void*           p_src       = NULL;
    int             veclen      = msg->msg_iovlen;

    for (i=0; i<veclen; i++)
    {
        copy_bytes = msg->msg_iov[i].iov_len;
        p_src      = msg->msg_iov[i].iov_base;

        // check source pointer
        ret = rtdm_read_user_ok(user_info, p_src, copy_bytes);
        if (!ret)
        {
            tims_error("Copy user -> kernel: user src %p (%lu bytes) NOT OK \n",
                       p_src, copy_bytes);
            return -EINVAL;
        }

        ret = rtdm_copy_from_user(user_info, p_dest, p_src, copy_bytes);
        if (ret)
        {
            if (ret < 0)
            {
                tims_error("Can't copy user -> kernel, code = %lu \n", ret);
            }
            else
            {
                tims_error("Can't copy user -> kernel, only %lu/%lu bytes "
                           "have been copied \n", copy_bytes - ret, copy_bytes);
            }
            return ret;
        }
        p_dest += copy_bytes;
    }
    return 0;
}

// realtime or non realtime context (xenomai task or linux)
static unsigned long tims_copy_kernel_kernelslot(rtdm_user_info_t *user_info,
                                                 timsMbxSlot *slot,
                                                 const struct msghdr *msg)
{
    unsigned long   copy_bytes  = 0;
    int             i           = 0;
    void*           p_dest      = slot->p_head;
    void*           p_src       = NULL;
    int             veclen      = msg->msg_iovlen;

    for (i=0; i<veclen; i++)
    {
        copy_bytes = msg->msg_iov[i].iov_len;
        p_src      = msg->msg_iov[i].iov_base;

        memcpy(p_dest, p_src, copy_bytes);

        p_dest += copy_bytes;
    }
    return 0;
}

// realtime or non realtime context (xenomai task or linux)
static int copy_msg_into_slot(rtdm_user_info_t *user_info, timsMbxSlot *slot,
                              const struct msghdr *msg, unsigned long mbxFlags)
{
    int ret = 0;

    if (user_info) // message sender is in userspace
    {
        if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &mbxFlags)) // copy user -> user
        {
            ret = tims_copy_user_userslot(user_info, slot, msg);
            if (ret)
            {
                return ret;
            }
        }
        else // copy user -> kernel
        {
            ret = tims_copy_user_kernelslot(user_info, slot, msg);
            if (ret)
            {
                return ret;
            }
        }
    }
    else // message sender is in kernelspace
    {
        if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &mbxFlags)) // copy kernel -> user
        {
            ret = tims_copy_kernel_userslot(user_info, slot, msg);
            if (ret)
            {
                return ret;
            }
        }
        else // copy kernel -> kernel
        {
            ret = tims_copy_kernel_kernelslot(user_info, slot, msg);
            if (ret)
            {
                return ret;
            }
        }
    }
    return 0;
}

// realtime context
static int copy_msg_out_slot(rtdm_user_info_t *user_info, timsMbxSlot *slot,
                             const struct msghdr *msg, unsigned long mbxFlags)
{
  int ret = 0;

  if (user_info) {

//    tims_dbginfo("message receiver is in userspace \n");

    if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &mbxFlags)) {

//      tims_dbginfo("copy userbuffer -> user \n");
      ret = tims_copy_userslot_user(user_info, slot, msg);
      if (ret) {
        return ret;
      }

    } else {

//      tims_dbginfo("copy kernelbuffer -> user \n");
      ret = tims_copy_kernelslot_user(user_info, slot, msg);
      if (ret) {
        return ret;
      }
    }

  } else {

    if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &mbxFlags)) {

//      tims_dbginfo("copy userbuffer -> kernel \n");
      ret = tims_copy_userslot_kernel(user_info, slot, msg);
      if (ret) {
        return ret;
      }

    } else {

//      tims_dbginfo("copy kernelbuffer -> kernel \n");
      ret = tims_copy_kernelslot_kernel(user_info, slot, msg);
      if (ret) {
        return ret;
      }
    }
  }
  return 0;
}

// ****************************************************************************
//
//     ioctl, sending and receive functions
//
//    -> userspace / kernel task (realtime context)
//
// ****************************************************************************


int rt_tims_ioctl(struct rtdm_dev_context *context,
                  rtdm_user_info_t *user_info,
                  int request,
                  void *arg)
{
    timsCtx*                p_ctx   = NULL;
    timsCtx*                p_ctx2  = NULL;
    struct tims_mbx_cfg*    p_tmc   = NULL;
    int                     ret     = 0;
    timsMsgHead**           p_head  = NULL;
    timsMbxSlot*             slot    = NULL;

    switch(request)
    {
        case _RTIOC_GETSOCKOPT:
        {
            // struct _rtdm_getsockopt_args *args =
            //                             (struct _rtdm_getsockopt_args *)arg;
            // tims_dbginfo("ioctl, _RTIOC_GETSOCKOPT \n");
            return -ENOTTY;
        }

        case _RTIOC_SETSOCKOPT:
        {
            // struct _rtdm_setsockopt_args *args =
            //                             (struct _rtdm_setsockopt_args *)arg;
            // tims_dbginfo("ioctl, _RTIOC_SETSOCKOPT \n");
            return -ENOTTY;
        }

        case _RTIOC_BIND:
        {
            struct _rtdm_setsockaddr_args *p_rtdmadr =
                                          (struct _rtdm_setsockaddr_args *)arg;
            struct tims_sockaddr *p_tsock =
                                       (struct tims_sockaddr *)p_rtdmadr->addr;

            if (!arg)
                return -EINVAL;

            // get context
            p_ctx = tims_ctx_get_rtdm(context);
            if (!p_ctx)
                return -ENODEV;

            if (test_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags))
            {
                tims_error("RTIOC_BIND -> socket already has been bound "
                           "to address %x\n", p_ctx->sock.id);
                tims_ctx_put(p_ctx);
                return -EINVAL;
            }

            p_ctx2 = tims_ctx_get(p_tsock->id);
            if (p_ctx2)
            {
                  tims_error("RTIOC_BIND -> address %x has been allocated "
                           "by another one\n", p_ctx->sock.id);

                tims_ctx_put(p_ctx2);
                tims_ctx_put(p_ctx);
                return -EBUSY;
            }

            tims_dbginfo("bind socket on id %x \n", p_tsock->id);

            p_ctx->sock.id        = p_tsock->id;
            p_ctx->sock.sa_family = p_tsock->sa_family;

            set_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags);

            tims_ctx_put(p_ctx);
            return 0;
        }

        case TIMS_RTIOC_MBXCFG:
        {
            if (rtdm_in_rt_context())
            {
                tims_error("Call TIMS_RTIOC_MBXCFG only in non realtime context !!!\n");
                return -EPERM;
            }

            if (!arg)
                return -EINVAL;

              // get context
            p_ctx = tims_ctx_get_rtdm(context);
            if (!p_ctx)
                return -ENODEV;

            if (test_bit(TIMS_CTX_BIT_INITMBX, &p_ctx->flags) ||
                test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
            {
                tims_ctx_put(p_ctx);
                return -EEXIST;
            }

            p_tmc = (struct tims_mbx_cfg *)arg;
            ret = init_mailbox(p_ctx, p_tmc, user_info);
            tims_ctx_put(p_ctx);
            return ret;
        }

        case TIMS_RTIOC_MBXCLEAN:
        {
              // get context
            p_ctx = tims_ctx_get_rtdm(context);
            if (!p_ctx)
                return -ENODEV;

            if (!test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
            {
                tims_ctx_put(p_ctx);
                return -ENODEV;
            }

            tims_clean_mailbox(p_ctx);
            tims_ctx_put(p_ctx);
            return 0;
        }

        case TIMS_RTIOC_RECVBEGIN:
        {
            ret = 0;

            if (!rtdm_in_rt_context())
            {
              tims_error("Call TIMS_RTIOC_RECVBEGIN only in realtime context!\n");
              return -EPERM;
            }

            if (!arg)
              return -EINVAL;

            p_head = (timsMsgHead **)arg;

              // get context
            p_ctx = tims_ctx_get_rtdm(context);
            if (!p_ctx)
                return -ENODEV;

            if (!test_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags) ||
                !test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
            {
                tims_error("Not bound or mailbox not created\n");
                tims_ctx_put(p_ctx);
                return -EFAULT;
            }

            if (user_info &&
                !test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_ctx->p_mbx->flags))
            {
                  tims_error("Mbx is in kernelspace. No peek allowed !!!\n");
                  tims_ctx_put(p_ctx);
                  return -EPERM;
            }

            ret = tims_peek_intern(p_ctx->p_mbx, &slot, NULL);
            if (ret)
            {
              tims_error("ioctl, TIMS_RTIOC_RECVBEGIN\n");
              tims_ctx_put(p_ctx);
              return ret;
            }

            *p_head = slot->p_head;

            // don't put context (it will be done in TIMS_RTIOC_RECVEND)
            return 0;
        }

        case TIMS_RTIOC_RECVEND:
        {
            if (!rtdm_in_rt_context())
            {
                tims_error("Call TIMS_RTIOC_RECVEND only "
                           "in realtime context !!!\n");
                  return -EPERM;
            }

            // don't inc use counter (done in TIMS_RTIOC_RECVBEGIN)
            p_ctx = (timsCtx*) context->dev_private;

            if (!test_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags) ||
                !test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
            {
                tims_error("Not bound or mbx not created\n");
                return -EFAULT;
            }

            if (!p_ctx->p_mbx->p_peek)
            {
                return -ENOSYS;
            }

            if (user_info &&
                !test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_ctx->p_mbx->flags))
            {
                tims_error("Mbx is in kernelspace. No peek allowed !!!\n");
                return -EPERM;
            }

            tims_peek_end_intern(p_ctx->p_mbx);
            tims_ctx_put(p_ctx);
            return 0;
        }

        case TIMS_RTIOC_TIMEOUT:
        {
            if (!arg)
                  return -EINVAL;

              // get context
            p_ctx = tims_ctx_get_rtdm(context);
            if (!p_ctx)
                return -ENODEV;

            if (!test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
            {
                tims_ctx_put(p_ctx);
                return -ENODEV;
            }

            p_ctx->p_mbx->timeout_ns = *(int64_t *)arg;

            tims_ctx_put(p_ctx);
            return 0;
        }

        default: {
            return -ENOTTY;
        }

      }
      return -ENOTTY;
}

// realtime context
ssize_t rt_tims_recvmsg(struct rtdm_dev_context *context,
                        rtdm_user_info_t *user_info,
                        struct msghdr *msg,
                        int flags)
{
    int ret = 0;
    timsMbxSlot*    slot       = NULL;
    timsMsgHead*    p_head     = NULL;
    timsMsgHead*    p_head_map = NULL;
    timsCtx*        p_ctx      = NULL;

    // get context
    p_ctx = tims_ctx_get_rtdm(context);
    if (!p_ctx)
        return -ENODEV;


    if (!test_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags) ||
        !test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
    {
        tims_error("Not bound or mbx not created\n");
        tims_ctx_put(p_ctx);
        return -EFAULT;
    }

    if (!msg || msg->msg_iovlen != 2|| !msg->msg_iov) { // head MUST be present
        tims_error("Invalid msg pointers\n");
        tims_ctx_put(p_ctx);
        return -EINVAL;
    }

    ret = tims_peek_intern(p_ctx->p_mbx, &slot, (int64_t *)msg->msg_control);
    if (ret)
    {
        tims_ctx_put(p_ctx);
        return ret;
    }

    p_head     = slot->p_head;
    p_head_map = (timsMsgHead *)slot->p_head_map;

    // return, if destination buffer is too small
    if ((msg->msg_iov[0].iov_len < TIMS_HEADLEN) ||
        (msg->msg_iov[1].iov_len < (p_head_map->msglen - TIMS_HEADLEN)))
    {

        tims_error("%x -> %x: msg is too big for receive buffer "
                   "(msg: %d, buffer: %d)\n",
                   p_head_map->src, p_head_map->dest, p_head_map->msglen,
                   msg->msg_iov[0].iov_len + msg->msg_iov[1].iov_len);

        ret = -ENOMEM;
        goto recvmsg_error;
    }

    tims_dbginfo("Recv %x -> %x, type: %d, prio: %d, %u + %u bytes\n",
                p_head_map->src, p_head_map->dest,
                p_head_map->type, p_head_map->priority,
                TIMS_HEADLEN, p_head_map->msglen - TIMS_HEADLEN );

    ret = copy_msg_out_slot(user_info, slot, msg, p_ctx->p_mbx->flags);
    if (ret) {
        goto recvmsg_error;
    }

    tims_peek_end_intern(p_ctx->p_mbx);
    tims_ctx_put(p_ctx);
    return 0;

recvmsg_error:

    tims_peek_end_intern(p_ctx->p_mbx);
    tims_ctx_put(p_ctx);
    tims_error("Recvmsg exit, code = %d\n", ret);
    return ret;

}

// realtime or non realtime context (xenomai task or linux)
ssize_t rt_tims_sendmsg(struct rtdm_dev_context *context,
                        rtdm_user_info_t *user_info,
                        const struct msghdr *msg,
                        int flags)
{
    int             ret         = 0;
    int             i           = 0;
    unsigned char   vectorlen   = 0;
    int             msglen      = 0;
    timsCtx*        p_dest_ctx  = NULL;
    timsCtx*        p_ctx       = NULL;
    timsMbx*        p_dest_mbx  = NULL;
    timsMsgHead*    p_head      = NULL;
    timsMbxSlot*    slot        = NULL;

    // get context
    p_ctx = tims_ctx_get_rtdm(context);
    if (!p_ctx)
        return -ENODEV;

    // the send mailbox must have an address
    if (!test_bit(TIMS_CTX_BIT_BOUND, &p_ctx->flags))
    {
        tims_ctx_put(p_ctx);
        return -EFAULT;
    }

    if (!msg || !msg->msg_iovlen || !msg->msg_iov)
    {
        tims_ctx_put(p_ctx);
        return -EINVAL;
    }

    // get tims message
    p_head    = msg->msg_iov[0].iov_base;
    vectorlen = msg->msg_iovlen;

    tims_dbginfo("Send vector: %d entries:", vectorlen);
    for (i=0; i<vectorlen; i++)
    {
        tims_dbginfo_0(" 0x%p (%lu byte)", msg->msg_iov[i].iov_base,
                      (unsigned long)msg->msg_iov[i].iov_len);
    }
    tims_dbginfo_0("\n");

    // set byteorder before sending the message to another system
    tims_set_head_byteorder(p_head);
    if (vectorlen > 1) // data
    {
        tims_set_body_byteorder(p_head);
    }

    // get complete message length
    for (i=0; i< vectorlen; i++)
    {
        msglen += msg->msg_iov[i].iov_len;
    }

    // check message length
    if (p_head->msglen != msglen)
    {
        tims_error("Invalid msg length head.msglen = %d, "
                   "complete size = %d\n", p_head->msglen, msglen);
        tims_ctx_put(p_ctx);
        return -EINVAL;
    }

    p_head->src = p_ctx->sock.id;

    // local or global mailbox ???
    p_dest_ctx = tims_ctx_get(p_head->dest);
    if (!p_dest_ctx)
    {
        tims_ctx_put(p_ctx);
        return tims_sendmsg_global(user_info, msg);
    }

    p_dest_mbx = p_dest_ctx->p_mbx;

    // return, if buffer is too small
    if (msglen > p_dest_mbx->msg_size)
    {
        tims_error("%x -> %x, Msg is too big for mbx (%d -> %d)\n",
                   p_head->src, p_head->dest, msglen, p_dest_mbx->msg_size);
        tims_ctx_put(p_dest_ctx);
        tims_ctx_put(p_ctx);
        return -ENOMEM;
    }

    // sending message to a local mailbox
    slot = tims_get_write_slot(p_dest_mbx, p_head->priority);
    if (!slot)
    {
        tims_error("No free write slot in mailbox %08x \n",
                   p_dest_mbx->address);
        tims_ctx_put(p_dest_ctx);
        tims_ctx_put(p_ctx);
        return -ENOSPC;
    }

    ret = copy_msg_into_slot(user_info, slot, msg, p_dest_mbx->flags);
    if (ret)
        goto sendmsg_error;

    tims_put_write_slot(p_dest_mbx, slot);
    tims_ctx_put(p_dest_ctx);
    tims_ctx_put(p_ctx);
    return msglen;

sendmsg_error:

    tims_put_write_slot_error(p_dest_mbx, slot);
    tims_error("%x -> %x, Can't send message, code = %d\n",
               p_head->src, p_head->dest, ret);
    tims_ctx_put(p_dest_ctx);
    tims_ctx_put(p_ctx);
    return ret;
}

// ****************************************************************************
//
//     socket and close
//
//    -> userspace / kernel task (non realtime context)
//
// ****************************************************************************

int rt_tims_socket(struct rtdm_dev_context *context,
                   rtdm_user_info_t *user_info,
                   int protocol)
{
    timsCtx *p_ctx;

    if (protocol != RTDM_PF_TIMS)
    {
        tims_error("Unsupported protocol %d \n",protocol);
        return -EINVAL;
    }

    // don't create new socket while module shutdown or module init
    if ((test_bit(TIMS_STATE_BIT_STARTING, &td.state_flags)) ||
        (test_bit(TIMS_STATE_BIT_SHUTDOWN, &td.state_flags)))
    {
        tims_error("Tims init/shutdown\n");
        return -ENODEV;
    }

    p_ctx = (timsCtx *)context->dev_private;

    // init context
    init_context(p_ctx);
    p_ctx->protocol = protocol;

    // insert context in list (not in cache)
    tims_ctx_add(p_ctx);

    tims_dbginfo("Open socket, protocol %d \n",protocol);
    tims_dbginfo("Tims context created @ %p (%d byte)\n",
                 p_ctx, sizeof(timsCtx) );

    // inc module use counter
    try_module_get(THIS_MODULE);

    return 0;
}

int rt_tims_close(struct rtdm_dev_context *context,
                  rtdm_user_info_t *user_info)
{
    int ret = 0;

    // rtdm has been set the RTDM_CLOSING flag
    timsCtx*    p_ctx = (timsCtx *)context->dev_private;

    // setting close bit for functions which wouldn't get the
    // rtdm context (pipe-task, rtnet)
    set_bit(TIMS_CTX_BIT_CLOSING, &p_ctx->flags);

    // nobody gets this context (excepting ioctl TIMS_RTIOC_RECVEND)

    tims_dbginfo("Close socket with id %x \n", p_ctx->sock.id);

    // wait for write tasks and wake up read task
    if (test_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags))
    {
        ret = tims_wait_for_writers(p_ctx);
        if (!ret)
        {
            tims_info("Write list of mailbox %x is empty\n",
                      p_ctx->p_mbx->address);
        }

        ret = tims_wake_up_reader(p_ctx);
        if (!ret)
        {
            tims_info("No reader in mailbox %x\n", p_ctx->p_mbx->address);
        }

        // check use counter
         tims_info("Use counter is %d\n", p_ctx->use_counter);

        // free mailbox
        destroy_mailbox(p_ctx);

        clear_bit(TIMS_CTX_BIT_MBX_CREATED, &p_ctx->flags);
    }

    // destroy context
    tims_ctx_remove(p_ctx);

    // dec module use counter
    module_put(THIS_MODULE);

    return 0;
}

// ****************************************************************************
//
// rtnet receive function
//
// Note: This function runs in the context of the RTnet stack manager (prio 1)!
//
// ****************************************************************************

#ifdef CONFIG_TIMS_USE_RTNET

static int rtnet_recv_message(timsMbx* p_recv_mbx, timsMsgHead *head)
{
    timsMbxSlot*    recv_slot       = NULL;
    struct iovec    iov[get_max_pages(head->msglen)];
    struct msghdr   recv_msg;
    unsigned long   p_recv_map      = 0;
    void*           p_recv          = NULL;
    unsigned long   recv_page       = 0;
    unsigned long   recv_bytes      = head->msglen;
    int             ret;
    int             free_in_page    = 0;
    int             akt_copy_size   = 0;

    // get local mailbox slot
    recv_slot = tims_get_write_slot(p_recv_mbx, head->priority);
    if (!recv_slot)
    {
        tims_error("[RTnet]: No free write slot in mailbox %08x \n",
                   p_recv_mbx->address);
        rtdm_recv(td.rtnet_fd, head, 0, 0); // clean up
        return -ENOSPC;
    }

    //
    // create msg vector
    //
    memset(&recv_msg, 0, sizeof(struct msghdr));

    if (test_bit(TIMS_MBX_BIT_USRSPCBUFFER, &p_recv_mbx->flags))
    {
        // receive mailbox is in userpace

        p_recv_map  = recv_slot->p_head_map;
        p_recv      = recv_slot->p_head;
        recv_page   = recv_slot->map_idx;

        recv_msg.msg_iov    = iov;
        recv_msg.msg_iovlen = 0;

        while (recv_bytes)
        {
            free_in_page  = (p_recv_map & PAGE_MASK) + PAGE_SIZE - p_recv_map;
            akt_copy_size = free_in_page > recv_bytes ?
                            recv_bytes : free_in_page;

            iov[recv_msg.msg_iovlen].iov_base = (void*)p_recv_map;
            iov[recv_msg.msg_iovlen].iov_len  = akt_copy_size;

            recv_msg.msg_iovlen++;

            free_in_page -= akt_copy_size;

            if (!free_in_page)
            {
                recv_page++;
                if (recv_slot->p_mbx->p_mapInfo[recv_page].mapped)
                {
                    p_recv_map = recv_slot->p_mbx->p_mapInfo[recv_page].virtual;
                }
                else
                {
                    ret = -ENOMEM;
                    goto recvmsg_error_clean;
                }
            }
        }
    }
    else // receive mailbox is in kernelspace
    {
        struct iovec iov[1];

        iov[0].iov_base     = recv_slot->p_head;
        iov[0].iov_len      = recv_bytes;

        recv_msg.msg_iov     = iov;
        recv_msg.msg_iovlen = 1;
    }

    // receive complete message
    ret = rtdm_recvmsg(td.rtnet_fd, &recv_msg, 0);
    if (ret != head->msglen)
    {
        tims_error("[RTnet]: Corrupt message received, code = %d) \n", ret);
        goto recvmsg_error;
    }

    tims_put_write_slot(p_recv_mbx, recv_slot);
    return 0;

recvmsg_error_clean:

    rtdm_recv(td.rtnet_fd, head, 0, 0); // clean up

recvmsg_error:

    tims_put_write_slot_error(p_recv_mbx, recv_slot);
    tims_error("[RTnet]: %x -> %x, Can't send message, code = %d\n",
               head->src, head->dest, ret);
    return ret;

}

static void rtnet_recv_callback(struct rtdm_dev_context *context, void* arg)
{
    timsMsgHead     head;
    timsCtx*        ctx;
    int             ret;
    timsMbx*        p_recv_mbx  = NULL;

    // receive message head (only peek)
    ret = rtdm_recv(td.rtnet_fd, &head, sizeof(timsMsgHead), MSG_PEEK);
    if (ret < (int)sizeof(timsMsgHead))
    {
        tims_error("[RTnet]: Corrupt package received (rtdm_recv(): %d)\n",
                   ret);

        rtdm_recv(td.rtnet_fd, &head, 0, 0); /* clean up */
        return;
    }

    tims_set_head_byteorder(&head);

    tims_dbgdetail("[RTnet] %x -> %x, type %d, msglen %d \n",
                   head.src, head.dest, head.type, head.msglen);

    // get mailbox context
    ctx = tims_ctx_get(head.dest);
    if (!ctx)
    {
        tims_error("[RTnet]: %x -> %x, Can't deliver msg, type %d, msglen %d. "
                   "Mailbox not available.\n", head.src, head.dest, head.type,
                   head.msglen);

        rtdm_recv(td.rtnet_fd, &head, 0, 0); // clean up
        return;
    }

    p_recv_mbx = ctx->p_mbx;

    // return, if buffer is too small
    if (head.msglen > p_recv_mbx->msg_size)
    {
        tims_error("[RTnet]: %x -> %x, Msg is too big for mbx (%d -> %d)\n",
                   head.src, head.dest, head.msglen, p_recv_mbx->msg_size);

        rtdm_recv(td.rtnet_fd, &head, 0, 0); // clean up
        tims_ctx_put(ctx);
        return;
    }

    // receive complete message
    ret = rtnet_recv_message(p_recv_mbx, &head);
    if (ret)
    {
        tims_error("[RTnet]:%x -> %x, Can't receive message, code = %d) \n",
                   head.src, head.dest, ret);
    }

    tims_ctx_put(ctx);
    return;
}

#endif /* CONFIG_TIMS_USE_RTNET */

// ****************************************************************************
//
//     pipe receive task and functions
//
//    -> kernel realtime task
//
// ****************************************************************************

static int pipe_receive_router_config(RT_PIPE_MSG* recvMsg)
{
    timsMsgRouter_ConfigMsg*      configMsg;
    unsigned int                  configSize;
    unsigned int                  i;
    timsMsgRouter_MbxRoute*       entry;
    timsMsgHead*                  p_head;

#ifdef CONFIG_TIMS_USE_RTNET

    struct ifreq                  ifr[8];
    struct ifconf                 ifc;
    int                           ips;
    int                           j;
    int                           localIp;

#endif  /* CONFIG_TIMS_USE_RTNET */

    configMsg = (timsMsgRouter_ConfigMsg *)P_MSGPTR(recvMsg);
    p_head    = &configMsg->head;

    if (p_head->msglen < sizeof(timsMsgRouter_ConfigMsg) )
    {
        tims_error("[PIPE]: Corrupt configuration msg in pipeFromClient, "
                   "p_head->msglen < len(timsMsgRouter_ConfigMsg)\n");
        return -EINVAL;
    }

    if (p_head->flags & MESSAGE_FLAG_BODY_ORDER_LE)
        configMsg->num = __le32_to_cpu(configMsg->num);
    else
        configMsg->num = __be32_to_cpu(configMsg->num);

    configSize = configMsg->num * sizeof(timsMsgRouter_MbxRoute);

    if ((p_head->msglen - sizeof(timsMsgRouter_ConfigMsg)) < configSize )
    {
        tims_error("[PIPE]: Corrupt configuration msg in pipeFromClient, "
                   "configSize too great\n");
        return -EINVAL;
    }

#ifdef CONFIG_TIMS_USE_RTNET

    if (td.mbxRoute != NULL)
    {
        tims_error("[PIPE]: Duplicate configuration received.\n");
        return -EINVAL;
    }

    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;
    if (rtdm_ioctl(td.rtnet_fd, SIOCGIFCONF, &ifc) < 0)
        ips = 0;
    else
        ips = ifc.ifc_len / sizeof(struct ifreq);

    td.mbxRoute = kmalloc(configSize, GFP_KERNEL);
    if (!td.mbxRoute)
    {
        tims_error("[PIPE]: Insufficient memory for configuration (%d bytes)\n",
                   configSize);
        return -ENOMEM;
    }
    set_bit(TIMS_INIT_BIT_RTNET_MBXROUTE, &td.init_flags);
    tims_info("Buffer for struct maxRoute created\n");

#endif  /* CONFIG_TIMS_USE_RTNET */

    for (i = 0; i < configMsg->num; i++)
    {
        entry = &configMsg->mbx_route[i];

#ifdef CONFIG_TIMS_USE_RTNET

        if (p_head->flags & MESSAGE_FLAG_BODY_ORDER_LE)
        {
            entry->mbx = __le32_to_cpu(entry->mbx);
            entry->ip  = __le32_to_cpu(entry->ip);
        }
        else
        {
            entry->mbx = __be32_to_cpu(entry->mbx);
            entry->ip  = __be32_to_cpu(entry->ip);
        }

        localIp = 0;
        for (j = 0; j < ips; j++)
        {
            if (entry->ip ==
                ((struct sockaddr_in*)&ifr[j].ifr_addr)->sin_addr.s_addr)
            {
                localIp = 1;
            }
        }

        if (!localIp)
        {
            memcpy(&td.mbxRoute[td.mbxRouteNum],
                   &entry, sizeof(timsMsgRouter_MbxRoute));
            td.mbxRouteNum++;
        }

#endif  /* CONFIG_TIMS_USE_RTNET */

    }
    return 0;
}


static int pipe_register_mbx_list(RT_PIPE_MSG* recvMsg)
{
    timsMsgRouter_MbxMsg    mbxMsg;
    timsMsgHead*            p_head;
    int                     ret;
    timsCtx*                  p_ctx;
    rtdm_lockctx_t          lock_ctx;

    p_head = (timsMsgHead *)P_MSGPTR(recvMsg);

    tims_info("Registering %d mailboxes.\n", td.mbx_num);

    tims_fillhead(&mbxMsg.head, TIMS_MSG_ROUTER_MBX_PURGE, 0, 0, 0, 0, 0,
                  TIMS_HEADLEN);

    ret = rt_pipe_write(&td.pipeToClient, &mbxMsg, TIMS_HEADLEN, P_NORMAL);
    if (ret != TIMS_HEADLEN)
    {
        tims_warn("[PIPE]: Can't send mbx purge msg, code = %d\n", ret);
    }

    tims_fillhead(&mbxMsg.head, TIMS_MSG_ROUTER_MBX_INIT, 0, 0, 0, 0, 0,
                  sizeof(timsMsgRouter_MbxMsg));

    rtdm_lock_get_irqsave(&td.ctx_lock, lock_ctx);

    list_for_each_entry(p_ctx, &td.ctx_list, ctx_list)
    {
        mbxMsg.mbx = p_ctx->p_mbx->address;

        tims_dbginfo("[PIPE]: Send %x -> %x, type %d, len %d\n",
                    mbxMsg.head.src, mbxMsg.head.dest, mbxMsg.head.type,
                    mbxMsg.head.msglen);

        ret = rt_pipe_write(&td.pipeToClient, &mbxMsg,
                            sizeof(timsMsgRouter_MbxMsg), P_NORMAL);
        if (ret != sizeof(timsMsgRouter_MbxMsg))
        {
            tims_error("[PIPE]: Can't send mbx msg %x, code = %d\n",
                       mbxMsg.mbx, ret);
            rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
            return ret;
        }
        else
        {
            tims_info("[PIPE]: Registering mbx %x @ TcpTimsMsgRouter "
                      "(%d bytes)\n", mbxMsg.mbx, sizeof(timsMsgRouter_MbxMsg));
        }
    }
    rtdm_lock_put_irqrestore(&td.ctx_lock, lock_ctx);
    return 0;
}



static void pipe_recv_proc(void *arg)
{
    timsMsgHead*    p_head  = NULL;
    timsCtx*        p_ctx   = NULL;
    timsMbx*        p_mbx   = NULL;
    timsMbxSlot*    slot    = NULL;
    RT_PIPE_MSG*    recvMsg = NULL;
    struct iovec    iov[2];
    struct msghdr   msg     = { NULL, 0, iov, 2, NULL, 0, 0 };
    int             ret     = 0;
    void*           ptr     = NULL;

    atomic_inc(&td.taskCount);

    tims_print("[PIPE]: waiting for msg from timsMsgClient ...\n");

    while (!td.terminate)   // terminate will be set by cleanup()
    {
        // read message from pipe (blocking)
        ret = rt_pipe_receive(&td.pipeFromClient, &recvMsg, TM_INFINITE);
        if (ret < 0)
        {
            if (ret == -EINTR) { // waked up without data
                tims_warn("[PIPE]: Recv -EINTR while receiving -> continue\n");
                continue;
            }
            else // error
            {
                tims_error("[PIPE]: Can't receive data, code = %d\n", ret);
                goto task_exit;
            }
        }
        else if (ret > 1 && ret < TIMS_HEADLEN) // smaller than timsMsgHead
        {
            rt_pipe_free(&td.pipeToClient, recvMsg);
            tims_warn("[PIPE]: Recv msg with %d bytes only, skip\n", ret);
            continue;
        }
        else if (ret == 1) // internal message
        {
            int* p_val = (int *)P_MSGPTR(recvMsg);
            if (*p_val == 1)    // terminate
            {
                tims_error("[PIPE]: Recv term signal from TIMS -> EXIT\n");
                goto task_exit;
            }
        }
        p_head = (timsMsgHead *)P_MSGPTR(recvMsg);
        tims_parse_head_byteorder(p_head);

        // message received -> we have to free it with rt_pipe_free()

        if (P_MSGSIZE(recvMsg) != p_head->msglen )
        {
            tims_warn("[PIPE]: Corrupt msg in pipeFromClient, "
                      "head->msgLen != len(recvMsg)\n");
            rt_pipe_free(&td.pipeFromClient, recvMsg);
            continue;
        }

        // check message type
        if (p_head->dest == 0 && p_head->src == 0)
        {
            switch(p_head->type)
            {
                case TIMS_MSG_ROUTER_CONFIG:
                    pipe_receive_router_config(recvMsg);
                    tims_print("[PIPE]: Config file received ...\n");
                    rt_pipe_free(&td.pipeFromClient, recvMsg);
                    continue;

                case TIMS_MSG_ROUTER_CONNECTED:
                    pipe_register_mbx_list(recvMsg);
                    tims_print("[PIPE]: Connected to TcpTimsMsgRouter ...\n");
                    rt_pipe_free(&td.pipeFromClient, recvMsg);
                    continue;

                default:
                    tims_error("[PIPE]: Unknown meta msg in "
                               "pipeFromClient (type %d) \n", p_head->type);
                    rt_pipe_free(&td.pipeFromClient, recvMsg);
                    continue;
            }
        }

        // get local context/mailbox
        p_ctx = tims_ctx_get(p_head->dest);
        if (!p_ctx)
        {
            rt_pipe_free(&td.pipeFromClient, recvMsg);
            tims_error("[PIPE]: Mbx %x not available\n", p_head->dest);
            continue;
        }
        p_mbx = p_ctx->p_mbx;

        // Check if message fits into mailbox slot
        if (p_head->msglen > p_mbx->msg_size)
        {
            rt_pipe_free(&td.pipeFromClient, recvMsg);
            tims_ctx_put(p_ctx);
            tims_error("[PIPE]: Msg is bigger than max msg size %u\n",
                       p_mbx->msg_size);

            continue;
        }

        // get write slot
        slot  = tims_get_write_slot(p_mbx, p_head->priority);
        if (!slot)
        {
            rt_pipe_free(&td.pipeFromClient, recvMsg);
            tims_ctx_put(p_ctx);
            tims_error("[PIPE]: No free write slot in mailbox %08x, "
                       "[%d|%d|%d|%d]\n", p_mbx->address,
                       p_mbx->slot_state.free, p_mbx->slot_state.write,
                       p_mbx->slot_state.read, p_mbx->slot_state.peek);

            continue;
        }

        ptr = (void *)p_head;

        // add message to vector list
        iov[0].iov_base = ptr;
        iov[0].iov_len  = TIMS_HEADLEN;
        ptr += TIMS_HEADLEN;

        iov[1].iov_base = ptr;
        iov[1].iov_len  = p_head->msglen - TIMS_HEADLEN;

        ret = copy_msg_into_slot(NULL, slot, &msg, p_mbx->flags);
        if (ret)
        {
            tims_put_write_slot_error(p_mbx, slot);
            rt_pipe_free(&td.pipeFromClient, recvMsg);
            tims_ctx_put(p_ctx);
            continue;
        }

        tims_put_write_slot(p_mbx, slot);
        rt_pipe_free(&td.pipeFromClient, recvMsg);
        tims_ctx_put(p_ctx);
    }

    task_exit:
    atomic_dec(&td.taskCount);
    tims_info("timsRecvTask EXIT \n");
}

// ****************************************************************************
//
//     tims rtdm device
//
// ****************************************************************************

static struct rtdm_device tims_rtdmdev = {
    struct_version:     RTDM_DEVICE_STRUCT_VER,

    device_flags:       RTDM_PROTOCOL_DEVICE,
    context_size:       sizeof(timsCtx),
    device_name:        "",

    protocol_family:    RTDM_PF_TIMS,
    socket_type:        SOCK_RAW,

    open_rt:            NULL,
    open_nrt:           NULL,

    socket_rt:          NULL,


    socket_nrt:         rt_tims_socket,

    ops: {
        close_rt:       NULL,
        close_nrt:      rt_tims_close,

        ioctl_rt:       rt_tims_ioctl,
        ioctl_nrt:      rt_tims_ioctl,

        read_rt:        NULL,
        read_nrt:       NULL,

        write_rt:       NULL,
        write_nrt:      NULL,

        recvmsg_rt:     rt_tims_recvmsg,
        recvmsg_nrt:    NULL,

        sendmsg_rt:     rt_tims_sendmsg,
        sendmsg_nrt:    rt_tims_sendmsg,
    },

    device_class:       RTDM_CLASS_NETWORK,
    device_sub_class:   RTDM_SUBCLASS_TIMS,
    driver_name:        "TIMS",
    driver_version:     RTDM_DRIVER_VER(0, 0, 4),
    peripheral_name:    "",
    provider_name:      "Joerg Langenberg",

    proc_name:          "tims"
};

// ****************************************************************************
//
//    init and cleamup functions
//
//    -> linux task (non realtime context)
//
// ****************************************************************************



static inline void tims_ctx_cache_init(void)
{
    memset(&td.ctxCache, 0, CONFIG_TIMS_CTX_CACHE_SIZE * sizeof(timsCtxCache));
}



static void tims_cleanup(void)
{
    int     ret;

#ifdef CONFIG_TIMS_USE_RTNET
    void*     tmp;
#endif // CONFIG_TIMS_USE_RTNET

    // set pipe_task terminate bit
    td.terminate = 1;

    // wake up pipe_task
//TODO

#ifdef CONFIG_TIMS_USE_RTNET

    if (test_and_clear_bit(TIMS_INIT_BIT_RTNET_SOCKET, &td.init_flags))
    {
        while (rtdm_close(td.rtnet_fd) == -EAGAIN)
        {
            tims_info("RTnet socket busy - waiting...\n");
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1*HZ); /* wait a second */
        }
    }

    if (test_and_clear_bit(TIMS_INIT_BIT_RTNET_TDMA, &td.init_flags))
    {
        rtdm_close(td.tdma_disc);
          tims_info("RTnet time service closed\n");
    }

    if (test_and_clear_bit(TIMS_INIT_BIT_RTNET_MBXROUTE, &td.init_flags))
    {
        tmp          = td.mbxRoute;
        td.mbxRoute = NULL;
        if (tmp != NULL)
            kfree(tmp);

        tims_info("Buffer for struct maxRoute deleted\n");
    }


#endif  // CONFIG_TIMS_USE_RTNET

    if (test_and_clear_bit(TIMS_INIT_BIT_PIPE_TO_CLIENT, &td.init_flags))
    {
        rt_pipe_delete(&td.pipeToClient);
        tims_info("Pipe to client has been deleted\n");
    }

    if (test_and_clear_bit(TIMS_INIT_BIT_PIPE_FROM_CLIENT, &td.init_flags))
    {
        rt_pipe_delete(&td.pipeFromClient);
        tims_info("Pipe from client has been deleted\n");
    }

    if (test_and_clear_bit(TIMS_INIT_BIT_RECV_TASK, &td.init_flags))
    {
        rtdm_task_destroy(&td.pipeRecvTask);
        tims_info("timsRecvTask has been deleted\n");
    }

    // unregister tims
    if (test_and_clear_bit(TIMS_INIT_BIT_REGISTERED, &td.init_flags))
    {
        ret = rtdm_dev_unregister(&tims_rtdmdev, 50); // poll delay 50ms
        if (ret)
        {
            tims_error("Can't unregister driver, code = %d \n", ret);
        }
        else
        {
            tims_info("Driver unregistered \n");
        }
    }
}



static int tims_init(void)
{
    int ret;
    timsMsgHead getConfig;

#ifdef CONFIG_TIMS_USE_RTNET

    struct sockaddr_in      bindAddr;
    int                     nonblock = -1;
    struct rtnet_callback   callback = {rtnet_recv_callback, NULL};

    td.rtnet_fd    = -1;
    td.mbxRoute    = NULL;
    td.mbxRouteNum = 0;
    td.tdma_disc   = 0;

#endif // CONFIG_TIMS_USE_RTNET

    atomic_set(&td.taskCount,0);
    rtdm_lock_init(&td.ctx_lock);        // init context lock
    INIT_LIST_HEAD(&td.ctx_list);

    // init context cache

    tims_ctx_cache_init();

    //
    // creating pipes
    //

    ret = rt_pipe_create(&td.pipeToClient, "timsPipeToClient",
                         PIPE_TIMS_TO_CLIENT,
                         max_msg_size * 1024 * max_msg_slots);
    if (ret)
    {
        tims_error("Unable to create PIPE channel %d to Client.\n",
                   PIPE_TIMS_TO_CLIENT);
        goto init_error;
    }
    set_bit(TIMS_INIT_BIT_PIPE_TO_CLIENT, &td.init_flags);
    tims_info("Pipe to Client created \n");

    ret = rt_pipe_create(&td.pipeFromClient, "timsPipeFromClient",
                         PIPE_CLIENT_TO_TIMS,
                         max_msg_size * 1024 * max_msg_slots);
    if (ret)
    {
        tims_error("Unable to create PIPE channel %d from Client.\n",
                   PIPE_CLIENT_TO_TIMS);
        goto init_error;
    }
    set_bit(TIMS_INIT_BIT_PIPE_FROM_CLIENT, &td.init_flags);
    tims_info("Pipe from client created \n");

    //
    // creating pipe recv task
    //

    ret = rtdm_task_init(&td.pipeRecvTask, "timsPipeReceiver", pipe_recv_proc,
                         NULL, RTDM_TASK_LOWEST_PRIORITY, 0);
    if (ret < 0)
    {
        tims_error("Unable to create timsPipeReceiver task.\n");
        goto init_error;
    }
    tims_info("timsRecvTask started \n");
    set_bit(TIMS_INIT_BIT_RECV_TASK, &td.init_flags);

    //
    // init rtnet
    //

#ifdef CONFIG_TIMS_USE_RTNET

    // create socket
    td.rtnet_fd = rtdm_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (td.rtnet_fd < 0)
    {
        tims_error("Unable to create RTnet socket.\n");
        ret = td.rtnet_fd;
        goto init_error;
    }
    tims_info("RTnet socket created \n");
    set_bit(TIMS_INIT_BIT_RTNET_SOCKET, &td.init_flags);


    // bind to TIMS_MSG_ROUTER_PORT
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_port        = htons(TIMS_MSG_ROUTER_PORT);
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    ret = rtdm_bind(td.rtnet_fd, (struct sockaddr *)&bindAddr, sizeof(bindAddr));
    if (ret < 0)
    {
        tims_error("Unable to bind RTnet socket.\n");
        goto init_error;
    }
    tims_info("RTnet socket has been bound \n");

    // allocate additional buffers
    ret = rtdm_ioctl(td.rtnet_fd, RTNET_RTIOC_EXTPOOL, &rtnet_buffers);
    if (ret < 0)
    {
        tims_error("Unable to allocate additional RTnet buffers.\n");
        goto init_error;
    }
    rtdm_ioctl(td.rtnet_fd, RTNET_RTIOC_TIMEOUT, &nonblock);
    rtdm_ioctl(td.rtnet_fd, RTNET_RTIOC_CALLBACK, &callback);

    // open time service
    if ((td.tdma_disc = rtdm_open(TIME_REFERENCE_DEV, O_RDONLY)) < 0)
    {
        tims_error("Unable to initialise time service (RTmac/TDMA is missing)\n");
        ret = -ENODEV;
        goto init_error;
    }
    tims_info("RTnet socket created \n");
    set_bit(TIMS_INIT_BIT_RTNET_TDMA, &td.init_flags);

#endif // CONFIG_TIMS_USE_RTNET

    //
    // Request rtnet configuration
    //

    tims_fillhead(&getConfig, TIMS_MSG_ROUTER_GET_CONFIG, 0, 0,
                  0, 0, 0, TIMS_HEADLEN);

    ret = rt_pipe_write(&td.pipeToClient, &getConfig, TIMS_HEADLEN, P_NORMAL);
    if (ret != TIMS_HEADLEN)
    {

#ifdef CONFIG_TIMS_USE_RTNET
        tims_error("Can't request config file -> EXIT \n");
        goto init_error;
#else    // ! CONFIG_TIMS_USE_RTNET
        tims_warn("Can't request config file -> use TCP-Router \n");
#endif    // ! CONFIG_TIMS_USE_RTNET

    }

    //
    // register tims @ RTDM
    //

    ret = rtdm_dev_register(&tims_rtdmdev);
    if (ret)
    {
        tims_error("Can't register driver \n");
        goto init_error;
    }
    set_bit(TIMS_INIT_BIT_REGISTERED, &td.init_flags);
    tims_info("Driver registered \n");

    return 0;

init_error:
    tims_cleanup();
    return ret;
}



int __init mod_start(void)
{
    int ret;

    // clear driver mem (and all flags)
    memset(&td, 0, sizeof(struct tims_driver));

    // set start flag
    set_bit(TIMS_STATE_BIT_STARTING, &td.state_flags);

    rtdm_printk("********** %s - %s ***********\n", DRIVER_DESC, DRIVER_VERSION);

    if (dbglevel < 0 && dbglevel > TIMS_LEVEL_MAX)
    {
        rtdm_printk("TIMS: ERROR: Invalid debug level -> EXIT \n");
        return -1;
    }
    rtdm_printk("TIMS: Using debug level %d\n", dbglevel);
    rtdm_printk("TIMS: Max message size is %d kB \n", max_msg_size);
    rtdm_printk("TIMS: Max slots for sending over TCP/Pipe %d \n", max_msg_slots);

#ifdef CONFIG_TIMS_USE_RTNET

    rtdm_printk("TIMS: RTnet enabled\n");

#endif // CONFIG_TIMS_USE_RTNET

    ret = tims_init();
    clear_bit(TIMS_STATE_BIT_STARTING, &td.state_flags);
    return ret;
}



void mod_exit(void)
{
    // set shutdown flag
    set_bit(TIMS_STATE_BIT_SHUTDOWN, &td.state_flags);
    tims_cleanup();
}


module_init (mod_start);
module_exit (mod_exit);

#ifndef __CSS_RQ_H_
#define __CSS_RQ_H_

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/unistd.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>

#define NOT_FOUND -1
#define Q_MAXCAPACITY 300000000
#define T_PERIOD 10000000000

enum serverState { Inactive, Active, ActvResid, InactvNonIso };
enum job_state {Ready, Running, Done};

/*
  Auxiliary macros
*/
#define _serversList rq->css.serversManager.serversList
#define _serversCount rq->css.serversManager.serversCount
#define _pSedf rq->css.serversManager.sedf_ptr

// CSS Server
struct css_server { 
  struct task_struct *job;

  u64 Q_maxCap;
  u64 T_period;

  u64 c_capacity;
  u64 d_deadline;
  u64 r_residualCap;
  u64 h_replenish;

  unsigned int state;
  struct css_server **pSedf; 

  ktime_t run_ktime;
  struct hrtimer run_timer;

  ktime_t replenish_ktime;
  struct hrtimer replenish_timer;

  ktime_t deadline_ktime;
  struct hrtimer deadline_monitor_timer;
};

/*private*/
enum hrtimer_restart timer_callback_residual_exhausted(struct hrtimer *timer);
enum hrtimer_restart timer_callback_capacity_exhausted(struct hrtimer *timer);
enum hrtimer_restart timer_callback_replenishment(struct hrtimer *timer);

void css_server_consume_residual(struct css_server *server, struct css_server *sedf);
void handle_capacity_exhausted(struct css_server *server);

/*public*/
int css_server_get_sid(struct rq *rq, struct task_struct *p);
extern void css_server_start_run(struct css_server *server, u64 capacity);
extern void css_server_start_replenish_timer(struct css_server *server);

//--------------------------------------------------------

// CSS Severs Manager
struct css_servers_manager {
  unsigned int serversCount;
   struct css_server *serversList;
  struct css_server *sedf_ptr; /* points to next Ar server available for reclaiming*/
};
//--------------------------------------------------------

// CSS Run queue
struct css_rq {
  struct rb_root root;

  struct css_servers_manager serversManager;

  raw_spinlock_t lock;
  unsigned nr_running;
};

/* private methods */

/* public methods */
void cssrq_init_css_server(struct rq *rq, struct css_server *server);
void cssrq_init_css_rq(struct css_rq *rq);
void cssrq_trigger_server_start(struct rq *rq, struct task_struct *p);
void cssrq_interrupt_server(struct rq *rq, struct task_struct *p);
void cssrq_stop_server(struct rq *rq, struct task_struct *p);
/* method defined in sched.c*/
void css_rq_add_task_to_rq_rbtree(struct task_struct *p);
/* method defined in sched.c*/
void css_rq_remove_task_from_rq_rbtree(struct task_struct *p);

//--------------------------------------------------------

// External methods
extern void __setparam_css(struct task_struct *p,
                           const struct sched_attr *attr);
extern void __getparam_css(struct task_struct *p, struct sched_attr *attr);
//--------------------------------------------------------
#endif

/*
 * Note: Assuming no more than one job per server now
 * run_timer related events:
 *   - switched to task: start run_timer with capacity (c). done, TODO: get
 * remaining cap
 *   - task switched away (__state = 0): Job interrupted. done
 *       Stop run_timer;
 *       Decrement the capacity: c = r_time;
 *       Server remain as active.
 *   - dequeue task: Job finished, stops run_timer and get the remaining time
 * (r_time). If r_time < 0, update the remaining capacity (r): r = r_time; and c
 * = 0; Keep the server active and mark as Ar If r_time = 0, server goes to
 * inactive.
 *   - run_timer expires: means that the capacity is exhausted.
 *       TODO: what to do??? schedule();
 *
 *  replenish_timer related events:
 *    - expires: - recharge c
 *               - if no pending job, goes to Inactive, even with some reserved
 * capacity left
 */
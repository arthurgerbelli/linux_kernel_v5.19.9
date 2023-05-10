#ifndef __CSS_RQ_H_
#define __CSS_RQ_H_

#include <linux/hrtimer.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/unistd.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>

/* Initial defines, run 1:
 *   1x periodic with 0.4U
 *   2x aperiodic with 0.3U each
 */
#define Q_MAXCAPACITY 300000000
#define T_PERIOD 10000000000

enum serverState{Inactive, Active, ActvResid, InactvNonIso};
 

struct css_server {
  pid_t servedTask;

  u64 Q_maxCap;
  u64 T_period;

  u64 c_capacity;
  u64 d_deadline;
  u64 r_residualCap;
  u64 h_replenish;

  int state;

  struct hrtimer timer;
};
struct css_servers_manager {
  unsigned int serversCount;
  struct css_server *serversList;
};
struct css_rq {
  struct rb_root root;

  struct css_servers_manager serversManager;

  raw_spinlock_t lock;
  unsigned nr_running;
};

void init_css_server(struct css_server *server);
void init_css_rq(struct css_rq *rq);

extern void __setparam_css(struct task_struct *p,
                           const struct sched_attr *attr);
extern void __getparam_css(struct task_struct *p, struct sched_attr *attr);

#endif

#ifndef __CSS_RQ_H_
#define __CSS_RQ_H_

#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>

struct css_rq {
  struct rb_root root;

  raw_spinlock_t lock;
  unsigned nr_running;
};

void init_css_rq(struct css_rq *rq);

extern void __setparam_css(struct task_struct *p, const struct sched_attr *attr);
extern void __getparam_css(struct task_struct *p, struct sched_attr *attr);

#endif

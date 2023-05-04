#include "css_rq.h"
#include "css_task.h"
#include <linux/rbtree.h>

void init_css_rq(struct css_rq *rq) {
  rq->root = RB_ROOT;
  raw_spin_lock_init(&rq->lock);
  rq->nr_running = 0;
}

void __setparam_css(struct task_struct *p, const struct sched_attr *attr) {
  printk(KERN_INFO "CSS __setparam_css\n");
  struct sched_css_entity *css_se = &p->css;
  css_se->css_runtime = attr->sched_runtime;
  css_se->css_deadline = attr->sched_deadline;
  css_se->css_period = attr->sched_period ?: css_se->css_deadline;
  // dl_se->flags = attr->sched_flags & SCHED_DL_FLAGS;
  // dl_se->dl_bw = to_ratio(dl_se->dl_period, dl_se->dl_runtime);
  // dl_se->dl_density = to_ratio(dl_se->dl_deadline, dl_se->dl_runtime);
}

void __getparam_css(struct task_struct *p, struct sched_attr *attr) {
  struct sched_css_entity *css_se = &p->css;

  attr->sched_priority = p->rt_priority;
  attr->sched_runtime = css_se->css_runtime;
  attr->sched_deadline = css_se->css_deadline;
  attr->sched_period = css_se->css_period;
  // attr->sched_flags &= ~SCHED_DL_FLAGS;
  // attr->sched_flags |= dl_se->flags;
}

#ifndef __CSS_TASK_H_
#define __CSS_TASK_H_

#include <linux/rbtree.h>

struct sched_css_entity {
  struct rb_node node;

  u64 css_runtime;  /* Maximum runtime for each instance	*/
  u64 css_deadline; /* Relative deadline of each instance	*/
  u64 css_period;   /* Separation of two instances (period) */
  unsigned int css_job_state;
};

#endif

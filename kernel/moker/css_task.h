#ifndef __CSS_TASK_H_
#define __CSS_TASK_H_

#include <linux/rbtree.h>

struct sched_css_entity {
  struct rb_node node;

  unsigned int css_sid; /* Server id assigned to this task*/

  u64 css_runtime;  /* Maximum runtime */
  u64 css_deadline; /* Relative deadline, set by the scheduler or the user	*/
  u64 css_abs_deadline; /* Absolute deadline, set only by the scheduler	*/
  u64 css_period;   /* Separation of two instances (period) */
  unsigned int css_job_state;
};

#endif

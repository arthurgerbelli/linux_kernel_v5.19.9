#ifndef __CSS_TASK_H_
#define __CSS_TASK_H_

#include <linux/hrtimer.h>
#include <linux/rbtree.h>

struct sched_css_entity {
  struct rb_node node;

  __u64 css_runtime;  /* Maximum runtime for each instance	*/
  __u64 css_deadline; /* Relative deadline of each instance	*/
  __u64 css_period;   /* Separation of two instances (period) */

  /*
   * To future use
   *
   */
  struct hrtimer timer;
};

#endif

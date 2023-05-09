#include "mktrace.h"
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

struct trace_evt_buffer trace;

unsigned int enabled = 0;

static void increment(int *item) {
  *item = *item + 1;
  if (*item >= TRACE_BUFFER_SIZE) {
    *item = 0;
  }
}

static int is_empty(int r, int w) {
  // empty if r == w
  // otherwise r != w
  return !(r ^ w); // xor
}

static int is_full(int r, int w) {
  int write = w;
  increment(&write);
  return write == r;
}

static int dequeue(char *buffer) {
  int ret = 0, len;
  char evt[10];
  raw_spin_lock(&trace.lock);
  if (!is_empty(trace.read_item, trace.write_item)) { // if it is not empty
    switch ((int)trace.events[trace.read_item].event) {
    case SCHED_TICK:
      strcpy(evt, "SCH_TK");
      break;
    case SWITCH_AWAY:
      strcpy(evt, "SWT_AY");
      break;
    case SWITCH_TO:
      strcpy(evt, "SWT_TO");
      break;
    case ENQUEUE_RQ:
      strcpy(evt, "ENG_RQ");
      break;
    case DEQUEUE_RQ:
      strcpy(evt, "DEQ_RQ");
      break;      
    default:
      strcpy(evt, "UK_EVT");
      break;
    }
    len = sprintf(buffer, "%llu,", trace.events[trace.read_item].time);
    len += sprintf(buffer + len, "%s,", evt);
    len += sprintf(buffer + len, "%d,", (int)trace.events[trace.read_item].policy);
    len += sprintf(buffer + len, "%d,", (int)trace.events[trace.read_item].prio);
    len += sprintf(buffer + len, "%d,", (int)trace.events[trace.read_item].pid);
    len += sprintf(buffer + len, "%d,", (int)trace.events[trace.read_item].state);
    len += sprintf(buffer + len, "%s\n", trace.events[trace.read_item].comm);
    increment(&trace.read_item);
    ret = 1;
  }

  raw_spin_unlock(&trace.lock);
  return ret;
}

static int enqueue(enum evt event, unsigned long long time,
                   struct task_struct *p) {
 
  raw_spin_lock(&trace.lock);
 
  if (is_full(trace.read_item, trace.write_item))
    increment(&trace.read_item);
  trace.events[trace.write_item].event = event;
  trace.events[trace.write_item].time = time;
  trace.events[trace.write_item].pid = p->pid;
  trace.events[trace.write_item].state = p->__state;
  trace.events[trace.write_item].prio = p->prio;
  trace.events[trace.write_item].policy = p->policy;
  strcpy(trace.events[trace.write_item].comm, p->comm);
  increment(&trace.write_item);
  raw_spin_unlock(&trace.lock);
  return 1;
}

ssize_t trace_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
  
  char buffer[TRACE_STRING_BUFFER_SIZE];
  int ret = 0, len = 0;

  if (!dequeue(buffer))
    return 0;
  len = strlen(buffer);
  if (len <= 0)
    return -EFAULT;
  if (count < len)
    return -EFAULT;
  ret = raw_copy_to_user(buf, buffer, len);
  if (ret != 0)
    return -EFAULT;
  return len;
}
 
static const struct proc_ops trace_ops = {
    .proc_read = trace_read,
};

static int __init proc_trace_init(void) {
  proc_create(TRACE_ENTRY_NAME, 0444, NULL, &trace_ops);
  printk("MOKER:/proc/%s created\n", TRACE_ENTRY_NAME);
  raw_spin_lock_init(&trace.lock);
  trace.write_item = 0;
  trace.read_item = 0;
  return 0;
}

module_init(proc_trace_init);

void moker_trace(enum evt event, struct task_struct *p) {
  unsigned long long time;
  if(enabled) {
		time = ktime_to_ns(ktime_get());
		enqueue(event, time, p);
	}
	
}

void trace_enable (unsigned int e){
	enabled = e;

}


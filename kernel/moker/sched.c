#include "../sched/sched.h"
#include "css_rq.h"
#include "css_task.h"
#include <linux/timekeeping.h>
#include <linux/unistd.h>

/*
 * Auxiliary functions
 *
 */

int isServerActive(struct css_server *server) {
  return server->state == Active || server->state == ActvResid;
}
/*
 * CSS scheduling class.
 * Implements SCHED_CSS
 */
static void enqueue_task_css(struct rq *rq, struct task_struct *p, int flags) {
  int sid, found;
  u64 arrivalTime;

  arrivalTime = ktime_get_ns();

  /* print for csv trace analisys */
  printk("CSS,ENQ_RQ,%d,%llu\n", p->pid, arrivalTime);

  raw_spin_lock(&rq->css.lock);

  /* Look for a Server that is serving this incoming task */
  found = 0;
  for (sid = 0; sid < _serversCount; sid++) {
    if (_serversList[sid].servedTask == p->pid) {
      found = 1;
      break;
    }
  }

  if (found) {
    if (isServerActive(serversList[sid])) {
      /* task if buffered and will be served later */
      printk("CSS_DB: early arrival of next job %d, buffer it\n", p->pid);
      _serversList[sid].d_deadline =
          _serversList[sid].d_deadline + _serversList[sid].T_period;

    } else if (_serversList[sid].state == Inactive ||
               _serversList[sid].state == InactvNonIso) {
      if (arrivalTime < _serversList[sid].d_deadline) {
        /* Server became active and
         * task served with current deadline and capacity
         */
        _serversList[sid].state = Active;
        /* dont restart the timer here, this is done in a switch_to event*/
        printk("CSS_DB: active server %d and use the capacity available\n",
               sid);
      } else {
        /* Case server is inactive & (a >= d) */
        /* Server became active and replenished */
        _serversList[sid].c_capacity = _serversList[sid].Q_maxCap;
        //_serversList[sid].previous_deadline = _serversList[sid].d_deadline;
        // do we need max(a,d-1) here?
        _serversList[sid].d_deadline = arrivalTime + _serversList[sid].T_period;
        _serversList[sid].h_replenish = _serversList[sid].d_deadline;
        _serversList[sid].r_residualCap = 0;

        _serversList[sid].state = Active;
        css_server_start_replenish_timer(&_serversList[sid]);
        printk("CSS_DB: active server %d and recharge its capacity\n", sid);
      }
    }

  } else {
    /* new unserved task, allocate a new server */
    _serversCount++;
    _serversList = krealloc(
        _serversList, (sizeof(struct css_server) * _serversCount), GFP_KERNEL);
    cssrq_init_css_server(&_serversList[sid]);
    _serversList[sid].servedTask = p->pid;

    printk("CSS_DB: new server for %d\n", _serversList[sid].servedTask);

    if (p->css.css_period == 0) {
      /* aperiodic task arrival */
      p->css.css_period = _serversList[sid].T_period;
      p->css.css_runtime = _serversList[sid].Q_maxCap;
      p->css.css_deadline = arrivalTime + p->css.css_period;
      _serversList[sid].d_deadline = p->css.css_deadline + arrivalTime;
      _serversList[sid].h_replenish = _serversList[sid].d_deadline;

    } else {
      /* periodic task arrival */
      p->css.css_deadline = arrivalTime + p->css.css_period;
      _serversList[sid].d_deadline = p->css.css_deadline + arrivalTime;
      _serversList[sid].h_replenish = _serversList[sid].d_deadline;
      _serversList[sid].T_period = p->css.css_period;
      _serversList[sid].c_capacity = p->css.css_runtime;
      _serversList[sid].Q_maxCap = _serversList[sid].c_capacity;
      printk("CSS_DB: periodic task %d capacity = %llu\n",
             _serversList[sid].servedTask, _serversList[sid].c_capacity);
    }

    p->css.css_job_state = Ready;
    /* server is ready now, activate it */
    _serversList[sid].state = Active;
  }

  /* Scheduling follow the EDF policy.
   * Each task is sorted in a rb_tree according its deadline.
   * Each CSS Server is bounded to a task (a number of tasks in future impl) by
   * its pid.
   */

  struct rb_node **new = &rq->css.root.rb_node;
  struct rb_node *parent = NULL;
  struct sched_css_entity *entry_node;

  while (*new) {
    parent = *new;
    entry_node = rb_entry(parent, struct sched_css_entity, node);
    if (entry_node->css_deadline > p->css.css_deadline)
      new = &((*new)->rb_left);
    else if (entry_node->css_deadline < p->css.css_deadline)
      new = &((*new)->rb_right);
    else
      return;
  }
  rb_link_node(&p->css.node, parent, new);
  rb_insert_color(&p->css.node, &rq->css.root);

  rq->css.nr_running++;
  add_nr_running(rq, 1);
  raw_spin_unlock(&rq->css.lock);
}
static void dequeue_task_css(struct rq *rq, struct task_struct *p, int flags) {
  /* print for csv trace analisys */
  printk("CSS,DEQ_RQ,%d,%llu\n", p->pid, ktime_get_ns());

  raw_spin_lock(&rq->css.lock);

  rb_erase(&p->css.node, &rq->css.root);
  rq->css.nr_running--;
  sub_nr_running(rq, 1);

  raw_spin_unlock(&rq->css.lock);
}
static void yield_task_css(struct rq *rq) {}
static bool yield_to_task_css(struct rq *rq, struct task_struct *p) {
  return true;
}
static void check_preempt_curr_css(struct rq *rq, struct task_struct *p,
                                   int flags) {
  switch (rq->curr->policy) {
  case SCHED_DEADLINE:
  case SCHED_FIFO:
  case SCHED_RR:
    break;
  case SCHED_NORMAL:
  case SCHED_BATCH:
  case SCHED_IDLE:
  // case SCHED_RESET_ON_FORK:
  case SCHED_CSS:
    resched_curr(rq);
    break;
  }
}

static struct task_struct *pick_next_task_css(struct rq *rq) {

  struct rb_node *first = NULL;
  struct task_struct *p = NULL;
  struct sched_css_entity *css_node;

  if (!RB_EMPTY_ROOT(&rq->css.root)) {

    raw_spin_lock(&rq->css.lock);

    // get 1st node (rb_node type) from rb tree
    first = rb_first(&rq->css.root);

    // get addr of sched_css_entity that contains that rb_node
    css_node = rb_entry(first, struct sched_css_entity, node);

    // get addr of the task_struct that contains that css
    p = container_of(css_node, struct task_struct, css);

    raw_spin_unlock(&rq->css.lock);
  }

  return p;
}
static void put_prev_task_css(struct rq *rq, struct task_struct *p) {}

static void set_next_task_css(struct rq *rq, struct task_struct *p,
                              bool first) {}

#ifdef CONFIG_SMP
static int balance_css(struct rq *rq, struct task_struct *p,
                       struct rq_flags *rf) {
  return 0;
}
static struct task_struct *pick_task_css(struct rq *rq) { return NULL; }
static int select_task_rq_css(struct task_struct *p, int cpu, int flags) {
  return cpu;
}
static void migrate_task_rq_css(struct task_struct *p,
                                int new_cpu __maybe_unused) {}
static void set_cpus_allowed_css(struct task_struct *p,
                                 const struct cpumask *new_mask, u32 flags) {}
static void rq_online_css(struct rq *rq) {}
static void rq_offline_css(struct rq *rq) {}
static void task_woken_css(struct rq *rq, struct task_struct *p) {}
static void task_dead_css(struct task_struct *p) {}
static struct rq *find_lock_rq_css(struct task_struct *task, struct rq *rq) {
  return NULL;
}
#endif
static void task_tick_css(struct rq *rq, struct task_struct *p, int queued) {}
static void task_fork_css(struct task_struct *p) {}
static void prio_changed_css(struct rq *rq, struct task_struct *p,
                             int oldprio) {}
static void switched_from_css(struct rq *rq, struct task_struct *p) {}
static void switched_to_css(struct rq *rq, struct task_struct *p) {
  /* This switch to means that the policy of a task has changed to css */
}
static unsigned int get_rr_interval_css(struct rq *rq,
                                        struct task_struct *task) {
  return 0;
}
static void update_curr_css(struct rq *rq) {}

DEFINE_SCHED_CLASS(css) = {
    .enqueue_task = enqueue_task_css,
    .dequeue_task = dequeue_task_css,
    .yield_task = yield_task_css,
    .yield_to_task = yield_to_task_css,
    .check_preempt_curr = check_preempt_curr_css,
    .pick_next_task = pick_next_task_css,
    .put_prev_task = put_prev_task_css,
    .set_next_task = set_next_task_css,
#ifdef CONFIG_SMP
    .balance = balance_css,
    .pick_task = pick_task_css,
    .select_task_rq = select_task_rq_css,
    .migrate_task_rq = migrate_task_rq_css,
    .set_cpus_allowed = set_cpus_allowed_css,
    .rq_online = rq_online_css,
    .rq_offline = rq_offline_css,
    .task_woken = task_woken_css,
    .task_dead = task_dead_css,
    .find_lock_rq = find_lock_rq_css,
#endif
    .task_tick = task_tick_css,
    .task_fork = task_fork_css,
    .prio_changed = prio_changed_css,
    .switched_from = switched_from_css,
    .switched_to = switched_to_css,
    .get_rr_interval = get_rr_interval_css,
    .update_curr = update_curr_css,
};

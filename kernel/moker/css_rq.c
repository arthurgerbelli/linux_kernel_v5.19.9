#include "css_rq.h"
#include "../sched/sched.h"
#include "css_task.h"
#include <linux/rbtree.h>

// CSS Run queue
void cssrq_init_css_rq(struct css_rq *rq) {
  rq->root = RB_ROOT;
  rq->serversManager.serversCount = 0;
  rq->serversManager.serversList = NULL;
  raw_spin_lock_init(&rq->lock);
  rq->nr_running = 0;
}

void cssrq_init_css_server(struct css_server *server) {

  server->servedTask = 0;
  server->Q_maxCap = Q_MAXCAPACITY;
  server->T_period = T_PERIOD;
  server->c_capacity = server->Q_maxCap;
  server->r_residualCap = 0;
  server->d_deadline = 0;  /* set at job arrival */
  server->h_replenish = 0; /* set after deadline*/
  server->state = Inactive;
  server->previous_deadline = 0;

  hrtimer_init(&server->run_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hrtimer_init(&server->replenish_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hrtimer_init(&server->deadline_monitor_timer, CLOCK_MONOTONIC,
               HRTIMER_MODE_REL);
}

enum hrtimer_restart timer_callback_deadline_reached(struct hrtimer *timer) {

  struct css_server *server =
      container_of(timer, struct css_server, deadline_monitor_timer);
  printk("CSS_DB: Deadline of %d reached!\n", server->servedTask);
  return HRTIMER_NORESTART;
}

void cssrq_trigger_server_start(struct rq *rq, struct task_struct *p) {
  // TODO: use hashtable or pointers to task_struct
  int sid, found;
  raw_spin_lock(&rq->css.lock);
  found = 0;
  for (sid = 0; sid < _serversCount; sid++) {
    if (_serversList[sid].servedTask == p->pid) {
      found = 1;
      break;
    }
  }
  if (found) {
    p->css.css_job_state = Running; // TODO: after refactor servedTask, move it
                                    // to css_server_start_run
    css_server_start_run(&_serversList[sid], _serversList[sid].c_capacity);

    if (_serversList[sid].c_capacity == _serversList[sid].Q_maxCap) {
      /* This means that is a fresh run, a deadline has just been generated.
       * We will monitor if the job will miss it by using another timer.
       */
      _serversList[sid].deadline_ktime =
          ktime_set(0, _serversList[sid].d_deadline);
      hrtimer_start(&_serversList[sid].deadline_monitor_timer,
                    _serversList[sid].deadline_ktime, HRTIMER_MODE_REL);
      _serversList[sid].deadline_monitor_timer.function =
          &timer_callback_deadline_reached;
    }
  }

  raw_spin_unlock(&rq->css.lock);
  //-------------------------------------------
}

void cssrq_interrupt_server(struct rq *rq, struct task_struct *p) {
  /* Look for a Server that is serving this incoming task */
  int sid, found = 0;
  raw_spin_lock(&rq->css.lock);
  found = 0;
  for (sid = 0; sid < _serversCount; sid++) {
    if (_serversList[sid].servedTask == p->pid) {
      found = 1;
      break;
    }
  }

  if (found) {
    p->css.css_job_state = Ready;
    hrtimer_cancel(&_serversList[sid].run_timer);
    _serversList[sid].c_capacity =
        hrtimer_get_remaining(&_serversList[sid].run_timer);
    printk("CSS_DB: Server %d interrupted, capacity decremented to %llu\n",
           _serversList[sid].servedTask, _serversList[sid].c_capacity);
    /* server remains active */
    printk("CSS_DB: Server %d state: %d\n", _serversList[sid].servedTask,
           _serversList[sid].state);
  }
  raw_spin_unlock(&rq->css.lock);
}

void cssrq_stop_server(struct rq *rq, struct task_struct *p) {
  /* Look for a Server that is serving this incoming task */
  int sid, found;
  u64 remaining_time;
  raw_spin_lock(&rq->css.lock);
  found = 0;
  for (sid = 0; sid < _serversCount; sid++) {
    if (_serversList[sid].servedTask == p->pid) {
      found = 1;
      break;
    }
  }

  if (found && (p->css.css_job_state == Running)) {

    p->css.css_job_state = Done;

    hrtimer_cancel(&_serversList[sid].deadline_monitor_timer);

    hrtimer_cancel(&_serversList[sid].run_timer);
    remaining_time = hrtimer_get_remaining(&_serversList[sid].run_timer);

    if (remaining_time > 0) {
      _serversList[sid].r_residualCap = remaining_time;
      _serversList[sid].c_capacity = 0;
      _serversList[sid].state = ActvResid;
      printk("CSS_DB: Server %d stopped with r = %llu\n",
             _serversList[sid].servedTask, _serversList[sid].r_residualCap);
    } else {
      /* rare case */
      _serversList[sid].state = Inactive;
      printk("CSS_DB: Server %d went to inactive\n",
             _serversList[sid].servedTask);
    }
  }
  raw_spin_unlock(&rq->css.lock);
}
//------------------------------------------------------------------

// CSS Server
enum hrtimer_restart timer_callback_capacity_exhausted(struct hrtimer *timer) {

  struct css_server *server = container_of(timer, struct css_server, run_timer);
  printk("CSS_DB: [T: %llu] Capacity of %d exhausted!\n", ktime_get_ns(),server->servedTask);
  server->previous_deadline = server->d_deadline;
  server->d_deadline = server->d_deadline + server->T_period;
  // TODO: schedule() next server; maybe?
  return HRTIMER_NORESTART;
}

void css_server_start_run(struct css_server *server, u64 capacity) {
  /*
   * Starts the timer used to decrease the capacity and set the
   * remaining capacity.
   */
  printk(
      "CSS_DB: [T: %llu] server of task %d start running, with capacity %llu\n",
      ktime_get_ns(), server->servedTask, capacity);
  // TODO: p->css.css_job_state = Running;
  server->state = Active;
  server->run_ktime = ktime_set(0, capacity);
  hrtimer_start(&server->run_timer, server->run_ktime, HRTIMER_MODE_REL);
  server->run_timer.function = &timer_callback_capacity_exhausted;
}

enum hrtimer_restart timer_callback_replenishment(struct hrtimer *timer) {

  struct css_server *server = container_of(timer, struct css_server, run_timer);
  printk("CSS_DB: Replenishment for %d\n", server->servedTask);
  if (server->state == ActvResid) {
    /* Server has no pending job */
    server->state = Inactive;
    printk("CSS_DB: Server %d went to inactive\n", server->servedTask);
    // TODO: Implement the case for stealing
  } else if (server->state == Active) {
    /* TODO: Server has pending job
     *  - Replenish and generate a new deadline */
  }

  return HRTIMER_NORESTART;
}
void css_server_start_replenish_timer(struct css_server *server) {
  server->replenish_ktime = ktime_set(0, server->h_replenish);
  hrtimer_start(&server->replenish_timer, server->replenish_ktime,
                HRTIMER_MODE_REL);
  server->replenish_timer.function = &timer_callback_replenishment;
}
//------------------------------------------------------------------

/*
 * Setter / Getter for css attributes
 */

void __setparam_css(struct task_struct *p, const struct sched_attr *attr) {
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

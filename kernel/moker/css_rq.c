#include "css_rq.h"
#include "../sched/sched.h"
#include "css_task.h"
#include <linux/rbtree.h>

// CSS Run queue
void cssrq_init_css_rq(struct css_rq *rq) {
  rq->root = RB_ROOT;
  rq->serversManager.serversCount = 0;
  memset(rq->serversManager.serversList, 0,
         sizeof(struct css_server) * MAXSERVERS);
  rq->serversManager.sedf_ptr = kmalloc(sizeof(struct css_server), GFP_KERNEL);
  rq->serversManager.sedf_ptr = NULL;
  raw_spin_lock_init(&rq->lock);
  rq->nr_running = 0;
}

void cssrq_init_css_server(struct css_server *server) {
  server->job = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
  server->job = NULL;
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
  printk("CSS_DB: Deadline of %d reached!\n", server->job->pid);
  return HRTIMER_NORESTART;
}

void cssrq_trigger_server_start(struct rq *rq, struct task_struct *p) {
  int sid;
  sid = css_server_get_sid(rq, p);

  raw_spin_lock(&rq->css.lock);

  if (_pSedf != NULL) {
    /* Ar residual capacity available */
    css_server_consume_residual(&_serversList[sid], _pSedf);
  } else {
    /* Uses its own capacity */
    css_server_start_run(&_serversList[sid], _serversList[sid].c_capacity);
  }

  if (_serversList[sid].state == Active &&
      _serversList[sid].r_residualCap > 0) {
    /* This means that the next job already arrived, if its deadline is the
     * earliest, use its own residual capacity to run.
     */
    printk("CSS_DB: executes early arrival of %d\n", p->pid);
    css_server_start_run(&_serversList[sid], _serversList[sid].r_residualCap);
  } else {
    css_server_start_run(&_serversList[sid], _serversList[sid].c_capacity);
  }

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

  raw_spin_unlock(&rq->css.lock);
  //-------------------------------------------
}

void cssrq_interrupt_server(struct rq *rq, struct task_struct *p) {
  /* Look for a Server that is serving this incoming task */
  int sid;
  sid = css_server_get_sid(rq, p);

  raw_spin_lock(&rq->css.lock);

  p->css.css_job_state = Ready;
  hrtimer_cancel(&_serversList[sid].run_timer);
  _serversList[sid].c_capacity =
      hrtimer_get_remaining(&_serversList[sid].run_timer);
  printk("CSS_DB: Server %d interrupted, capacity decremented to %llu\n",
         _serversList[sid].job->pid, _serversList[sid].c_capacity);
  /* server remains active */
  printk("CSS_DB: Server %d state: %d\n", _serversList[sid].job->pid,
         _serversList[sid].state);

  raw_spin_unlock(&rq->css.lock);
}

void cssrq_stop_server(struct rq *rq, struct task_struct *p) {
  /* Look for the Server that is serving this incoming task */
    u64 remaining_time;
  int sid;
  sid = css_server_get_sid(rq, p);

  raw_spin_lock(&rq->css.lock);

  if (p->css.css_job_state == Running) {

    p->css.css_job_state = Done;

    hrtimer_cancel(&_serversList[sid].deadline_monitor_timer);

    remaining_time = hrtimer_get_remaining(&_serversList[sid].run_timer);
    hrtimer_cancel(&_serversList[sid].run_timer);

    if (remaining_time > 0) {
      _serversList[sid].r_residualCap = remaining_time;
      _serversList[sid].c_capacity = 0;
      if (_pSedf == NULL) {
        /* it became the first Ar available */
        _pSedf = &_serversList[sid];
      } else {
        /* TODO: to create a list for Sedf? Linked list?*/
      }

      //_serversList[sid].state = ActvResid;

      printk("CSS_DB: Server %d stopped with r = %llu\n",
             _serversList[sid].job->pid, _serversList[sid].r_residualCap);
      printk("CSS_DB: Sedf points to r = %llu\n", _pSedf->r_residualCap);
    } else {
      /* rare case */
      _serversList[sid].state = Inactive;
      printk("CSS_DB: Server %d went to inactive\n",
             _serversList[sid].job->pid);
      hrtimer_cancel(&_serversList[sid].replenish_timer);
    }
  }
  raw_spin_unlock(&rq->css.lock);
}
//------------------------------------------------------------------

// CSS Server
enum hrtimer_restart timer_callback_residual_exhausted(struct hrtimer *timer) {

  static bool changed_to_capacity = false;

  if (!changed_to_capacity) {
    struct css_server *server =
        container_of(timer, struct css_server, run_timer);
    printk("CSS_DB: [T: %llu] Residual Cap. used by %d exhausted!\n",
           ktime_get_ns(), server->job->pid);

    /* Find address of Sedf */
    struct css_servers_manager *pServers_manager = NULL;
    struct css_server *pServers_list = &(pServers_manager->serversList[server->sid]);
    pServers_manager =
        container_of(pServers_list, struct css_servers_manager, serversList);

    // TODO: check for next residual cap.
    //       for now we are assuming just one
    pServers_manager->sedf_ptr->r_residualCap = 0;
    if (ktime_get_ns() < pServers_manager->sedf_ptr->d_deadline) {
      pServers_manager->sedf_ptr->state = Inactive;
    }

    /* restore its deadline */
    server->job->css.css_abs_deadline = server->d_deadline;

    // TODO: implement a shared resource control for css_job_state
    if (server->job->css.css_job_state == Running) {
      /* start to decrement its own capacity */
      server->run_ktime = ktime_set(0, server->c_capacity);
      hrtimer_forward_now(timer, server->run_ktime);

      changed_to_capacity = true;
      return HRTIMER_RESTART;
    }

  } else {
    handle_capacity_exhausted(server);
    changed_to_capacity = false;
  }

  return HRTIMER_NORESTART;
}

void css_server_consume_residual(struct css_server *server,
                                 struct css_server *sedf) {
  printk("CSS_DB: [T: %llu] server of task %d start comsuming Residual cap. "
         "%llu\n",
         ktime_get_ns(), server->job->pid, sedf->r_residualCap);

  /* The job runs under the deadline from the server which it is reclaiming
   * capacity*/
  server->job->css.css_abs_deadline = sedf->d_deadline;
  /* NOTE:
   * By changing its css_abs_deadline, we are change the value from one node
   * of the rb_tree without re-balacing it. There is no problem because this
   * is the task with the earliest deadline now, when a new task enqueues it
   * will re-balace anyway.
   */

  server->run_ktime = ktime_set(0, sedf->r_residualCap);
  hrtimer_start(&server->run_timer, server->run_ktime, HRTIMER_MODE_REL);
  server->run_timer.function = &timer_callback_residual_exhausted;
}

enum hrtimer_restart timer_callback_capacity_exhausted(struct hrtimer *timer) {

  struct css_server *server = container_of(timer, struct css_server, run_timer);
  handle_capacity_exhausted(server);
  return HRTIMER_NORESTART;
}

void handle_capacity_exhausted(struct css_server *server) {
  static bool is_any_to_steal = false;

  printk("CSS_DB: [T: %llu] Capacity of %d exhausted!\n", ktime_get_ns(),
         server->job->pid);
  if (is_any_to_steal) {
    // TODO: try to steal inactive capacity
  } else {
    /* Server depleted */
    server->d_deadline = server->d_deadline + server->T_period;
    server->job->css.css_abs_deadline = server->deadline;
    css_rq_remove_task_from_rq_rbtree(server->job);
    css_rq_add_task_to_rq_rbtree(server->job);
    schedule();
  }
}

void css_server_start_run(struct css_server *server, u64 capacity) {
  /*
   * Starts the timer used to decrease the capacity and set the
   * remaining capacity.
   */
  printk("CSS_DB: [T: %llu] server of task %d start running, with capacity "
         "%llu\n",
         ktime_get_ns(), server->job->pid, capacity);

          server->job->css.css_job_state = Running;

  server->run_ktime = ktime_set(0, capacity);
  hrtimer_start(&server->run_timer, server->run_ktime, HRTIMER_MODE_REL);
  server->run_timer.function = &timer_callback_capacity_exhausted;

  // TODO: p->css.css_job_state = Running;
  server->state = Active;
  server->run_ktime = ktime_set(0, capacity);
  hrtimer_start(&server->run_timer, server->run_ktime, HRTIMER_MODE_REL);
  server->run_timer.function = &timer_callback_capacity_exhausted;
}

enum hrtimer_restart timer_callback_replenishment(struct hrtimer *timer) {

  struct css_server *server = container_of(timer, struct css_server, run_timer);
  printk("CSS_DB: Replenishment for %d\n", server->job->pid);
  if (server->state == ActvResid) {
    /* Server has no pending job */
    server->state = Inactive;
    printk("CSS_DB: Server %d went to inactive\n", server->job->pid);
    // TODO: Implement the case for stealing
  } else if (server->state == Active) {
    /* Replenish */
    server->c_capacity = server->Q_maxCap;
    server->r_residualCap = 0;
    if (server->d_deadline > server->h_replenish) {
      /* Means that the deadline were already generated at job enqueue */
      printk("CSS_DB: Server %d deadline's already renewed\n",
             server->job->pid);
    } else {
      server->d_deadline = server->d_deadline + server->T_period;
    }
    server->h_replenish = server->d_deadline;

    server->replenish_ktime = ktime_set(0, server->h_replenish);
    hrtimer_forward_now(timer, server->replenish_ktime);
    return HRTIMER_RESTART;
  }

  return HRTIMER_NORESTART;
}

void css_server_start_replenish_timer(struct css_server *server) {
  server->replenish_ktime = ktime_set(0, server->h_replenish);
  hrtimer_start(&server->replenish_timer, server->replenish_ktime,
                HRTIMER_MODE_REL);
  server->replenish_timer.function = &timer_callback_replenishment;
}

int css_server_get_sid(struct rq *rq, struct task_struct *p) {
  int sid, found;
  found = 0;
  for (sid = 0; sid < MAXSERVERS; sid++) {
    if (_serversList[sid].job->pid == p->pid) {
      found = 1;
      break;
    }
  }

  return found ? sid : -1;
}

//------------------------------------------------------------------

/*
 * Setter / Getter for css attributes
 */

void __setparam_css(struct task_struct *p, const struct sched_attr *attr) {
  struct sched_css_entity *css_se = &p->css;
  css_se->css_runtime = attr->sched_runtime;
  css_se->css_deadline = attr->sched_deadline;
  css_se->css_period = attr->sched_period;
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

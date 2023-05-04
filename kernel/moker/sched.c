#include "../sched/sched.h"
/*
 * LIFO scheduling class.
 * Implements SCHED_LIFO
 */
static void enqueue_task_lf(struct rq *rq, struct task_struct *p, int flags) {
raw_spin_lock(&rq->lf.lock);
list_add(&p->lf.node,&rq->lf.tasks);
rq->lf.task = p;
rq->lf.nr_running++;
add_nr_running(rq, 1);
raw_spin_unlock(&rq->lf.lock);

#ifdef CONFIG_MOKER_TRACING
moker_trace(ENQUEUE_RQ, p);
#endif

}
static void dequeue_task_lf(struct rq *rq, struct task_struct *p, int flags) {
struct sched_lf_entity *t = NULL;
raw_spin_lock(&rq->lf.lock);
list_del(&p->lf.node);
if(list_empty(&rq->lf.tasks)){
	rq->lf.task = NULL;
}else{
t = list_first_entry(&rq->lf.tasks,struct sched_lf_entity, node);
rq->lf.task = container_of(t,struct task_struct, lf);
}
rq->lf.nr_running--;
sub_nr_running(rq, 1);
raw_spin_unlock(&rq->lf.lock);

#ifdef CONFIG_MOKER_TRACING
moker_trace(DEQUEUE_RQ, p);
#endif
}
static void yield_task_lf(struct rq *rq) {}
static bool yield_to_task_lf(struct rq *rq, struct task_struct *p) {
  return true;
}
static void check_preempt_curr_lf(struct rq *rq, struct task_struct *p, int flags) {

switch(rq->curr->policy){
	case SCHED_DEADLINE:
	case SCHED_FIFO:
	case SCHED_RR:
		break;
	case SCHED_NORMAL:
	case SCHED_BATCH:
	case SCHED_IDLE:
	//case SCHED_RESET_ON_FORK:
	case SCHED_LIFO:
		resched_curr(rq);
		break;
	}
}

static struct task_struct *pick_next_task_lf(struct rq *rq) { 

struct task_struct * p = NULL;
raw_spin_lock(&rq->lf.lock);
p = rq->lf.task;
raw_spin_unlock(&rq->lf.lock);
return p;

}
static void put_prev_task_lf(struct rq *rq, struct task_struct *p) {}

static void set_next_task_lf(struct rq *rq, struct task_struct *p, bool first) {
}

#ifdef CONFIG_SMP
static int balance_lf(struct rq *rq, struct task_struct *p,
                      struct rq_flags *rf) {
  return 0;

}
static struct task_struct *pick_task_lf(struct rq *rq) { return NULL; }
static int select_task_rq_lf(struct task_struct *p, int cpu, int flags) {
  return cpu;
}
static void migrate_task_rq_lf(struct task_struct *p,
                               int new_cpu __maybe_unused) {}
static void set_cpus_allowed_lf(struct task_struct *p,
                                const struct cpumask *new_mask, u32 flags) {}
static void rq_online_lf(struct rq *rq) {}
static void rq_offline_lf(struct rq *rq) {}
static void task_woken_lf(struct rq *rq, struct task_struct *p) {}
static void task_dead_lf(struct task_struct *p) {}
static struct rq *find_lock_rq_lf(struct task_struct *task, struct rq *rq) {
  return NULL;
}
#endif
static void task_tick_lf(struct rq *rq, struct task_struct *p, int queued) {}
static void task_fork_lf(struct task_struct *p) {}
static void prio_changed_lf(struct rq *rq, struct task_struct *p, int oldprio) {
}
static void switched_from_lf(struct rq *rq, struct task_struct *p) {}
static void switched_to_lf(struct rq *rq, struct task_struct *p) {}
static unsigned int get_rr_interval_lf(struct rq *rq,
                                       struct task_struct *task) {
  return 0;
}
static void update_curr_lf(struct rq *rq) {}
DEFINE_SCHED_CLASS(lf) = {
    .enqueue_task = enqueue_task_lf,
    .dequeue_task = dequeue_task_lf,
    .yield_task = yield_task_lf,
    .yield_to_task = yield_to_task_lf,
    .check_preempt_curr = check_preempt_curr_lf,
    .pick_next_task = pick_next_task_lf,
    .put_prev_task = put_prev_task_lf,
    .set_next_task = set_next_task_lf,
#ifdef CONFIG_SMP
    .balance = balance_lf,
    .pick_task = pick_task_lf,
    .select_task_rq = select_task_rq_lf,
    .migrate_task_rq = migrate_task_rq_lf,
    .set_cpus_allowed = set_cpus_allowed_lf,
    .rq_online = rq_online_lf,
    .rq_offline = rq_offline_lf,
    .task_woken = task_woken_lf,
    .task_dead = task_dead_lf,
    .find_lock_rq = find_lock_rq_lf,
#endif
    .task_tick = task_tick_lf,
    .task_fork = task_fork_lf,
    .prio_changed = prio_changed_lf,
    .switched_from = switched_from_lf,
    .switched_to = switched_to_lf,
    .get_rr_interval = get_rr_interval_lf,
    .update_curr = update_curr_lf,
};


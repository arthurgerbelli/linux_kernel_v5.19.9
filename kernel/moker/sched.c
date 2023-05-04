#include "../sched/sched.h"
/*
 * CSS scheduling class.
 * Implements SCHED_CSS
 */
static void enqueue_task_css(struct rq *rq, struct task_struct *p, int flags) {
raw_spin_lock(&rq->css.lock);
list_add(&p->css.node,&rq->css.tasks);
rq->css.task = p;
rq->css.nr_running++;
add_nr_running(rq, 1);
raw_spin_unlock(&rq->css.lock);

#ifdef CONFIG_MOKER_TRACING
moker_trace(ENQUEUE_RQ, p);
#endif

}
static void dequeue_task_css(struct rq *rq, struct task_struct *p, int flags) {
struct sched_css_entity *t = NULL;
raw_spin_lock(&rq->css.lock);
list_del(&p->css.node);
if(list_empty(&rq->css.tasks)){
	rq->css.task = NULL;
}else{
t = list_first_entry(&rq->css.tasks,struct sched_css_entity, node);
rq->css.task = container_of(t,struct task_struct, css);
}
rq->css.nr_running--;
sub_nr_running(rq, 1);
raw_spin_unlock(&rq->css.lock);

#ifdef CONFIG_MOKER_TRACING
moker_trace(DEQUEUE_RQ, p);
#endif
}
static void yield_task_css(struct rq *rq) {}
static bool yield_to_task_css(struct rq *rq, struct task_struct *p) {
  return true;
}
static void check_preempt_curr_css(struct rq *rq, struct task_struct *p, int flags) {

switch(rq->curr->policy){
	case SCHED_DEADLINE:
	case SCHED_FIFO:
	case SCHED_RR:
		break;
	case SCHED_NORMAL:
	case SCHED_BATCH:
	case SCHED_IDLE:
	//case SCHED_RESET_ON_FORK:
	case SCHED_CSS:
		resched_curr(rq);
		break;
	}
}

static struct task_struct *pick_next_task_css(struct rq *rq) { 

struct task_struct * p = NULL;
raw_spin_lock(&rq->css.lock);
p = rq->css.task;
raw_spin_unlock(&rq->css.lock);
return p;

}
static void put_prev_task_css(struct rq *rq, struct task_struct *p) {}

static void set_next_task_css(struct rq *rq, struct task_struct *p, bool first) {
}

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
static void prio_changed_css(struct rq *rq, struct task_struct *p, int oldprio) {
}
static void switched_from_css(struct rq *rq, struct task_struct *p) {}
static void switched_to_css(struct rq *rq, struct task_struct *p) {}
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


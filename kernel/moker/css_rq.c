#include "css_rq.h"

void init_css_rq(struct css_rq *rq){
  INIT_LIST_HEAD(&rq->tasks);
  raw_spin_lock_init(&rq->lock);
  rq->task = NULL;
  rq->nr_running = 0;
}


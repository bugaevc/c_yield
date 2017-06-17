#ifndef GEN_H
#define GEN_H

// for both
typedef void gen_t;

// for callee
gen_t *gen_init(void);
void *gen_yield(gen_t *,void *);
void gen_return(gen_t *,void *);

// for caller
void *gen_send(gen_t *, void *);
int gen_is_done(gen_t *);

#endif

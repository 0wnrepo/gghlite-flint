#include <gghlite/gghlite.h>
#include "nike.h"

int main(int argc, char *argv[]) {
  cmdline_params_t cmdline_params;

  parse_cmdline(cmdline_params, argc, argv);
  int verbose = (cmdline_params->flags & GGHLITE_FLAGS_VERBOSE);

  printf("####################################################################\n");
  if (verbose)
    printf("%s\n", name);
  else
    printf("NIKE\n");
  printf(" λ: %3ld, N: %2ld                              seed: 0x%016lx\n",
         cmdline_params->lambda,
         cmdline_params->N,
         cmdline_params->seed);
  printf("#############################################all logs are base two##\n\n");

  flint_rand_t randstate;
  flint_randinit_seed(randstate, cmdline_params->seed, 1);


  if (verbose)
    print_intro();

  printf("-----------------------------------------------------\n");
  printf("Step 1: GCHQ runs Setup:\n");
  printf("-----------------------------------------------------\n");

  uint64_t t = ggh_walltime(0);
  uint64_t t_total = ggh_walltime(0);

  gghlite_t self;

  long mlm_lambda;
  if (cmdline_params->flags & GGHLITE_FLAGS_GDDH_HARD)
    mlm_lambda = cmdline_params->lambda;
  else
    mlm_lambda = 2*cmdline_params->lambda+1;

  gghlite_pk_init_params(self->pk, mlm_lambda, cmdline_params->N-1, 1<<0,
                         cmdline_params->flags);
  gghlite_print_params(self->pk);
  printf("\n---\n");
  gghlite_init_instance(self, randstate);

  gghlite_pk_t params;
  gghlite_pk_ref(params, self);
  gghlite_clear(self, 0);

  printf("\n");
  printf("wall time: %.2f s\n\n", ggh_walltime(t)/1000000.0);

  printf("-----------------------------------------------------\n");
  printf("Step 2: Publish");
  if (verbose)
    printf("\n-----------------------------------------------------\n");

  t = ggh_walltime(0);

  gghlite_enc_t *e = calloc(2, sizeof(gghlite_enc_t));
  gghlite_enc_t *u = calloc(2, sizeof(gghlite_enc_t));
  gghlite_enc_t *b = calloc(2, sizeof(gghlite_enc_t));
  gghlite_clr_t *s = calloc(2, sizeof(gghlite_clr_t));

  for(int i=0; i<2; i++) {
    if (verbose)
      printf("%8s samples e_%d, ",agents[i],i);
    gghlite_enc_init(e[i], params);
    gghlite_sample(e[i], params, 0, 0, randstate);

    if (verbose)
      printf("computes u_%d and publishes it\n", i);
    gghlite_enc_init(u[i], params);
    gghlite_elevate(u[i], params, e[i], 1, 0, 0, 1, randstate);
  }

  if (verbose)
    printf("\n… and so on\n\n");
  else
    printf("  ");
  t = ggh_walltime(t);
  printf("wall time: %.2f s, per party: %.2f s\n", t/1000000.0,t/1000000.0/2);

  printf("-----------------------------------------------------\n");
  printf("Step 3: KeyGen");
  if (verbose)
    printf("\n-----------------------------------------------------\n");

  t = ggh_walltime(0);
  uint64_t t_sample = 0;

  for(int i=0; i<2; i++) {
    gghlite_enc_init(b[i], params);
    gghlite_enc_set_ui(b[i], 1, params);
    gghlite_mul(b[i], params, b[i], e[i]);
  }

  if(verbose)
    printf("%8s computes e_%d·u_%d", agents[0], 0, 1);

  gghlite_enc_t tmp;
  gghlite_enc_init(tmp, params);
  for(int j=2; j<cmdline_params->N; j++) {
    uint64_t t_s = ggh_walltime(0);
    gghlite_sample(tmp, params, 0, 0, randstate);
    gghlite_elevate(tmp, params, tmp, 1, 0, 0, 1, randstate);
    t_s = ggh_walltime(t_s);
    t_sample += t_s;

    if(verbose)
      printf("·u_%d", j);

    for(int i=0; i<2; i++)
      gghlite_mul(b[i], params, b[i], tmp);
  }
  gghlite_enc_clear(tmp);

  gghlite_mul(b[0], params, b[0], e[1]);
  gghlite_mul(b[1], params, b[1], e[0]);

  for(int i=0; i<2; i++) {
    gghlite_clr_init(s[i]);
    gghlite_extract(s[i], params, b[i]);
    gghlite_enc_clear(b[i]);
  }

  if (verbose)
    printf("\n\n… and so on\n\n");
  else
    printf("  ");

  t = ggh_walltime(t) - t_sample;
  printf("wall time: %.2f s, per party: %.2f s\n", t/1000000.0,t/1000000.0/2);


  printf("-----------------------------------------------------\n");
  printf(" Check: ");
  if (verbose)
    printf("\n-----------------------------------------------------\n");

  int ret = 0;
  if(verbose)
    printf("s_0 == s_1: ");
  assert(!fmpz_poly_is_zero(s[0]));
  if (gghlite_clr_equal(s[1], s[0])) {
    verbose ? printf("TRUE\n")  : printf("+");
  } else {
    verbose ? printf("FALSE\n") : printf("-");
    ret = -1;
  }

  printf("\n");
  if (!verbose)
    printf("-----------------------------------------------------\n");


  printf("λ: %3ld, N: %2ld, n: %6ld, seed: 0x%08lx, prime: %ld, success: %d, time: %10.2fs\n",
         cmdline_params->lambda, cmdline_params->N, params->n, cmdline_params->seed, cmdline_params->flags & GGHLITE_FLAGS_PRIME_G,
         ret==0, ggh_walltime(t_total)/1000000.0);

  for(int i=0; i<2; i++) {
    gghlite_enc_clear(e[i]);
    gghlite_enc_clear(u[i]);
    gghlite_clr_clear(s[i]);
  }
  free(e);
  free(u);
  free(s);
  free(b);
  gghlite_pk_clear(params);

  flint_randclear(randstate);
  mpfr_free_cache();
  flint_cleanup();
  return ret;
}

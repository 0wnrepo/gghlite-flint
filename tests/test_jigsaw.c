#include <gghlite/gghlite.h>
#include <gghlite/gghlite-internals.h>

int test_jigsaw(const size_t lambda, const size_t kappa, int rerand, int symmetric, flint_rand_t randstate) {

  printf("λ: %4zu, κ: %2zu, rerand: %d, symmetric: %d …", lambda, kappa, rerand, symmetric);

  gghlite_sk_t self;
  gghlite_flag_t flags = GGHLITE_FLAGS_GDDH_HARD | GGHLITE_FLAGS_QUIET | GGHLITE_FLAGS_GOOD_G_INV;
  if (!symmetric)
    flags |= GGHLITE_FLAGS_ASYMMETRIC;

  gghlite_init(self, lambda, kappa, rerand, flags, randstate);

  fmpz_t p; fmpz_init(p);
  fmpz_poly_oz_ideal_norm(p, self->g, self->params->n, 0);


  fmpz_t a[kappa];
  fmpz_t acc;  fmpz_init(acc);
  fmpz_set_ui(acc, 1);

  for(size_t k=0; k<kappa; k++) {
    fmpz_init(a[k]);
    fmpz_randm(a[k], randstate, p);
    fmpz_mul(acc, acc, a[k]);
    fmpz_mod(acc, acc, p);
  }

  gghlite_clr_t e[kappa];
  gghlite_enc_t u[kappa];

  for(size_t k=0; k<kappa; k++) {
    gghlite_clr_init(e[k]);
    gghlite_enc_init(u[k], self->params);
  }

  gghlite_enc_t left;
  gghlite_enc_init(left, self->params);
  gghlite_enc_set_ui0(left, 1, self->params);

  for(size_t k=0; k<kappa; k++) {
    fmpz_poly_set_coeff_fmpz(e[k], 0, a[k]);
    const int i = (gghlite_sk_is_symmetric(self)) ? 0 : k;
    gghlite_enc_set_gghlite_clr(u[k], self, e[k], 1, i, rerand, randstate);
    gghlite_enc_mul(left, self->params, left, u[k]);
  }

  gghlite_enc_t rght;
  gghlite_enc_init(rght, self->params);
  gghlite_enc_set_ui0(rght, 1, self->params);

  fmpz_poly_t tmp; fmpz_poly_init(tmp);
  fmpz_poly_set_coeff_fmpz(tmp, 0, acc);
  gghlite_enc_set_gghlite_clr0(rght, self, tmp, randstate);

  for(size_t k=0; k<kappa; k++) {
    const int i = (gghlite_sk_is_symmetric(self)) ? 0 : k;
    gghlite_enc_set_ui(u[k], 1, self->params, 1, i, rerand, randstate);
    gghlite_enc_mul(rght, self->params, rght, u[k]);
  }

  gghlite_enc_sub(rght, self->params, rght, left);
  int status = 1 - gghlite_enc_is_zero(self->params, rght);

  for(size_t i=0; i<kappa; i++) {
    fmpz_clear(a[i]);
    gghlite_clr_clear(e[i]);
    gghlite_enc_clear(u[i]);
  }

  gghlite_enc_clear(left);
  gghlite_enc_clear(rght);
  gghlite_clr_clear(tmp);
  fmpz_clear(p);
  gghlite_sk_clear(self, 1);

  if (status == 0)
    printf(" PASS\n");
  else
    printf(" FAIL\n");

  return status;
}


int main(int argc, char *argv[]) {
  flint_rand_t randstate;
  flint_randinit_seed(randstate, 0x1337, 1);

  int status = 0;

  status += test_jigsaw(20, 2, 0, 1, randstate);
  status += test_jigsaw(20, 3, 0, 1, randstate);
  status += test_jigsaw(20, 4, 0, 1, randstate);
  status += test_jigsaw(20, 2, 1, 1, randstate);
  status += test_jigsaw(20, 3, 1, 1, randstate);
  status += test_jigsaw(20, 4, 1, 1, randstate);

  status += test_jigsaw(20, 2, 0, 0, randstate);
  status += test_jigsaw(20, 3, 0, 0, randstate);
  status += test_jigsaw(20, 4, 0, 0, randstate);
  status += test_jigsaw(20, 2, 1, 0, randstate);
  status += test_jigsaw(20, 3, 1, 0, randstate);
  status += test_jigsaw(20, 4, 1, 0, randstate);

  flint_randclear(randstate);
  flint_cleanup();
  mpfr_free_cache();
  return status;
}
#include <string.h>
#include "gghlite-internals.h"
#include "gghlite.h"
#include "oz/oz.h"
#include "oz/flint-addons.h"

void _gghlite_zero(gghlite_t self) {
  memset(self, 0, sizeof(struct _gghlite_struct));
}

void _gghlite_set_y(gghlite_t self) {
  assert(!fmpz_poly_is_zero(self->a));
  assert(!fmpz_is_zero(self->pk->q));
  assert(!fmpz_mod_poly_is_zero(self->z_inv));

  fmpz_mod_poly_t tmp;  fmpz_mod_poly_init(tmp, self->pk->q);
  fmpz_mod_poly_oz_ntt_enc_fmpz_poly(tmp, self->a, self->pk->ntt);
  fmpz_mod_poly_oz_ntt_mul(tmp, tmp, self->z_inv, self->pk->n);

  fmpz_mod_poly_init(self->pk->y, self->pk->q);
  fmpz_mod_poly_set(self->pk->y, tmp);

  fmpz_mod_poly_clear(tmp);
}

void _gghlite_sample_a(gghlite_t self, flint_rand_t randstate) {
  assert(self->D_g);

  fmpz_poly_init(self->a);
  uint64_t t = ggh_walltime(0);
  fmpz_poly_sample_D_plus1(self->a, self->D_g, randstate);
  self->t_sample += ggh_walltime(t);
}

void _gghlite_set_x(gghlite_t self) {
  fmpz_mod_poly_t tmp;
  fmpz_mod_poly_init(tmp, self->pk->q);

  fmpz_mod_poly_t acc;
  fmpz_mod_poly_init(acc, self->pk->q);

  for(size_t k=0; k<self->pk->kappa; k++) {
    if(fmpz_poly_is_zero(self->b[k][0]) || fmpz_poly_is_zero(self->b[k][1]))
      continue;

    fmpz_mod_poly_oz_ntt_pow_ui(acc, self->z_inv, k+1, self->pk->n);

    fmpz_mod_poly_init(self->pk->x[k][0], self->pk->q);
    fmpz_mod_poly_oz_ntt_enc_fmpz_poly(tmp, self->b[k][0], self->pk->ntt);
    fmpz_mod_poly_oz_ntt_mul(self->pk->x[k][0], tmp, acc, self->pk->n);

    fmpz_mod_poly_init(self->pk->x[k][1], self->pk->q);
    fmpz_mod_poly_oz_ntt_enc_fmpz_poly(tmp, self->b[k][1], self->pk->ntt);
    fmpz_mod_poly_oz_ntt_mul(self->pk->x[k][1], tmp, acc, self->pk->n);
  }
  fmpz_mod_poly_clear(tmp);
  fmpz_mod_poly_clear(acc);
}

void _gghlite_set_D_g(gghlite_t self) {
  assert(self);
  assert(self->pk);
  assert(mpfr_cmp_d(self->pk->sigma_p,0)>0);
  self->D_g = _gghlite_dgsl_from_poly(self->g, self->pk->sigma_p, NULL, DGSL_INLATTICE);
}

void _gghlite_sample_b(gghlite_t self, flint_rand_t randstate) {
  assert(self->pk);
  assert(self->pk->n);
  assert(self->D_g);
  assert(!fmpz_poly_is_zero(self->g));

  if ((self->pk->rerand_mask != 1) && (self->pk->rerand_mask != 0))
    ggh_die("Re-randomisation at higher levels is not implemented yet.");

  const long n = self->pk->n;

  mpfr_t sigma_n;
  mpfr_init2(sigma_n, _gghlite_prec(self->pk));

  mpfr_t norm;
  mpfr_init2(norm, _gghlite_prec(self->pk));

  mpfr_t sqrtn_sigma_p;
  mpfr_init2(sqrtn_sigma_p, _gghlite_prec(self->pk));
  mpfr_set_si(sqrtn_sigma_p, n, MPFR_RNDN);
  mpfr_sqrt(sqrtn_sigma_p, sqrtn_sigma_p, MPFR_RNDN);
  mpfr_mul(sqrtn_sigma_p, sqrtn_sigma_p, self->pk->sigma_p, MPFR_RNDN);

  fmpz_poly_t B;
  fmpz_poly_init2(B, 2*n);

  long fail[3] = {0,0,0};

#ifndef GGHLITE_CHECK_SIGMA_N
#ifndef GGHLITE_QUIET
  fprintf(stderr, "#### WARNING: Not checking that `σ_n(rot(B^(k))) < ℓ_b`. ####\n");
#endif //GGHLITE_QUITE
#endif //GGHLITE_CHECK_SIGMA_N

  const int nsp = _gghlite_nsmall_primes(self->pk);
  mp_limb_t *primes = _fmpz_poly_oz_ideal_probable_prime_factors(self->pk->n, nsp);

  for(size_t k=0; k<self->pk->kappa; k++) {
    fmpz_poly_init(self->b[k][0]);
    fmpz_poly_init(self->b[k][1]);

    if (!(self->pk->rerand_mask & (1ULL)<<k))
      continue;

    while(1) {
      printf("\r Computing B^(%2ld):: !i: %4ld, !s: %4ld, !n: %4ld",k+1, fail[0], fail[1], fail[2]);
      fflush(0);
      uint64_t t = ggh_walltime(0);
      _dgsl_rot_mp_call_inlattice_multiplier(self->b[k][0], self->D_g, randstate->gmp_state);
      _dgsl_rot_mp_call_inlattice_multiplier(self->b[k][1], self->D_g, randstate->gmp_state);
      self->t_sample += ggh_walltime(t);

      t = ggh_walltime(0);
      const int coprime = fmpz_poly_oz_coprime(self->b[k][0], self->b[k][1],
                                               self->pk->n, 0, primes);
      self->t_is_subideal += ggh_walltime(t);

      if (!coprime) {
        fail[0]++;
        continue;
      }

      fmpz_poly_oz_mul(self->b[k][0], self->D_g->B, self->b[k][0], self->pk->n);
      fmpz_poly_oz_mul(self->b[k][1], self->D_g->B, self->b[k][1], self->pk->n);

      _fmpz_vec_set(B->coeffs+0, self->b[k][0]->coeffs, n);
      _fmpz_vec_set(B->coeffs+n, self->b[k][1]->coeffs, n);


#ifdef GGHLITE_CHECK_SIGMA_N
      fmpz_poly_rot_basis_sigma_n(sigma_n, B);
      if (mpfr_cmp(sigma_n, self->pk->ell_b) < 0) {
        fail[1]++;
        continue;
      }
#endif

      fmpz_poly_eucl_norm_mpfr(norm, B, MPFR_RNDN);
      if (mpfr_cmp(norm, sqrtn_sigma_p) > 0) {
        fail[2]++;
        continue;
      }

      break;
    }
    printf("\n");
  }
  free(primes);
  mpfr_clear(sqrtn_sigma_p);
  mpfr_clear(sigma_n);
  mpfr_clear(norm);
  fmpz_poly_clear(B);
}

void _gghlite_sample_g(gghlite_t self, flint_rand_t randstate) {
  assert(self->pk);
  assert(self->pk->n);

  fmpz_poly_init(self->g);

  mpfr_t g_inv_norm;
  mpfr_init2(g_inv_norm, fmpz_sizeinbase(self->pk->q,2));

  mpfr_t norm;
  mpfr_init2(norm, mpfr_get_prec(self->pk->sigma));

  mpfr_t sqrtn_sigma;
  mpfr_init2(sqrtn_sigma, mpfr_get_prec(self->pk->sigma));
  mpfr_set_si(sqrtn_sigma, self->pk->n, MPFR_RNDN);
  mpfr_sqrt(sqrtn_sigma, sqrtn_sigma, MPFR_RNDN);
  mpfr_mul(sqrtn_sigma, sqrtn_sigma, self->pk->sigma, MPFR_RNDN);

  dgsl_rot_mp_t *D = _gghlite_dgsl_from_n(self->pk->n, self->pk->sigma);

  long fail[3] = {0,0,0};

  fmpq_poly_t g_q;
  fmpq_poly_init(g_q);

  fmpq_poly_t g_inv;
  fmpq_poly_init(g_inv);

  const int check_prime = self->pk->flags & GGHLITE_FLAGS_PRIME_G;

  int prime_pass = 0;
  mp_limb_t *primes;
  if (check_prime) {
    const int nsp = _gghlite_nsmall_primes(self->pk);
    primes = _fmpz_poly_oz_ideal_probable_prime_factors(self->pk->n, nsp);
  } else {
    /* we only need 2κ but κ^2 doesn't cost much, so we rule out prime factors up to κ^2 */
    primes = _fmpz_poly_oz_ideal_small_prime_factors(self->pk->n, self->pk->kappa*self->pk->kappa);
  }

  while(1) {
    printf("\r      Computing g:: !n: %4ld, !p: %4ld, !i: %4ld",fail[0], fail[1], fail[2]);
    fflush(0);

    uint64_t t = ggh_walltime(0);
    fmpz_poly_sample_D(self->g, D, randstate);
    self->t_sample += ggh_walltime(t);

    fmpz_poly_eucl_norm_mpfr(norm, self->g, MPFR_RNDN);
    if(mpfr_cmp(norm, sqrtn_sigma)>0) {
      fail[0]++;
      continue;
    }

    /* 1. check if prime */
    t = ggh_walltime(0);
    if (check_prime)
      prime_pass = fmpz_poly_oz_ideal_is_probaprime(self->g, self->pk->n, 0, primes);
    else
      prime_pass = fmpz_poly_oz_ideal_not_prime_factors(self->g, self->pk->n, primes);
    self->t_is_prime += ggh_walltime(t);
    if (!prime_pass) {
      fail[1]++;
      continue;
    }

    /* 2. check norm of inverse */
    fmpq_poly_set_fmpz_poly(g_q, self->g);
    _fmpq_poly_oz_invert_approx(g_inv, g_q, self->pk->n, 2*self->pk->lambda);
    if (!_gghlite_g_inv_check(self->pk, g_inv)) {
      fail[2]++;
      continue;
    }
    break;
  }

  free(primes);

  printf("\n");
  fflush(0);
  mpfr_clear(norm);
  mpfr_clear(sqrtn_sigma);
  mpfr_clear(g_inv_norm);
  fmpq_poly_clear(g_q);
  fmpq_poly_clear(g_inv);
  dgsl_rot_mp_clear(D);
}

void _gghlite_set_pzt(gghlite_t self) {
  assert(self->pk);
  assert(self->pk->n);
  assert(fmpz_cmp_ui(self->pk->q,0)>0);
  assert(!fmpz_mod_poly_is_zero(self->z));
  assert(!fmpz_poly_is_zero(self->h));

  fmpz_mod_poly_t z_kappa;  fmpz_mod_poly_init(z_kappa, self->pk->q);
  fmpz_mod_poly_set(z_kappa, self->z);
  fmpz_mod_poly_oz_ntt_pow_ui(z_kappa, z_kappa, self->pk->kappa, self->pk->n);

  fmpz_mod_poly_t g_inv;  fmpz_mod_poly_init(g_inv, self->pk->q);
  fmpz_mod_poly_oz_ntt_enc_fmpz_poly(g_inv, self->g, self->pk->ntt);
  fmpz_mod_poly_oz_ntt_inv(g_inv, g_inv, self->pk->n);

  fmpz_mod_poly_t pzt;  fmpz_mod_poly_init(pzt, self->pk->q);
  fmpz_mod_poly_oz_ntt_mul(pzt, z_kappa, g_inv, self->pk->n);

  fmpz_mod_poly_t h;  fmpz_mod_poly_init(h, self->pk->q);
  fmpz_mod_poly_oz_ntt_enc_fmpz_poly(h, self->h, self->pk->ntt);

  fmpz_mod_poly_oz_ntt_mul(pzt, pzt, h, self->pk->n);

  fmpz_mod_poly_init(self->pk->pzt, self->pk->q);
  fmpz_mod_poly_set(self->pk->pzt, pzt);

  fmpz_mod_poly_clear(h);
  fmpz_mod_poly_clear(pzt);
  fmpz_mod_poly_clear(z_kappa);
  fmpz_mod_poly_clear(g_inv);
}

void _gghlite_sample_h(gghlite_t self, flint_rand_t randstate) {
  assert(self->pk);
  assert(self->pk->n);
  assert(fmpz_cmp_ui(self->pk->q,0)>0);

  mpfr_t sqrt_q;
  mpfr_init2(sqrt_q, fmpz_sizeinbase(self->pk->q, 2));
  _gghlite_get_q_mpfr(sqrt_q, self->pk, MPFR_RNDN);
  mpfr_sqrt(sqrt_q, sqrt_q, MPFR_RNDN);

  /* dgsl samples proportionally to `\exp(-(x-c)²/(2σ²))` but GGHLite is
     specifiied with respect to `\exp(-π(x-c)²/σ²)`. So we divide by \sqrt{2π} */
  mpfr_mul_d(sqrt_q, sqrt_q, S_TO_SIGMA, MPFR_RNDN);

  fmpz_poly_init(self->h);

  mp_limb_t *primes = _fmpz_poly_oz_ideal_small_prime_factors(self->pk->n, 2*self->pk->lambda);

  uint64_t t = ggh_walltime(0);

  /** We don't need to check if g and h share no small common factors because <g> has no small
      factors and because the probability of a false positive is exponentially small in any case. On
      the other hand, it doesn't cost much so we might as well do it.
   */

  int no_small_common = 0;
  while(!no_small_common) {
    fmpz_poly_sample_sigma(self->h, self->pk->n, sqrt_q, randstate);
    no_small_common = fmpz_poly_oz_coprime(self->h, self->g, self->pk->n, 1, primes);
   }
  self->t_sample +=  ggh_walltime(t);

  free(primes);
  mpfr_clear(sqrt_q);
}

void _gghlite_sample_z(gghlite_t self, flint_rand_t randstate) {
  assert(self->pk);
  assert(self->pk->n);
  assert(fmpz_cmp_ui(self->pk->q,0)>0);

  fmpz_mod_poly_init(self->z, self->pk->q);

  uint64_t t = ggh_walltime(0);
  fmpz_mod_poly_randtest(self->z, randstate, self->pk->n);
  fmpz_mod_poly_oz_ntt_enc(self->z, self->z, self->pk->ntt);

  fmpz_mod_poly_init(self->z_inv, self->pk->q);
  fmpz_mod_poly_oz_ntt_inv(self->z_inv, self->z, self->pk->n);
  self->t_sample +=  ggh_walltime(t);
}

void gghlite_init_instance(gghlite_t self, flint_rand_t randstate) {
  assert(self->pk->lambda);
  assert(self->pk->kappa);

  fmpz_mod_poly_oz_ntt_precomp_init(self->pk->ntt, self->pk->n, self->pk->q);

  _gghlite_sample_g(self, randstate);
  _gghlite_sample_z(self, randstate);
  _gghlite_sample_h(self, randstate);

  _gghlite_set_D_g(self);

  _gghlite_sample_b(self, randstate);
  _gghlite_sample_a(self, randstate);

  _gghlite_set_y(self);
  _gghlite_set_x(self);
  _gghlite_set_pzt(self);
  _gghlite_pk_set_D_sigma_p(self->pk);
  _gghlite_pk_set_D_sigma_s(self->pk);
}

void gghlite_init(gghlite_t self, const size_t lambda, const size_t kappa,
                  const uint64_t rerand_mask, const uint64_t flags, flint_rand_t randstate) {
  _gghlite_zero(self);
  gghlite_pk_init_params(self->pk, lambda, kappa, rerand_mask, flags);
  gghlite_init_instance(self, randstate);
}

void gghlite_clear(gghlite_t self, int clear_pk) {
  fmpz_poly_clear(self->a);
  for(unsigned long k=0; k<self->pk->kappa; k++) {
    if(self->pk->rerand_mask && (1ULL)<<k) {
      fmpz_poly_clear(self->b[k][0]);
      fmpz_poly_clear(self->b[k][1]);
    }
  }
  fmpz_poly_clear(self->h);
  fmpz_mod_poly_clear(self->z);
  fmpz_mod_poly_clear(self->z_inv);
  fmpz_poly_clear(self->g);
  dgsl_rot_mp_clear(self->D_g);

  if (clear_pk)
    gghlite_pk_clear(self->pk);
}


void gghlite_print_norms(const gghlite_t self) {
  assert(self->pk->n);
  assert(!fmpz_poly_is_zero(self->g));

  mpfr_t norm;
  mpfr_init2(norm, _gghlite_prec(self->pk));

  mpfr_t bound;
  mpfr_init2(bound, _gghlite_prec(self->pk));

  mpfr_t sqrt_n;
  mpfr_init2(sqrt_n, _gghlite_prec(self->pk));
  mpfr_set_ui(sqrt_n, self->pk->n, MPFR_RNDN);
  mpfr_sqrt(sqrt_n, sqrt_n, MPFR_RNDN);
  mpfr_log2(sqrt_n, sqrt_n, MPFR_RNDN);

  fmpz_poly_eucl_norm_mpfr(norm, self->g, MPFR_RNDN);
  mpfr_log2(norm, norm, MPFR_RNDN);

  mpfr_set(bound, self->pk->sigma, MPFR_RNDN);
  mpfr_log2(bound, bound, MPFR_RNDN);
  mpfr_add(bound, bound, sqrt_n, MPFR_RNDN);

  printf("  log(|g|): %6.1f ?< %6.1f\n", mpfr_get_d(norm, MPFR_RNDN), mpfr_get_d(bound, MPFR_RNDN));

  for(size_t k=0; k<self->pk->kappa; k++) {
    if ((1ULL<<k) & self->pk->rerand_mask) {
      mpfr_set(bound, self->pk->sigma_p, MPFR_RNDN);
      mpfr_log2(bound, bound, MPFR_RNDN);
      mpfr_add(bound, bound, sqrt_n, MPFR_RNDN);

      for(int i=0; i<2; i++) {
        fmpz_poly_eucl_norm_mpfr(norm, self->b[k][i], MPFR_RNDN);
        mpfr_log2(norm, norm, MPFR_RNDN);
        printf("log(|b_%d|): %6.1f ?< %6.1f\n", i, mpfr_get_d(norm, MPFR_RNDN), mpfr_get_d(bound, MPFR_RNDN));
      }
    }
  }

  mpfr_set(bound, self->pk->sigma_p, MPFR_RNDN);
  mpfr_log2(bound, bound, MPFR_RNDN);
  mpfr_add(bound, bound, sqrt_n, MPFR_RNDN);

  fmpz_poly_eucl_norm_mpfr(norm, self->a, MPFR_RNDN);
  mpfr_log2(norm, norm, MPFR_RNDN);
  printf("  log(|a|): %6.1f ?< %6.1f\n", mpfr_get_d(norm, MPFR_RNDN), mpfr_get_d(bound, MPFR_RNDN));

  _gghlite_get_q_mpfr(bound, self->pk, MPFR_RNDN);
  mpfr_sqrt(bound, bound, MPFR_RNDN);
  mpfr_log2(bound, bound, MPFR_RNDN);
  mpfr_add(bound, bound, sqrt_n, MPFR_RNDN);

  fmpz_poly_eucl_norm_mpfr(norm, self->h, MPFR_RNDN);
  mpfr_log2(norm, norm, MPFR_RNDN);
  printf("  log(|h|): %6.1f ?< %6.1f\n", mpfr_get_d(norm, MPFR_RNDN), mpfr_get_d(bound, MPFR_RNDN));
}

void gghlite_print_times(const gghlite_t self) {
  printf("        sampling: %7.1fs\n", self->t_sample/1000000.0);
  printf("  primality test: %7.1fs\n", self->t_is_prime/1000000.0);
  printf("<b_0,b_1> == <g>: %7.1fs\n", self->t_is_subideal/1000000.0);
}

dgsl_rot_mp_t *_gghlite_dgsl_from_poly(fmpz_poly_t g, mpfr_t sigma, fmpq_poly_t c, dgsl_alg_t algorithm) {
  /* GPV samples proportionally to `\exp(-(x-c)²/(2σ²))` but GGHLite is
     specifiied with respect to `\exp(-π(x-c)²/σ²)`. So we divide by \sqrt{2π}
  */

  mpfr_t sigma_;
  mpfr_init2(sigma_, mpfr_get_prec(sigma));
  mpfr_set(sigma_, sigma, MPFR_RNDN);
  mpfr_mul_d(sigma_, sigma_, S_TO_SIGMA, MPFR_RNDN);

  dgsl_rot_mp_t *D = dgsl_rot_mp_init(fmpz_poly_length(g), g, sigma_, c, algorithm, OZ_VERBOSE);
  mpfr_clear(sigma_);
  return D;
}

dgsl_rot_mp_t *_gghlite_dgsl_from_n(const long n, mpfr_t sigma) {
  fmpz_poly_t I;
  fmpz_poly_init(I);
  fmpz_poly_one(I);

  /* GPV samples proportionally to `\exp(-(x-c)²/(2σ²))` but GGHLite is
     specifiied with respect to `\exp(-π(x-c)²/σ²)`. So we divide by \sqrt{2π}
  */

  mpfr_t sigma_;
  mpfr_init2(sigma_, mpfr_get_prec(sigma));
  mpfr_set(sigma_, sigma, MPFR_RNDN);
  mpfr_mul_d(sigma_, sigma_, S_TO_SIGMA, MPFR_RNDN);

  dgsl_rot_mp_t *D = dgsl_rot_mp_init(n, I, sigma_, NULL, DGSL_IDENTITY, OZ_VERBOSE);
  fmpz_poly_clear(I);
  mpfr_clear(sigma_);
  return D;
}

/**
   @file gghlite-defs.h
*/

#ifndef _DEFS_H_
#define _DEFS_H_

#include <gghlite/config.h>
#include <gghlite/misc.h>
#include <flint/fmpz_poly.h>
#include <flint/fmpz_mod_poly.h>
#include <dgsl/dgsl.h>


/**
   Plaintext elements are represented as polynomials in the usual coefficient
   representation.
*/

typedef fmpz_poly_t     gghlite_clr_t;

/**
   Encodings are represented as evaluations of polynomials, i.e. as vectors `e`
   of length `n` where `e_i` holds our element evaluated at `ω_n^i` where `ω_n`
   as some root of unity in Z_q.
**/

typedef fmpz_mod_poly_t gghlite_enc_t;


/**
   \brief Flags controlling global behaviour
*/

typedef enum {
  GGHLITE_FLAGS_DEFAULT   = 0x0, //!< default behaviour
  GGHLITE_FLAGS_PRIME_G   = 0x1, //!< enforce that @f$\ideal{g}@f$ is a prime ideal (expensive!)
  GGHLITE_FLAGS_VERBOSE   = 0x2, //!< be more verbose
  GGHLITE_FLAGS_GDDH_HARD = 0x4, //!< pick σ_1^* so that GDDH is hard
  GGHLITE_FLAGS_SLOPPY    = 0x8, //!< skip some expensive checks
} gghlite_flags_t;

/**
   Maximum supported multi-linearity level.
*/

#define KAPPA (sizeof(uint64_t)<<3)

/**
   @brief GGHLite Public key Struct

   This struct represents a GGHLite public key or `params`. We emphasise computation speed over
   memory size and hence include various caches to speed up computations.
*/

struct _gghlite_pk_struct {
  size_t lambda;                      //!< security parameter $λ$
  size_t kappa;                       //!< multi-linearity parameter $κ ≤$ `KAPPA`
  uint64_t rerand_mask;               //!< mask where $i$-th bit toggles generation of re-randomisers for level-$i$
  uint64_t flags;                     //!< see @ref gghlite_flags_t
  long n;                             //!< dimension of the lattice $n$
  long ell;                           //!< number of bits $ℓ$ in each coefficient used for extraction
  fmpz_t q;                           //!< modulus $q$
  mpfr_t sigma;                       //!< Gaussian width parameter $σ$ for sampling $g$
  mpfr_t sigma_p;                     //!< Gaussian width parameter $σ'$ for sampling $b_{k,i}$
  mpfr_t sigma_s;                     //!< Gaussian width parameter $σ^*$ for sampling $ρ_i$
  mpfr_t ell_b;                       //!< bound $ℓ_b$ on $σ_n(rot(B^(k)))$
  mpfr_t ell_g;                       //!< bound $ℓ_g$ on $|g^-1|$
  mpfr_t xi;                          //!< fraction $ξ$ of $q$ used for zero-testing
  gghlite_enc_t pzt;                  //!< zero-testing parameter $p_{zt}$
  gghlite_enc_t x[KAPPA][2];          //!< level-$k$ encodings of zero $x_{k,i}$ for each level $k$ specified by rerandomisation mask
  gghlite_enc_t y;                    //!< one level-1 encoding of 1
  dgsl_rot_mp_t *D_sigma_p;           //!< discrete Gaussian distribution $D_{\ZZ,σ'}$
  dgsl_rot_mp_t *D_sigma_s;           //!< discrete Gaussian distribution $D_{\ZZ,σ^*}$
  fmpz_mod_poly_oz_ntt_precomp_t ntt; //!< pre-computation data for computing in the NTT domain
};

/**
   @brief GGHLite Public key

   @see _gghlite_pk_struct
*/

typedef struct _gghlite_pk_struct gghlite_pk_t[1];

/**
   @brief GGHLite Secret Key.
*/

struct _gghlite_struct {
  gghlite_pk_t pk;             //!< the GGHLite public key
  gghlite_clr_t g;             //!< an ideal generator $\ideal{g}$
  gghlite_enc_t z;             //!< $g^{-1}$ in $\\ZZ_q[x]/(x^n+1)$
  gghlite_enc_t z_inv;         //!< masking element $z$
  gghlite_clr_t h;             //!< masking element $h$
  gghlite_clr_t a;             //!< an element $a \\bmod \\ideal{g} = 1$
  gghlite_clr_t b[KAPPA][2];   //!< an element $b \\bmod \\ideal{g} = 0$
  dgsl_rot_mp_t *D_g;          //!< discrete Gaussian distribution $D_{R,σ'}$ with $R = \\ZZ[x]/(x^n+1)$
  uint64_t t_is_prime;         //!< time spent on checking if g is prime in μs
  uint64_t t_is_subideal;      //!< time spent on verifying that $\\ideal{b_0,b_1} = \\ideal{g}$ in μs
  uint64_t t_sample;           //!< time spent on sampling  in μs
};

typedef struct _gghlite_struct gghlite_t[1];

#endif /* _DEFS_H_ */

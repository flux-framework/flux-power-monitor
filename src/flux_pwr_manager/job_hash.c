#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "job_hash.h"
#include "flux_pwr_logging.h"
/* Hash numerical jobid in 'key'.
 * N.B. zhashx_hash_fn signature
 */
static size_t job_hasher(const void *key) {
  const uint64_t *id = key;
  return *id;
}

#define NUMCMP(a, b) ((a) == (b) ? 0 : ((a) < (b) ? -1 : 1))

/* Compare hash keys.
 * N.B. zhashx_comparator_fn signature
 */
static int job_hash_key_cmp(const void *key1, const void *key2) {
  const uint64_t *id1 = key1;
  const uint64_t *id2 = key2;
  return NUMCMP(*id1, *id2);
}

zhashx_t *job_hash_create(void) {
  zhashx_t *hash;

  if (!(hash = zhashx_new())) {
    errno = ENOMEM;
    return NULL;
  }
  zhashx_set_key_hasher(hash, job_hasher);
  zhashx_set_key_comparator(hash, job_hash_key_cmp);
  zhashx_set_key_duplicator(hash, NULL);
  zhashx_set_key_destructor(hash, NULL);

  return hash;
}

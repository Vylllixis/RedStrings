/* Code tiers : bc7enc (richgel999/bc7enc, MIT ou domaine public), compilé sans -Werror (voir
 * engine/CMakeLists.txt). Seul fichier qui en voit les types. */
/* Inclusion volontaire du .c tiers : ses fonctions internes ne sont visibles que de ce fichier. */
#include "bc7enc.c" // NOLINT(bugprone-suspicious-include)

#include "engine/bc7.h"

void eng_bc7_init(void)
{
    static bool done = false;
    if (!done) {
        bc7enc_compress_block_init();
        done = true;
    }
}

void eng_bc7_encode_block(uint8_t out[16], const uint8_t rgba[64], bool perceptual, uint32_t effort)
{
    bc7enc_compress_block_params params;
    /* Les fonctions d'initialisation de l'en-tête sont `inline` sans `static` : elles n'ont pas de
     * définition externe en C11. Les champs sont donc renseignés ici (mêmes valeurs). */
    params.m_max_partitions_mode = BC7ENC_MAX_PARTITIONS1;
    params.m_try_least_squares = BC7ENC_TRUE;
    params.m_mode_partition_estimation_filterbank = BC7ENC_TRUE;
    params.m_uber_level = effort > BC7ENC_MAX_UBER_LEVEL ? BC7ENC_MAX_UBER_LEVEL : effort;
    params.m_use_mode5_for_alpha = BC7ENC_TRUE;
    params.m_use_mode7_for_alpha = BC7ENC_TRUE;
    params.m_perceptual = perceptual ? BC7ENC_TRUE : BC7ENC_FALSE;
    if (perceptual) {
        params.m_weights[0] = 128;
        params.m_weights[1] = 64;
        params.m_weights[2] = 16;
        params.m_weights[3] = 32;
    } else {
        params.m_weights[0] = 1;
        params.m_weights[1] = 1;
        params.m_weights[2] = 1;
        params.m_weights[3] = 1;
    }
    (void)bc7enc_compress_block(out, rgba, &params);
}

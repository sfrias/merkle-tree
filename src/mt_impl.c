/*
 * mt-data.c
 *
 *  Created on: 02.05.2014
 *      Author: dhein
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "merkletree.h"
#include "mt_crypto.h"

//----------------------------------------------------------------------
mt_t *mt_create(void) {
  mt_t *mt = calloc(1, sizeof(mt_t));
  if (!mt) {
    return NULL;
  }
  for (int32_t i = 0; i < TREE_LEVELS; ++i) {
    mt_al_t *tmp = mt_al_create();
    if (!tmp) {
      for (uint32_t j = 0; j < i; ++j) {
        free(mt->level[j]);
      }
      free(mt);
      return NULL;
    }
    mt->level[i] = tmp;
  }
  return mt;
}

//----------------------------------------------------------------------
void mt_delete(mt_t *mt) {
  for (int32_t i = 0; i < TREE_LEVELS; ++i) {
    free(mt->level[i]);
  }
  free(mt);
}

//----------------------------------------------------------------------
mt_error_t mt_add(mt_t *mt, const uint8_t hash[HASH_LENGTH]) {
  if (!(mt && hash)) {
    return MT_ERR_ILLEGAL_PARAM;
  }
  mt_error_t err = MT_ERR_UNSPECIFIED;
  err = mt_al_add(mt->level[0], hash);
  if (err != MT_SUCCESS) {
    return err;
  }
  mt->elems += 1;
  if (mt->elems == 1) {
    return MT_SUCCESS;
  }
  uint8_t message_digest[HASH_LENGTH];
  memcpy(message_digest, hash, HASH_LENGTH);
  uint32_t q = mt->elems - 1;
  uint32_t l = 0;         // level
  while (q > 0) {
    if ((q & 1) != 0) {
      uint8_t const * const left = mt_al_get(mt->level[l], q - 1);
      err = mt_hash(left, message_digest, message_digest);
      if (err != MT_SUCCESS) {
        return err;
      }
      err = mt_al_add_or_update(mt->level[l + 1], message_digest, (q >> 1));
      if (err != MT_SUCCESS) {
        return err;
      }
    }
    q >>= 1;
    l += 1;
  }
  return MT_SUCCESS;
}

//----------------------------------------------------------------------
static uint32_t hasNextLevelExceptRoot(mt_t const * const mt, uint32_t cur_lvl) {
  if (!mt) {
    return 0;
  }
  // TODO fix number of levels, we need to count root!
  return (cur_lvl + 1 < TREE_LEVELS - 1)
      & (getSize(mt->level[(cur_lvl + 1)]) > 0);
}

//----------------------------------------------------------------------
static const uint8_t *findRightNeighbor(const mt_t *mt, uint32_t offset,
    int32_t l) {
  if (!mt) {
    return NULL;
  }
  do {
    if (offset < getSize(mt->level[l])) {
      return mt_al_get(mt->level[l], offset);
    }
    l -= 1;
    offset <<= 1;
  } while (l > -1);
  // This can happen, if there is no neighbor.
  return NULL;
}

//----------------------------------------------------------------------
mt_error_t mt_verify(const mt_t *mt, const uint8_t hash[HASH_LENGTH],
    const uint32_t offset) {
  if (!(mt && hash && (offset < mt->elems))) {
    return MT_ERR_ILLEGAL_PARAM;
  }
  mt_error_t err = MT_ERR_UNSPECIFIED;
  uint8_t message_digest[HASH_LENGTH];
  memcpy(message_digest, hash, HASH_LENGTH);
  uint32_t q = offset;
  uint32_t l = 0;         // level
  while (hasNextLevelExceptRoot(mt, l)) {
    if (!(q & 0x01)) { // left subtree
      // If I am the left neighbor (even index), we need to check if a right
      // neighbor exists.
      const uint8_t *right;
      if ((right = findRightNeighbor(mt, q + 1, l)) != NULL) {
        err = mt_hash(message_digest, right, message_digest);
        if (err != MT_SUCCESS) {
          return err;
        }
      }
    } else {           // right subtree
      // In the right subtree, there must always be a left neighbor!
      uint8_t const * const left = mt_al_get(mt->level[l], q - 1);
      err = mt_hash(left, message_digest, message_digest);
      if (err != MT_SUCCESS) {
        return err;
      }
    }
    q >>= 1;
    l += 1;
  }
  // TODO @this point we need to compare the root hash
  mt_print_hash(message_digest);
  return MT_SUCCESS;
}

mt_error_t mt_update(const mt_t *mt, const uint8_t hash[HASH_LENGTH],
    const uint32_t offset) {
  if (!(mt && hash && (offset < mt->elems))) {
    return MT_ERR_ILLEGAL_PARAM;
  }
  mt_error_t err = MT_ERR_UNSPECIFIED;
  err = mt_al_update(mt->level[0], hash, offset);
  if (err != MT_SUCCESS) {
    return err;
  }
  uint8_t message_digest[HASH_LENGTH];
  memcpy(message_digest, hash, HASH_LENGTH);
  uint32_t q = offset;
  uint32_t l = 0;         // level
  while (hasNextLevelExceptRoot(mt, l)) {
    if (!(q & 0x01)) { // left subtree
      // If I am the left neighbor (even index), we need to check if a right
      // neighbor exists.
      const uint8_t *right;
      if ((right = findRightNeighbor(mt, q + 1, l)) != NULL) {
        err = mt_hash(message_digest, right, message_digest);
        if (err != MT_SUCCESS) {
          return err;
        }
      }
    } else {           // right subtree
      // In the right subtree, there must always be a left neighbor!
      uint8_t const * const left = mt_al_get(mt->level[l], q - 1);
      err = mt_hash(left, message_digest, message_digest);
      if (err != MT_SUCCESS) {
        return err;
      }
    }
    q >>= 1;
    l += 1;
    err = mt_al_update(mt->level[l], message_digest, q);
    if (err != MT_SUCCESS) {
      return err;
    }
  }
  return MT_SUCCESS;
}

//----------------------------------------------------------------------
void mt_print(const mt_t *mt) {
  if (!mt) {
    printf("[ERROR][mt_print]: Merkle Tree NULL");
    return;
  }
  for (int32_t i = 0; i < TREE_LEVELS; ++i) {
    if (mt->level[i]->elems == 0) {
      return;
    }
    printf(
        "==================== Merkle Tree level[%02d]: ====================\n",
        i);
    mt_al_print(mt->level[i]);
  }
}

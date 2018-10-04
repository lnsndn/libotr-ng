/*
 *  This file is part of the Off-the-Record Next Generation Messaging
 *  library (libotr-ng).
 *
 *  Copyright (C) 2016-2018, the libotr-ng contributors.
 *
 *  This library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "prekey_message.h"

#include "alloc.h"

#include "deserialize.h"
#include "serialize.h"

INTERNAL prekey_message_s *otrng_prekey_message_new(void) {
  prekey_message_s *prekey_msg = otrng_xmalloc_z(sizeof(prekey_message_s));

  return prekey_msg;
}

INTERNAL prekey_message_s *otrng_prekey_message_build(uint32_t instance_tag,
                                                      const ec_point ecdh,
                                                      const dh_public_key dh) {
  prekey_message_s *msg = otrng_prekey_message_new();
  uint32_t *identifier;
  if (!msg) {
    return NULL;
  }

  msg->sender_instance_tag = instance_tag;

  otrng_ec_point_copy(msg->Y, ecdh);
  msg->B = otrng_dh_mpi_copy(dh);

  identifier = gcry_random_bytes(4, GCRY_STRONG_RANDOM);
  msg->id = *identifier;

  gcry_free(identifier);

  return msg;
}

INTERNAL void otrng_prekey_message_destroy(prekey_message_s *prekey_msg) {
  prekey_msg->id = 0;
  otrng_ec_point_destroy(prekey_msg->Y);
  otrng_dh_mpi_release(prekey_msg->B);
  prekey_msg->B = NULL;
}

INTERNAL void otrng_prekey_message_free(prekey_message_s *prekey_msg) {
  if (!prekey_msg) {
    return;
  }

  otrng_prekey_message_destroy(prekey_msg);
  free(prekey_msg);
}

INTERNAL otrng_result otrng_prekey_message_serialize_into(
    uint8_t **dst, size_t *nbytes, const prekey_message_s *prekey_msg) {

  size_t size = PRE_KEY_MAX_BYTES;
  *dst = otrng_xmalloc_z(size);

  return otrng_prekey_message_serialize(*dst, size, nbytes, prekey_msg);
}

INTERNAL otrng_result
otrng_prekey_message_serialize(uint8_t *dst, size_t dst_len, size_t *written,
                               const prekey_message_s *src) {
  size_t w = 0, len;
  w += otrng_serialize_uint16(dst + w, OTRNG_PROTOCOL_VERSION_4);
  w += otrng_serialize_uint8(dst + w, PRE_KEY_MSG_TYPE);
  w += otrng_serialize_uint32(dst + w, src->id);
  w += otrng_serialize_uint32(dst + w, src->sender_instance_tag);
  w += otrng_serialize_ec_point(dst + w, src->Y);

  if (!otrng_serialize_dh_public_key(dst + w, dst_len - w, &len, src->B)) {
    return OTRNG_ERROR;
  }

  w += len;

  if (written) {
    *written = w;
  }

  return OTRNG_SUCCESS;
}

INTERNAL otrng_result otrng_prekey_message_deserialize(prekey_message_s *dst,
                                                       const uint8_t *src,
                                                       size_t src_len,
                                                       size_t *nread) {
  const uint8_t *cursor = src;
  int64_t len = src_len;
  size_t read = 0;
  uint16_t protocol_version = 0;
  uint8_t msg_type = 0;
  otrng_result ret;

  if (!otrng_deserialize_uint16(&protocol_version, cursor, len, &read)) {
    return OTRNG_ERROR;
  }

  cursor += read;
  len -= read;

  if (protocol_version != OTRNG_PROTOCOL_VERSION_4) {
    return OTRNG_ERROR;
  }

  if (!otrng_deserialize_uint8(&msg_type, cursor, len, &read)) {
    return OTRNG_ERROR;
  }

  cursor += read;
  len -= read;

  if (msg_type != PRE_KEY_MSG_TYPE) {
    return OTRNG_ERROR;
  }

  if (!otrng_deserialize_uint32(&dst->id, cursor, len, &read)) {
    return OTRNG_ERROR;
  }

  cursor += read;
  len -= read;

  if (!otrng_deserialize_uint32(&dst->sender_instance_tag, cursor, len,
                                &read)) {
    return OTRNG_ERROR;
  }

  cursor += read;
  len -= read;

  if (!otrng_deserialize_ec_point(dst->Y, cursor, len)) {
    return OTRNG_ERROR;
  }

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  ret = otrng_deserialize_dh_mpi_otr(&dst->B, cursor, len, &read);

  if (nread) {
    *nread = cursor - src + read;
  }

  return ret;
}
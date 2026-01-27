#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>

#define AUTH_SHA256_BUFFER_SIZE      32u
#define AUTH_MAX_HMAC_BUFFER_SIZE    (AUTH_SHA256_BUFFER_SIZE * 4u)

/***********************************************************************
 * @brief function to generate the HMAC from the given payload and 
 * timestamp strings
 * @param payload - payload to use to generate HMAC
 * @param payload_len - length of the payload string
 * @param timestemp - current device boot up time in milliseconds
 * @return generated HMAC string
 */
char * auth_generate_hmac(const char * payload, size_t payload_len, int64_t timestamp);

#endif
#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>

#define AUTH_SHA256_BUFFER_SIZE             (32u)
#define AUTH_MAX_CANONICAL_STRING_LENGTH    (AUTH_SHA256_BUFFER_SIZE * 4u)
#define AUTH_MAX_HASH_REQ_BODY_LENGTH       (AUTH_SHA256_BUFFER_SIZE * 4u)
#define AUTH_MAX_ENCODE_BASE64_LENGTH       (AUTH_SHA256_BUFFER_SIZE * 4u)
#define AUTH_HMAC_LENGTH                    (AUTH_MAX_ENCODE_BASE64_LENGTH)

/***************************************************************************
 * @brief Build canonical string for v1 authentication
 * @param version - version to use to build the canonical string value
 * @param device_id - device_id to use to build the canonical string value
 * @param timestamp - timestamp to use to build the canonical string value
 * @param method - method to use to build the canonical string value
 * @param path - path to use to build the canonical string value
 * @param body_hash - body_hash to use to build the canonical string value
 */ 
char * auth_build_canonical_string(const char* version, const char* device_id,
                           long timestamp, const char* method,
                           const char* path, const char* body_hash);

/***************************************************************************
 * @brief Hash request body with SHA-256
 * @param body - data to do the hash on
 * @param length - length of the body
 * @return hashed body data
 */ 
char * auth_hash_request_body(const char * body, size_t length);

/***************************************************************************
 * @brief Encode binary data to Base64
 * @param input - data to encode to base 64
 * @param length - length of the data to encode
 * @return encoded base 64 data 
 */ 
char * auth_encode_base64(const unsigned char* input, size_t length);

/***************************************************************************
 * @brief Generate HMAC-SHA256 signature for canonical string
 * @param canonical_string - string to generate HMAC from, must be string terminated 
 *          (necessary to get the string length from)
 * @param secret - secret key string to generate HMAC from, must be string terminated 
 *          (necessary to get the string length from)
 * @return generated HMAC string, NULL if any error occurs
 */
char * auth_generate_hmac(const char * canonical_string, const char * secret);

#endif
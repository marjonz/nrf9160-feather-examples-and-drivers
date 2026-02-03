#include "auth.h"

#include "lib/macro.h"
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*****************************************
 * Local Variables
 */
static char hash_request_body[AUTH_MAX_HASH_REQ_BODY_LENGTH];
static char canonical_string[AUTH_MAX_CANONICAL_STRING_LENGTH];
static char encode_base64_string[AUTH_MAX_ENCODE_BASE64_LENGTH];

/*****************************************
 * Public Functions
 */
// Build canonical string for v1 authentication
char * auth_build_canonical_string(const char* version, const char* device_id,
                           int64_t timestamp, const char* method,
                           const char* path, const char* body_hash) 
{
    ZERO_ARRAY(canonical_string);
    // Append each component followed by newline
    snprintf(canonical_string, sizeof(canonical_string), "%s \n" "%s \n" "%" PRIi64 " \n" "%s \n" "%s \n" "%s \n",
        version, device_id, timestamp, method, path, body_hash);

    return canonical_string;
}

char * auth_hash_request_body(const char * body, size_t length)
{
    unsigned char hash_result[AUTH_SHA256_BUFFER_SIZE];  // SHA-256 = 32 bytes
    ZERO_ARRAY(hash_result);

    mbedtls_md_context_t ctx = {0};
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_starts(&ctx);

    // Hash body (handles NULL/empty case)
    if (body != NULL && length > 0) 
    {
        mbedtls_md_update(&ctx, body, length);
    }

    mbedtls_md_finish(&ctx, hash_result);
    mbedtls_md_free(&ctx);

    // Convert to hex string
    ZERO_ARRAY(hash_request_body);
    for (uint8_t i = 0u; i < sizeof(hash_result); i++) 
    {
        char hex_buff[3u];
        ZERO_ARRAY(hex_buff);
        snprintf(hex_buff, sizeof(hex_buff), "%02x", hash_result[i]);
        strcat(hash_request_body, hex_buff);
    }

    return hash_request_body;
}

// Encode binary data to Base64
char * auth_encode_base64(const unsigned char* input, size_t length) 
{
    // Calculate required output buffer size
    size_t outputLen = 0;

    // First call: get required size
    mbedtls_base64_encode(NULL, 0, &outputLen, input, length);

    // Allocate buffer (add 1 for null terminator)
    ZERO_ARRAY(encode_base64_string); // Make sure it is Null terminated

    if (outputLen >= sizeof(encode_base64_string))
    {
        // The output is too large for the container (including NULL terminator), exit.
        return NULL;
    }

    // Second call: perform encoding
    int result = mbedtls_base64_encode(encode_base64_string, outputLen, &outputLen, input, length);
    if (result != 0) 
    {
        return NULL;  // Error encoding
    }

    return encode_base64_string;
}

// Generate HMAC-SHA256 signature for canonical string
char * auth_generate_hmac(const char * canonical_string, const char * secret)
{
    unsigned char hmac_result[AUTH_SHA256_BUFFER_SIZE];  // SHA-256 = 32 bytes
    ZERO_ARRAY(hmac_result);

    mbedtls_md_context_t ctx = {0};
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)canonical_string, strlen(canonical_string));
    mbedtls_md_hmac_finish(&ctx, hmac_result);
    mbedtls_md_free(&ctx);

    // Return Base64-encoded signature
    return auth_encode_base64(hmac_result, sizeof(hmac_result));
}

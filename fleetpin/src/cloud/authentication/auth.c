#include "auth.h"

#include "lib/macro.h"
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(auth, LOG_LEVEL_ERR);

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
                           uint64_t timestamp, const char* method,
                           const char* path, const char* body_hash) 
{
    ZERO_ARRAY(canonical_string);
    // This is a horrible assumption, but the assumption is that all these pointers will fit in the target buffer.
    // Append each component followed by newline
    char * end_of_string = strncat(canonical_string, version, strlen(version));    
    end_of_string = strncat(end_of_string, " \n", 3); // Append whitespace + newline
    end_of_string = strncat(end_of_string, device_id, strlen(device_id));
    end_of_string = strncat(end_of_string, " \n", 3); // Append whitespace + newline
    uint8_t timestamp_str[20u] = {0}; // Large enough to hold 64 bit int
    uint8_t count = snprintf(timestamp_str, sizeof(timestamp_str), "%llu \n", timestamp);
    LOG_ERR("Wrote %d bytes to %s", count, timestamp_str); 
    end_of_string = strncat(end_of_string, timestamp_str, strlen(timestamp_str));
    //end_of_string = strncat(end_of_string, " \n", 3); // Append whitespace + newline
    end_of_string = strncat(end_of_string, method, strlen(method));
    end_of_string = strncat(end_of_string, " \n", 3); // Append whitespace + newline
    end_of_string = strncat(end_of_string, path, strlen(path));
    end_of_string = strncat(end_of_string, " \n", 3); // Append whitespace + newline

    //version, device_id, timestamp, method, path);
    if ((body_hash != NULL) && strlen(body_hash) > 0)
    {
        // Append body hash if it is not empty
        end_of_string = strncat(end_of_string, body_hash, strlen(body_hash));
        if (end_of_string == NULL)
        {
            LOG_ERR("Failed to append body hash to canonical string.");
            return NULL; // Error building string
        }
        (void) strncat(end_of_string, " \n", 3); // Append whitespace + newline after body hash
    }
    return canonical_string;
}

char * auth_hash_request_body(const char * body, size_t length)
{
    if (body == NULL || length == 0)
    {
        // Return hash of empty string
        body = "";
        length = 0;
        return NULL;
    }
    
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
    int err = 0;

    #ifndef ORIGINAL
    //Allocate memory to ctx 
    struct mbedtls_md_context_t* ctx = (struct mbedtls_md_context_t*)malloc(sizeof(struct mbedtls_md_context_t)); 
    if (ctx == NULL) 
    {
        LOG_ERR("Unable to allocate memory"); 
    }
    //Get info 
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(info == NULL) 
    {
        LOG_ERR("Couldnt find info for SHA256"); 
    }
    else 
    {
        LOG_ERR("Info Size: %d, Info Name: %s", mbedtls_md_get_size(info), mbedtls_md_get_name(info)); 
    }
    //Init the context. Just does a memset()
    mbedtls_md_init(ctx);
    //Setup ctx with info and using hmac 
    err = mbedtls_md_setup(ctx, info, 0); //No hmac?
    if(err) 
    {
        LOG_ERR("Setup Fail: %d", err);
        err = 0; 
    }
    mbedtls_md_hmac_starts(ctx, (const unsigned char*)secret, strlen(secret));
    if(err) 
    {
        LOG_ERR("HMAC Starts Fail: %d", err);
        err = 0;
    }
    mbedtls_md_hmac_update(ctx, (const unsigned char*)canonical_string, strlen(canonical_string));
    if(err) 
    {
        LOG_ERR("HMAC Update Fail: %d", err);
        err = 0;
    }
    mbedtls_md_hmac_finish(ctx, hmac_result);
    if(err) 
    {
        LOG_ERR("HMAC Finish Fail: %d", err);
        err = 0;
    }

    mbedtls_md_free(ctx);

    free(ctx); 
    ctx = NULL;

    #else 
    
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(info == NULL) 
    {
        LOG_ERR("Couldnt find info for SHA256"); 
    }
 
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    err = mbedtls_md_setup(&ctx, info, 1); 
    if(err) 
    {
        LOG_ERR("Setup Fail: %d", err);
        err = 0; 
    }
    
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
    if(err) 
    {
        LOG_ERR("HMAC Starts Fail: %d", err);
        err = 0;
    }
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)canonical_string, strlen(canonical_string));
    if(err) 
    {
        LOG_ERR("HMAC Update Fail: %d", err);
        err = 0;
    }
    mbedtls_md_hmac_finish(&ctx, hmac_result);
    if(err) 
    {
        LOG_ERR("HMAC Finish Fail: %d", err);
        err = 0;
    }
    
    mbedtls_md_free(&ctx);
    #endif 

    //Can just call mbedtls_md_hmac which does all the above?

    LOG_ERR("HMAC result: %s, len %d", hmac_result, strlen(hmac_result)); 

    // Return Base64-encoded signature
    return auth_encode_base64(hmac_result, sizeof(hmac_result));
}

#include "auth.h"
#include "mbedtls/md.h"

#include <string.h>

#define SHA256_BUFFER_SIZE      32u
#define MAX_HMAC_BUFFER_SIZE    (SHA256_BUFFER_SIZE * 4u)

/*****************************************
 * Local Variables
 */
//String deviceId = "device-1234";                // Unique per device
// TODO: Pull from the filesystem
static const char* deviceSecret = "super-secret-key";  // Store securely in NVS ideally
static char hmac_hex[MAX_HMAC_BUFFER_SIZE] = {0};

/*****************************************
 * Public Functions
 */
char * auth_generate_hmac(const char * payload, const char * timestamp) 
{
    unsigned char hmacResult[SHA256_BUFFER_SIZE];  // SHA-256 = 32 bytes

    char temp_message_buffer[MAX_HMAC_BUFFER_SIZE];
    memset(temp_message_buffer, 0, sizeof(temp_message_buffer));
    // copy the payload and timestamp into the buffer.
    strncat(temp_message_buffer, payload, sizeof(temp_message_buffer));
    strncat(temp_message_buffer, timestamp, sizeof(temp_message_buffer));

    mbedtls_md_context_t ctx = {0};
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)deviceSecret, strlen(deviceSecret));
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)temp_message_buffer, strlen(temp_message_buffer));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert to hex string
    char * hmac_hex_ptr = hmac_hex;
    for (uint8_t i = 0u; i < sizeof(hmacResult); i++) 
    {
        char hex_buff[3u];
        snprintf(hex_buff, sizeof(hex_buff), "%02x", hmacResult[i]);
        strncat(hmac_hex, hex_buff, sizeof(hmac_hex));
    }

    return &hmac_hex;
}
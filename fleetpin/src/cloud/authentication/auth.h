#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>

/***********************************************************************
 * @brief function to generate the HMAC from the given payload and 
 * timestamp strings
 * @param payload - payload to use to generate HMAC
 * @param timestemp - current device boot up time in milliseconds (string)
 * @return generated HMAC string
 */
char * auth_generate_hmac(const char * payload, const char * timestamp_ms) ;

#endif
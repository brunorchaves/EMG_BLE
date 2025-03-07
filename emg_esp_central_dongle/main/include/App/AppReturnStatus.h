#ifndef __APP_RETURN_STATUS_H__
#define __APP_RETURN_STATUS_H__

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

typedef enum E_AppStatus {
    APP_TIMEOUT_ERROR           = -6,
    APP_HARDWARE_ERROR          = -5,
    APP_INVALID_PARAMETER_ERROR = -4,
    APP_FAILED_ERROR            = -3,
    APP_NOT_INITIALIZED_ERROR   = -2,
    APP_GENERAL_ERROR           = -1,
    APP_OK                      = 0,
    APP_IS_BUSY                 = 1,
    APP_BUFFER_FULL             = 2,
    APP_BUFFER_EMPTY            = 3,
    APP_INVALID_REQUEST         = 4,
    APP_CHECKSUM_ERROR          = 5,
} E_AppStatusTypeDef;

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __APP_RETURN_STATUS_H__ */
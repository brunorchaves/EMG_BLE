/******************************************************************************
 * @file         : WirelessConnectionsProfileService.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2024 CMOS Drake
 * @brief        : Perfil bluetooth para informações sobre conexões sem fio
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef WIRELESS_CONNECTIONS_PROFILE_SERVICE_H
#define WIRELESS_CONNECTIONS_PROFILE_SERVICE_H

#ifdef __cplusplus
extern "C" 
{
#endif

#include <stdint.h>
#include <stdio.h>

/* Public functions ----------------------------------------------------------*/

extern void WirelessConnectionsProfile_SRV_SetCharValue(
  uint8_t charId, uint8_t size, const uint8_t* value);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_CONNECTIONS_PROFILE_SERVICE_H */

/*****************************END OF FILE**************************************/

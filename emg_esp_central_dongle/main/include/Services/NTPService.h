/******************************************************************************
 * @file         : NTPService.h
 * @author       : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de data / hora via NTP (Network Time
 * Protocol)
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef NTP_SERVICE_H
    #define NTP_SERVICE_H

    #ifdef __cplusplus
extern "C" {
    #endif

/* Includes ------------------------------------------------------------------*/

    #include <stdio.h>

    #include <esp_event.h>
    #include <esp_log.h>
    #include <esp_system.h>
    #include <esp_wifi.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/event_groups.h>
    #include <freertos/task.h>
    #include <lwip/apps/sntp.h>
    #include <sys/time.h>
    #include <time.h>

/* Public functions ----------------------------------------------------------*/

void SRV_NTP_Init();
void SRV_NTP_ObtainTime(void);
void SRV_NTP_PrintLocalTime(void);
time_t SRV_NTP_GetUnixTimestamp(void);
char *SRV_NTP_GetUnixTimestampString(void);
void SRV_NTP_FormatTimestamp(time_t currentTime, char *p_data, uint8_t dataLen);

    #ifdef __cplusplus
}
    #endif

#endif /* NTP_SERVICE_H */

/*****************************END OF FILE**************************************/

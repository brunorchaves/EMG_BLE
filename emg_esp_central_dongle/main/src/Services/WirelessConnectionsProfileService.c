#include "Services/WirelessConnectionsProfileService.h"

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"
#include "Services/WifiProvisionService.h"

#include "App/AppData.h"
#include "string.h"
#include <esp_wifi.h>

void WirelessConnectionsProfile_SRV_SetCharValue(uint8_t charId,
                                                 uint8_t size,
                                                 const uint8_t *value) {
    switch (charId) {
        default:
            break;

        case WIRELESS_CONNECTIONS_WIFI_REQUEST_STATE_CHAR:
            {
                wifi_ap_record_t acessPointInfo;

                uint8_t wifiStatus = WIFI_STATUS_ENABLED;

                if (esp_wifi_sta_get_ap_info(&acessPointInfo) == ESP_OK) {
                    printf("WiFi Status: Connected\n");
                    wifiStatus = WIFI_STATUS_CONNECTED;
                }

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                    sizeof(wifiStatus),
                    &wifiStatus);

                break;
            }

        case WIRELESS_CONNECTIONS_WIFI_REQUEST_QRCODE_CHAR:
            {
                uint8_t *qrCode = (uint8_t *)SRV_WifiProvision_GetQrCode();

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_WIFI_QRCODE_CHAR,
                    strlen((char *)qrCode),
                    qrCode);

                vPortFree((void *)qrCode);

                break;
            }

        case WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_ENABLED_STATE_CHAR:
            {
                uint8_t value = Bluetooth_SRV_EnabledState();

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_ENABLED_STATE_CHAR,
                    sizeof(value),
                    &value);

                break;
            }

        case WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_PAIRED_STATE_CHAR:
            {
                uint8_t value = Bluetooth_SRV_ConnectionState();

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_PAIRED_STATE_CHAR,
                    sizeof(value),
                    &value);

                break;
            }

        case WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_ENABLE_CHAR:
            {
                Bluetooth_SRV_Init();

                uint8_t value = Bluetooth_SRV_EnabledState();

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_ENABLED_STATE_CHAR,
                    sizeof(value),
                    &value);

                break;
            }

        case WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_DISABLE_CHAR:
            {
                Bluetooth_SRV_Deinit();

                uint8_t value = 0;

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_ENABLED_STATE_CHAR,
                    sizeof(value),
                    (uint8_t *)&value);

                break;
            }

        case WIRELESS_CONNECTIONS_WIFI_DISCONECT_CHAR:
            {
                uint8_t value = WIFI_STATUS_ENABLED;

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                    sizeof(value),
                    &value);

                printf("\n \n \n CHAMADA PARA DESCONEXÃO WIFI \n \n \n");
                SRV_WifiProvision_WifiDisconnectAndForgetNetwork();

                break;
            }

        case WIRELESS_CONNECTIONS_WIFI_NETWORK_NAME_CHAR:
            {
                wifi_ap_record_t acessPointInfo;

                if (esp_wifi_sta_get_ap_info(&acessPointInfo) == ESP_OK) {
                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_NETWORK_NAME_CHAR,
                        strlen((char *)acessPointInfo.ssid),
                        acessPointInfo.ssid);
                }

                break;
            }
    }
}
# Introduction 


# Getting Started

1.	Installation process
2.	Software dependencies
3.	Latest releases
4.	API references

# Build and Test


# Contribute

# Custom implements

1.  Vá até o arquivo "esp_https_ota.c" e, ao final dele, implemente a seguinte função:

```
int esp_https_ota_get_status_code(esp_https_ota_handle_t https_ota_handle)
{   
    esp_https_ota_t *https_ota = https_ota_handle;
    int status_code = esp_http_client_get_status_code(https_ota->http_client);
    return status_code;
}
```

2. Vá até o HEADER desse mesmo arquivo e declare o protótipo da função implementada:
```

/**
* @brief  This function returns the status code.
*
* @param[in]   https_ota_handle   pointer to esp_https_ota_handle_t structure
*
* @return
*    - status_code
*/
int esp_https_ota_get_status_code(esp_https_ota_handle_t https_ota_handle);
```

3.  Salve, compile e tudo certo!
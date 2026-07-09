#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "weather.h"

static bool scan_json_string(const char *start, const char *key, char *out, unsigned int out_size)
{
    const char *p;
    char fmt[32];

    if (out == NULL || out_size == 0)
        return false;

    p = strstr(start, key);
    if (p == NULL)
        return false;

    snprintf(fmt, sizeof(fmt), "%s\"%%%u[^\"]\"", key, out_size - 1);
    return sscanf(p, fmt, out) == 1;
}

bool parse_seniverse_response(const char *response, weather_info_t *info)
{
    const char *location_response;
    const char *now_response;
    char temperature_str[16] = { 0 };
    bool has_temperature = false;

    if (response == NULL || info == NULL)
        return false;

    memset(info, 0, sizeof(*info));
    info->weather_code = -1;

    location_response = strstr(response, "\"location\":");
    if (location_response == NULL)
        location_response = response;

    scan_json_string(location_response, "\"name\":", info->city, sizeof(info->city));
    scan_json_string(location_response, "\"path\":", info->loaction, sizeof(info->loaction));

    now_response = strstr(response, "\"now\":");
    if (now_response == NULL)
        now_response = response;

    scan_json_string(now_response, "\"text\":", info->weather, sizeof(info->weather));

    if (scan_json_string(now_response, "\"code\":", temperature_str, sizeof(temperature_str)))
    {
        info->weather_code = atoi(temperature_str);
    }

    memset(temperature_str, 0, sizeof(temperature_str));
    if (scan_json_string(now_response, "\"temperature\":", temperature_str, sizeof(temperature_str)))
    {
        info->temperature = atof(temperature_str);
        has_temperature = true;
    }

    return info->weather_code >= 0 && has_temperature;
}
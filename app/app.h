#ifndef __APP_H__
#define __APP_H__

#define MILLISECONDS(x)  (x)
#define SECONDS(x)       MILLISECONDS((x) * 1000)
#define MINUTES(x)       SECONDS((x) * 60)
#define HOURS(x)         MINUTES((x) * 60)
#define DAYS(x)          HOURS((x) * 24)

#define TIME_SYNC_INTERVAL        DAYS(1)
#define WIFI_UPDATE_INTERVAL      SECONDS(5)
#define TIME_UPDATE_INTERVAL      SECONDS(1)
#define INNER_UPDATE_INTERVAL     SECONDS(3)
#define OUTDOOR_UPDATE_INTERVAL   MINUTES(1)

void app_init(void);
void app_network_start(void);
void app_start(void);

#endif

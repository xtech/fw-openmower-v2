//
// Created by clemens on 01.10.24.
//

#ifndef SERVICE_DISCOVERY_H
#define SERVICE_DISCOVERY_H

#define SD_MULTICAST_GROUP "255.255.255.255"
#define SD_MULTICAST_PORT 8007
#define SD_INTERVAL 1500

void InitBootloaderServiceDiscovery(void);

#endif  // SERVICE_DISCOVERY_H

/* 
 * File:   wlan_event.h
 * Author: pluto
 *
 * Created on 06 March 2016, 12:14
 */

#ifndef WLAN_EVENT_H
#define WLAN_EVENT_H


void wlan_event_normal_connect_failed(void* data);

void wlan_event_normal_connected(void* data);

void wlan_event_normal_link_lost(void* data);


void wlan_event_connect_failed();



#ifdef __cplusplus
extern "C" {
#endif




#ifdef __cplusplus
}
#endif

#endif /* WLAN_EVENT_H */


/* 
 * File:   common.h
 * Author: pluto
 *
 * Created on 06 March 2016, 12:06
 */

#ifndef COMMON_H
#define COMMON_H


enum state {
    CONNECTED = 1,
    DISCONNETED 
};

extern enum state DEVICE_STATE;


extern int GLOBAL_COUNTER;


#ifdef __cplusplus
extern "C" {
#endif




#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */


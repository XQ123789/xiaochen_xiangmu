#ifndef __LED_H
#define __LED_H	 
#include "sys.h"

void LED_Init(void);			//LEDµ∆≥ı ºªØ
void CSTX_4GCTR_Init(void);
void LED_Run(void);
void wgs84_to_gcj02(double wgs_lon, double wgs_lat, double *gcj_lon, double *gcj_lat); 
void wgs84_to_amap(double wgs_lon, double wgs_lat, double *amap_lon, double *amap_lat);

#define LED1     PBout(5)
#define LED2     PBout(4)
#define LED3     PBout(3)

#define PWRKEY  PCout(7)
#define RESET   PCout(6)

#endif

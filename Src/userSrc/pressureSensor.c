/*
 * pressureSensorSPI.c
 *
 *  Created on: Mar 29, 2018
 *      Author: 402072495
 */
#include <pressureSensor.h>
#include "string.h"
#include <stdlib.h>

static uint32_t DummyByte[] = { 0xFFFFFFFF };
static float PaPerPSI = 6895;
float Patm = 101325;

/************************************HoneyWell 060PGSA3 gauge pressure sensor****************************************/
static void PRESSURESPI_CS_LOW(HW060GAUGE_DEVICE *ptPRESSURESPIDev) {
	ptPRESSURESPIDev->CS_Port->BSRR = (uint32_t) ptPRESSURESPIDev->CS_Pin << 16U;
}

static void PRESSURESPI_CS_HIGH(HW060GAUGE_DEVICE *ptPRESSURESPIDev) {
	ptPRESSURESPIDev->CS_Port->BSRR = (uint32_t) ptPRESSURESPIDev->CS_Pin;
}

static void HW060PGSA3_readRawSPI(HW060GAUGE_DEVICE *ptPressureDev) {
	uint16_t tem[2];
	/*read SPI pressure and temperature, 4 bytes*/
	PRESSURESPI_CS_LOW(ptPressureDev);
	HAL_SPI_TransmitReceive(ptPressureDev->pressure_spi, (uint8_t *) DummyByte, (uint8_t *) tem, 2, 1);
	PRESSURESPI_CS_HIGH(ptPressureDev);
	ptPressureDev->rawSPIPressure = (int16_t) (tem[0] & 0x3FFF);
	ptPressureDev->rawSPITemperature = (int16_t) (tem[1] >> 5);
}


static void HW060PGSA3_ValidateData(HW060GAUGE_DEVICE *pt) {
	pt->Pressure = (float) (pt->rawSPIPressure - pt->uOutMin) / (pt->uOutMax - pt->uOutMin) * (pt->PMax - pt->PMin) + pt->PMin; //unit Pa
	pt->Temperature = pt->rawSPITemperature / 10.235f - 50.0f;

}

static float HW060PGSA3_getPressure(PRESSURE_DEVICE *ptPressureDev) {
	HW060GAUGE_DEVICE *pt = (HW060GAUGE_DEVICE *) ptPressureDev;
	HW060PGSA3_readRawSPI(pt);
	HW060PGSA3_ValidateData(pt);
	return pt->Pressure;
}

HW060GAUGE_DEVICE *HW060PGSA3(SPI_HandleTypeDef *hspi, GPIO_TypeDef *csPort, uint16_t csPin, uint16_t jointNum, uint16_t seq) {
	HW060GAUGE_DEVICE *ptHW060PGSA3 = (HW060GAUGE_DEVICE *) malloc(sizeof(HW060GAUGE_DEVICE));
	if (ptHW060PGSA3 == NULL)
		return NULL;
	memset(ptHW060PGSA3, 0, sizeof(HW060GAUGE_DEVICE));
	ptHW060PGSA3->pressure_spi = hspi;
	ptHW060PGSA3->CS_Port = csPort;
	ptHW060PGSA3->CS_Pin = csPin;
	ptHW060PGSA3->base.jointNum = jointNum;
	ptHW060PGSA3->base.position = seq;
	ptHW060PGSA3->base.getPressure = HW060PGSA3_getPressure;
	ptHW060PGSA3->PMax = 60 * PaPerPSI + Patm;  //60*6895 Pa
	ptHW060PGSA3->PMin = Patm;
	ptHW060PGSA3->uOutMin = 0x0666;
	ptHW060PGSA3->uOutMax = 0x3999;
	return ptHW060PGSA3;
}
/****************************************************************************************************************/

/***********************HoneyWell 060PAAA5 Absolute Pressure Sensor********************************************/
static float HW060PAAA5_getPressure(PRESSURE_DEVICE *ptPressureDev) {
	HW060ABSOLUTE_DEVICE *pt = (HW060ABSOLUTE_DEVICE *) ptPressureDev;
	float vtem = pt->ptADDevice->fChannel[pt->ADPort];
	float ptem = (vtem - pt->VMin) / (pt->VMax - pt->VMin) * (pt->PMax - pt->PMin) + pt->PMin;
	pt->base.pressure = ptem;
	return ptem;
}

HW060ABSOLUTE_DEVICE *HW060PAAA5(AD_DEVICE *ptADDev, uint16_t ADPort, uint16_t jointNum, uint16_t seq) {
	HW060ABSOLUTE_DEVICE* ptHW060PAA5 = (HW060ABSOLUTE_DEVICE *) malloc(sizeof(HW060ABSOLUTE_DEVICE));
	if (ptHW060PAA5 == NULL)
		return NULL;
	memset(ptHW060PAA5, 0, sizeof(HW060ABSOLUTE_DEVICE));
	ptHW060PAA5->ptADDevice = ptADDev;
	ptHW060PAA5->ADPort = ADPort;
	ptHW060PAA5->base.jointNum = jointNum;
	ptHW060PAA5->base.position = seq;
	ptHW060PAA5->VMax = 0.9 * 5.0f; //output 10%-90% of Vsupply
	ptHW060PAA5->VMin = 0.1 * 5.0f;
	ptHW060PAA5->PMax = 60 * PaPerPSI;
	ptHW060PAA5->PMin = 0;
	ptHW060PAA5->base.getPressure = HW060PAAA5_getPressure;
	return ptHW060PAA5;
}
/************************************************************************************************************/

/*************************************************Pressure Hub************************************************/
static void pressureHub_attachPressureDev(PRESSURE_HUB *ptPressureHub, PRESSURE_DEVICE *ptPressureDev) {
	ptPressureHub->pressureDevices[ptPressureDev->jointNum][ptPressureDev->position] = ptPressureDev;
	ptPressureDev->pParent = ptPressureHub;
	ptPressureHub->Num += 1;
}

static float pressureHub_getPressure(PRESSURE_HUB *ptPressureHub, uint16_t jointN, uint16_t seq) {
	PRESSURE_DEVICE *pt = ptPressureHub->pressureDevices[jointN][seq];
	return pt->getPressure(pt);
}

static uint16_t pressureHub_getPressureAll(PRESSURE_HUB *ptPressureHub) {
	PRESSURE_DEVICE *pt;
	uint16_t n = 0;
	for (int i = 0; i < JOINT_NUM; i++)
		for (int j = 0; j < 2; j++)
			if (NULL != (pt = ptPressureHub->pressureDevices[i][j])) {
				ptPressureHub->Pressure[i][j] = pt->getPressure(pt);
				n++;
			}
	return n;
}

PRESSURE_HUB *PRESSUREHUB(CENTRAL *ptCentral) {

	HW060ABSOLUTE_DEVICE *ptHW060PAAA5;
	//new a pressureSensorHub on the heap
	PRESSURE_HUB *ptPressureHub = (PRESSURE_HUB *) malloc(sizeof(PRESSURE_HUB));
	if (ptPressureHub == NULL)
		return NULL;
	memset(ptPressureHub, 0, sizeof(PRESSURE_HUB));

	//add parent Central
	ptPressureHub->pParent = ptCentral;
	ptCentral->ptPressureHub = ptPressureHub;

	//attach methods
	ptPressureHub->attach = pressureHub_attachPressureDev;
	ptPressureHub->getPressure = pressureHub_getPressure;
	ptPressureHub->getPressureAll = pressureHub_getPressureAll;

	/*Add one pressure sensor, attached to ADPort  0, JointNum 0,  position 0*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 0, 0, 0);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  1, JointNum 0,  position 1*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 1, 0, 1);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  2, JointNum 1,  position 0*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 2, 1, 0);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  3, JointNum 1,  position 1*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 3, 1, 1);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  4, JointNum 2,  position 0*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 4, 2, 0);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  5, JointNum 2,  position 1*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 5, 2, 1);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  6, JointNum 3,  position 0*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 6, 3, 0);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	/*Add one pressure sensor, attached to ADPort  7, JointNum 3,  position 1*/
	ptHW060PAAA5 = HW060PAAA5(&(ptCentral->ADDevice), 7, 3, 1);
	ptPressureHub->attach(ptPressureHub, (PRESSURE_DEVICE *) ptHW060PAAA5);

	return ptPressureHub;
}

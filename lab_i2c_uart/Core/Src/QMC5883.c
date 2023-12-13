/*
 * QMC5883.c
 *
 *  Created on: 11 May 2021
 *      Author: Serdar
 */
#include"QMC5883.h"


//###############################################################################################################
uint8_t QMC_init(QMC_t *qmc,I2C_HandleTypeDef *i2c,uint8_t Output_Data_Rate)
{
	uint8_t array[2];
	qmc->i2c=i2c;
	array[0]=0x01;
	array[1]=0x01;

	qmc->offsets[0] = 0;
	qmc->offsets[1] = 0;
	qmc->offsets[2] = 0;

	qmc->scales[0] = 1;
	qmc->scales[1] = 1;
	qmc->scales[2] = 1;

	if(Output_Data_Rate==200)array[1]|=0b00001100;
	else if(Output_Data_Rate==100)array[1]|=0b00001000;
	else if(Output_Data_Rate==50)array[1]|=0b00000100;
	else if(Output_Data_Rate==10)array[1]|=0b00000000;
	else array[1]|=0b00001100;

	if(HAL_I2C_Mem_Write(qmc->i2c, 0x1A, 0x0B, 1, &array[0], 1, HAL_MAX_DELAY)!=HAL_OK)return 1;
	if(HAL_I2C_Mem_Write(qmc->i2c, 0x1A, 0x09, 1, &array[1], 1, HAL_MAX_DELAY)!=HAL_OK)return 2;

	return 0;
}

uint8_t QMC_Standby(QMC_t *qmc)
{
	uint8_t command = 0x0;
	if(HAL_I2C_Mem_Write(qmc->i2c, 0x1A, 0x09, 1, &command, 1, HAL_MAX_DELAY)!=HAL_OK)return 1;
	return 0;
}

uint8_t QMC_Reset(QMC_t *qmc)
{
	uint8_t command= 0x80;
	if(HAL_I2C_Mem_Write(qmc->i2c, 0x1A, 0x0A, 1, &command, 1, HAL_MAX_DELAY)!=HAL_OK)return 1;
	return 0;
}

static void QMC_apply_calibration(QMC_t *qmc){
	qmc->X = (qmc->rawX - qmc->offsets[0]) * qmc->scales[0];
	qmc->Y = (qmc->rawY - qmc->offsets[1]) * qmc->scales[1];
	qmc->Z = (qmc->rawZ - qmc->offsets[2]) * qmc->scales[2];
}

uint8_t QMC_read(QMC_t *qmc)
{
	  qmc->datas[0]=0;
	  HAL_I2C_Mem_Read(qmc->i2c, 0x1A, 0x06, 1, qmc->datas, 1, HAL_MAX_DELAY);

	  if(!(qmc->datas[0] & 1))
		  return 1;

	  HAL_I2C_Mem_Read(qmc->i2c, 0x1A, 0x00, 1, qmc->datas, 6, HAL_MAX_DELAY);
	  qmc->rawX= (int16_t)((qmc->datas[1]<<8) | qmc->datas[0]);
	  qmc->rawY= (int16_t)((qmc->datas[3]<<8) | qmc->datas[2]);
	  qmc->rawZ= (int16_t)((qmc->datas[5]<<8) | qmc->datas[4]);

	  QMC_apply_calibration(qmc);

	  return 0;
}

void QMC_Calibrate(QMC_t *qmc)
{
	int16_t	calibration_data[3][2] = {{32767,-32768},{32767,-32768},{32767,-32768}};

	uint32_t start_time = (uint32_t)xTaskGetTickCount();
	while((uint32_t)xTaskGetTickCount() - start_time < 30000){
		if(QMC_read(qmc) == 0){
			if(qmc->rawX < calibration_data[0][0]) calibration_data[0][0] = qmc->rawX;
			if(qmc->rawX > calibration_data[0][1]) calibration_data[0][1] = qmc->rawX;

			if(qmc->rawY < calibration_data[1][0]) calibration_data[1][0] = qmc->rawY;
			if(qmc->rawY > calibration_data[1][1]) calibration_data[1][1] = qmc->rawY;

			if(qmc->rawZ < calibration_data[2][0]) calibration_data[2][0] = qmc->rawZ;
			if(qmc->rawZ > calibration_data[2][1]) calibration_data[2][1] = qmc->rawZ;
		}
		osDelay(50);
	}

	qmc->offsets[0] = (float)(calibration_data[0][0] + calibration_data[0][1]) / 2.0;
	qmc->offsets[1] = (float)(calibration_data[1][0] + calibration_data[1][1]) / 2.0;
	qmc->offsets[2] = (float)(calibration_data[2][0] + calibration_data[2][1]) / 2.0;

	float x_avg_delta = (calibration_data[0][1] - calibration_data[0][0])/2;
	float y_avg_delta = (calibration_data[1][1] - calibration_data[1][0])/2;
	float z_avg_delta = (calibration_data[2][1] - calibration_data[2][0])/2;

	float avg_delta = (x_avg_delta + y_avg_delta + z_avg_delta) / 3;

	qmc->scales[0] = (float)(avg_delta / x_avg_delta);
	qmc->scales[1] = (float)(avg_delta / y_avg_delta);
	qmc->scales[2] = (float)(avg_delta / z_avg_delta);
}


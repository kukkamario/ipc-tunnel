#ifndef VARIANT_H
#define VARIANT_H

#include <stdint.h>
#include <stdbool.h>

bool VARIANT_Initialize(void* platform);

void VARIANT_Destruct(void);

uint32_t VARIANT_ReadChan0(uint8_t* buffer, uint32_t size);
uint32_t VARIANT_ReadChan1(uint8_t* buffer, uint32_t size);
uint32_t VARIANT_ReadChan2(uint8_t* buffer, uint32_t size);

bool VARIANT_WriteChan0(const uint8_t* buffer, uint32_t size);
bool VARIANT_WriteChan1(const uint8_t* buffer, uint32_t size);
bool VARIANT_WriteChan2(const uint8_t* buffer, uint32_t size);

uint32_t VARIANT_PacketSizeChan0(void);
uint32_t VARIANT_PacketSizeChan1(void);
uint32_t VARIANT_PacketSizeChan2(void);

#endif // VARIANT_H

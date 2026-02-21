/*
 * mcp_port_stm32_hal.c
 *
 *  Created on: Feb 21, 2026
 *      Author: arch7560
 */


#include "mcp_port.h"
#include "main.h"
#include "stm32f1xx_hal.h"   // for F103; CubeIDE should have this

// ---------------- USER CONFIG ----------------
// 1) SPI handle name from CubeMX (check main.c / stm32f1xx_hal_msp.c)
extern SPI_HandleTypeDef hspi1;     // CHANGE if you use hspi2, etc.
#define MCP_SPI_HANDLE   hspi1

// 2) CS pin macros from main.h (CubeMX generates these)
// Example if you named pin "MCP_CS" in CubeMX:
//   #define MCP_CS_Pin GPIO_PIN_4
//   #define MCP_CS_GPIO_Port GPIOA
#define MCP_CS_GPIO_Port GPIOA      // CHANGE
#define MCP_CS_Pin       GPIO_PIN_4 // CHANGE
// ------------------------------------------------

static volatile uint8_t s_irq_flag = 0;

void mcp_port_delay_us(uint32_t us)
{
  // crude delay: ms-granularity via HAL_GetTick
  // fine for bring-up; replace with timer-based us delay later
  uint32_t ms = (us + 999u) / 1000u;
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < ms) { }
}

uint32_t mcp_port_get_tick_ms(void)
{
  return HAL_GetTick();
}

void mcp_port_lock(void)   { /* no-op for now */ }
void mcp_port_unlock(void) { /* no-op for now */ }

void mcp_port_cs_assert(void)
{
  HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET);
}

void mcp_port_cs_deassert(void)
{
  HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET);
}

bool mcp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  if (!tx || !rx || len == 0) return false;

  mcp_port_cs_assert();
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&MCP_SPI_HANDLE,
                                                (uint8_t*)tx,
                                                (uint8_t*)rx,
                                                len,
                                                100);
  mcp_port_cs_deassert();

  return (st == HAL_OK);
}

void mcp_port_notify_irq(void)
{
  s_irq_flag = 1;
}

bool mcp_port_poll_irq_notified(void)
{
  if (s_irq_flag) { s_irq_flag = 0; return true; }
  return false;
}

# VL53L7CX Test Firmware — NUCLEO-N657X0-Q

Single-sensor ranging test for the VL53L7CX ToF sensor using
the STSW-IMG036 (ULD) driver on the NUCLEO-N657X0-Q.

## Wiring: SATEL-VL53L7CX → NUCLEO-N657X0-Q

| SATEL Pin | Signal   | NUCLEO Pin | Header         |
|-----------|----------|------------|----------------|
| 1         | GND      | GND        | Any GND pin    |
| 2         | IOVDD    | 3V3        | CN5-4 or CN5-5 |
| 3         | AVDD     | 3V3        | CN5-4 or CN5-5 |
| 5         | LPn      | PA3        | CN14-3 (D10)   |
| 6         | SCL      | PH9        | **CN15-3**     |
| 7         | SDA      | PC1        | **CN15-5**     |
| 8         | I2C_RST  | PD7        | CN14-2 (D9)    |
| 9         | INT      | PD12       | CN13-1 (D8)    |

**I2C1 pull-ups (R88 = 1.5 kΩ, R89 = 1.5 kΩ) are already populated
on the NUCLEO for PH9/PC1.  No solder bridge changes are needed
for the morpho header wiring above.**

SATEL pin 4 (PWREN) can be left unconnected or tied to 3V3 if your
SATEL board uses it.

### Debug UART

USART3 TX on PD8 (Arduino D1, CN13-2) at 115200 baud, 8-N-1.
Connect a USB-UART adapter's RX to PD8, GND to GND.

If you prefer to use the STLINK VCP, check which UART it routes to
on the NUCLEO-N657X0-Q (may need firmware update on the STLINK) and
change the UART handle in nucleo_tof_test.c accordingly.

## Project Setup (STM32CubeIDE)

1. **Create a new project** targeting the NUCLEO-N657X0-Q board.
   Accept default clock configuration for now.

2. **Download STSW-IMG036** from st.com and extract the ULD package.

3. **Copy files into your project source tree:**
   ```
   YourProject/
   ├── Core/
   │   ├── Inc/
   │   │   ├── nucleo_tof_test.h      ← this file
   │   │   └── ...
   │   └── Src/
   │       ├── main.c                 ← add calls (see below)
   │       ├── nucleo_tof_test.c      ← this file
   │       └── ...
   ├── Drivers/
   │   └── VL53L7CX_ULD/             ← from STSW-IMG036
   │       ├── VL53L7CX_ULD_API/
   │       │   ├── inc/
   │       │   │   ├── vl53l7cx_api.h
   │       │   │   ├── vl53l7cx_buffers.h
   │       │   │   └── vl53l7cx_plugin_*.h  (optional)
   │       │   └── src/
   │       │       ├── vl53l7cx_api.c
   │       │       └── vl53l7cx_plugin_*.c  (optional)
   │       └── Platform/
   │           ├── platform.h          ← this file (REPLACES the empty shell)
   │           └── platform.c          ← this file
   ```

4. **Add include paths** in Project Properties → C/C++ Build → Settings →
   Tool Settings → MCU GCC Compiler → Include Paths:
   - `Drivers/VL53L7CX_ULD/VL53L7CX_ULD_API/inc`
   - `Drivers/VL53L7CX_ULD/Platform`

5. **Add source files** to the build.  Right-click on the folders in the
   Project Explorer and ensure they are included in the build.

6. **Verify the I2C timing value.**
   Open the .ioc file → Connectivity → I2C1 → Parameter Settings.
   Note the computed Timing value for 400 kHz Fast Mode.
   Copy it into `nucleo_tof_test.c` where it says `PLACEHOLDER`.
   Then REMOVE I2C1 from CubeMX (we init it manually in code).

## Usage in main.c

Paste this between the USER CODE markers in main.c:

```c
/* USER CODE BEGIN Includes */
#include "nucleo_tof_test.h"
/* USER CODE END Includes */

/* ...inside main(), after HAL_Init() and SystemClock_Config()... */

/* USER CODE BEGIN 2 */
if (tof_test_init() == 0) {
    tof_test_ranging_loop();  /* does not return */
}
/* If we get here, init failed — check USART3 output */
while (1) {
    HAL_Delay(1000);
}
/* USER CODE END 2 */
```

## Expected USART3 Output (success)

```
=== VL53L7CX Test — NUCLEO-N657X0-Q ===
[INIT] Configuring I2C1 (PH9/PC1)... OK
[INIT] Sensor power-on sequence... OK
[INIT] Checking vl53l7cx_is_alive()... OK — sensor detected at 0x52
[INIT] Running vl53l7cx_init() — downloading firmware...
       This takes several seconds at 400 kHz.
[INIT] vl53l7cx_init() OK — firmware loaded
[INIT] Setting 8x8 resolution... OK
[INIT] Setting 15 Hz ranging frequency... OK
[INIT] Sensor ready.

[RANGE] Starting continuous ranging...
--- frame ---
  320  318  315  322  ...
  ...
```

## Troubleshooting

**is_alive() fails:**
- Check wiring: SDA ↔ PC1, SCL ↔ PH9 (NOT the Arduino D14/D15 — 
  those require solder bridge changes)
- Verify 3.3V on SATEL IOVDD and AVDD
- Verify LPn is HIGH (PA3 should read 3.3V)
- Probe SCL/SDA with a scope — you should see I2C traffic

**init() fails (status 255 / timeout):**
- I2C timing register is wrong for your kernel clock — recalculate
- I2C bus stuck — toggle I2C_RST (PD7) high for 10ms then low
- Firmware download uses ~32 KB I2C writes — if HAL times out,
  increase I2C_TIMEOUT_MS in platform.c

**init() fails (status 66 / MCU failure):**
- I2C NACK at end of read transactions — verify RdMulti sends NACK
  before the final STOP condition (HAL handles this, but check if
  HAL_I2C_Mem_Read is returning HAL_OK)
- Power supply droop during firmware download — check AVDD stability

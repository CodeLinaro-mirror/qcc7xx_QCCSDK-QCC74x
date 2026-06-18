# Antenna Diversity

## Quick Start

**Important:**
Antenna diversity support is board-dependent. This feature requires specific hardware design with an RF switch and multiple antennas. Please verify your board's hardware capabilities before implementing this feature.


1. Check your board's antenna diversity capability:
    - Verify RF switch availability
    - Confirm multiple antenna connections

2. Implement antenna operations:

    - Reference the example implementation in `bsp/board/qcc743dk/board.c`
    - Implement and register antenna operations for your board

3. Configure feature in application:
    
    ```bash
    # Enable antenna diversity
    CONFIG_ANTENNA_CONTROL =y

    # Or disable antenna diversity
    CONFIG_ANTENNA_CONTROL =n
    ```

4. Compile

    ```bash
    make CHIP=qcc743 BOARD=qcc743dk
    ```

5. Flash

    ```bash
    make flash COMX=xxx ## xxx is your com name
    ```

## Configuration Guide

### Overview

The QCC74x SDK provides antenna diversity capabilities to optimize wireless performance through two complementary features:

- **Static Antenna Diversity**: Performs one-time antenna selection during connection setup using scan-based selection
- **Dynamic Antenna Diversity**: Enables runtime RSSI-based antenna switching to adapt to changing RF conditions

This guide explains how to configure and implement antenna diversity for QCC74x boards equipped with an RF switch.

### Hardware Requirements

- QCC74x development board with RF switch capability
- Minimum of two physical antennas
- RF switch for antenna selection
- Required test equipment (for validation):
  - RF shielding boxes
  - Variable and fixed attenuators
  - RF cables and connectors
  - Test access points (APs)

### Implementation Guide

#### 1. Hardware Analysis

Before implementation, gather the following information from your hardware design:

1. Locate the RF switch control GPIO pin(s) in your schematic
2. Review the RF switch datasheet for:
   - Control voltage requirements
   - Switching timing specifications
   - Logic level requirements (active high/low)

#### 2. Software Implementation

##### 2.1 Core HAL Implementation

Create the following functions in your board-specific code (typically in `bsp/board/[your_board]/board.c`):

```c
// Initialize GPIO for RF switch control
static int board_antenna_hal_init(void)
{
    antenna_gpio = qcc74x_device_get_by_name("gpio");
    if (!antenna_gpio) {
        return -1;
    }

    // Configure GPIO with appropriate flags for your RF switch
    qcc74x_gpio_init(antenna_gpio, ANTENNA_GPIO_PIN, 
        GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);
    return 0;
}

// Implement antenna switching logic
static int board_antenna_hal_switch(antenna_id_t antenna)
{
    if (!antenna_gpio) {
        return -1;
    }

    // Adjust logic based on your RF switch control requirements
    if (antenna == ANTENNA_0) {
        qcc74x_gpio_set(antenna_gpio, ANTENNA_GPIO_PIN);
    } else if (antenna == ANTENNA_1) {
        qcc74x_gpio_reset(antenna_gpio, ANTENNA_GPIO_PIN);
    }

    return 0;
}
```

##### 2.2 Register HAL Operations

Define and register the antenna HAL operations structure:

```c
static const antenna_hal_ops_t board_antenna_ops = {
    .init = board_antenna_hal_init,
    .switch_antenna = board_antenna_hal_switch,
    .get_antenna_count = board_antenna_hal_get_count,
    .deinit = board_antenna_hal_deinit,
    .log = board_antenna_log,  // Optional: Set to NULL to disable logging
    .dynamic_div_enabled = false,  // Enable for dynamic diversity
};

int board_antenna_init(void)
{
    return antenna_hal_init(&board_antenna_ops);
}
```

### Feature Configuration

#### Static Antenna Diversity

Static diversity is enabled by default when you implement the antenna HAL operations. The SDK provides:

- `wifi_antenna_scan`: Scans all antennas for each AP to identify optimal antenna selection
- `wifi_antenna_connect <SSID> <PASSWORD>`: Connects to specified network using the best antenna

#### Dynamic Antenna Diversity

To enable runtime antenna switching, set `.dynamic_div_enabled = true` in your antenna_hal_ops_t structure.

### Build and Flash

```bash
# Build the firmware
make CHIP=qcc743 BOARD=qcc743dk

# Flash to device
make flash COMX=<your_com_port>
```

### Validation Testing

#### Static Diversity Test Setup

```
AP1 ─────┐              ┌───── AP2
         │              │
         └──► DUT ◄─────┘
     (Antenna 0)  (Antenna 1)
```

1. Configure test environment:
   - Place APs in separate RF shielding boxes
   - Connect AP1 to antenna 0 path
   - Connect AP2 to antenna 1 path
   
2. Validation steps:
   ```bash
   wifi_antenna_scan  # Scan for available APs
   ```

3. Expected Results:
   - Both APs should be detected in scan results
   - Best antenna in scan results should be the one connected to AP with better RSSI

#### Dynamic Diversity Test Setup

```
                   AP
                   │
        ┌──────────┴──────────┐
        │                     │
   Variable ATT          Fixed ATT
        │                     │
    Antenna 0            Antenna 1
        └──────── DUT ────────┘
```

1. Test procedure:
   - Set variable attenuator for initial stronger path
   - Connect to AP using `wifi_antenna_connect`
   - Gradually increase variable attenuation

2. Expected Results:
   - Initial antenna selection check:
     * DUT should connect to AP using the antenna with variable attenuator (better RSSI path)
   - Antenna switching check:
     * When RSSI drops below -60dBm, DUT should switch to the antenna with fixed attenuator

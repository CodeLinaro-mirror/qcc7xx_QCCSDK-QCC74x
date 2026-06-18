# Bluetooth Low Energy (BLE) Examples

This directory contains comprehensive Bluetooth Low Energy (BLE) application examples for QCC74x series chips, covering various application scenarios from basic BLE communication to complex Mesh networks.

## Example List

### 1. btble_cli - BLE Command Line Interface Example

**Description:**
Provides a comprehensive BLE command line interface for testing and development. This example includes complete BLE protocol stack operations with extensive command support.

**Key Features:**
- BLE controller and host stack initialization
- Rich serial command line interface with BLE CLI commands
- Connection state callback handling
- Auto-restart advertising functionality
- Support for various BLE operations (scan, connect, GATT operations, etc.)

**Use Cases:**
- BLE functionality verification and testing
- Protocol stack development and debugging
- Performance evaluation
- Educational purposes for BLE protocol learning

---

### 2. peripheral - BLE Peripheral Device Example

**Description:**
Demonstrates a complete BLE peripheral device implementation with multiple standard and custom GATT services.

**Key Features:**
- **Custom GATT Service** (ble_tp_svc): Test service with READ, WRITE, NOTIFY, INDICATE operations
- **Battery Service (BAS)**: Configurable battery level reporting
- **Device Information Service (DIS)**: Device identification information
- **Immediate Alert Service (IAS)**: Alert level control
- **OTA Update Service (OAD)**: Over-the-air firmware updates
- Auto advertising and connection management
- Shell commands for battery level control

**Use Cases:**
- IoT sensor devices
- Wearable devices
- Smart home peripherals
- Health monitoring devices
- Any BLE peripheral application

**OTA Update Process:**
```bash
# Generate BLE OTA file
python components/wireless/bluetooth/tools/GenerateOAD.py FW.bin FW.bin.sha256
```
---

### 3. central - BLE Central Device Example

**Description:**
Implements a BLE central device with automatic device discovery, connection, and GATT operations.

**Key Features:**
- Automatic device scanning with name filtering
- Targeted device connection (searches for devices with specified name)
- Service discovery and characteristic enumeration
- Automatic GATT subscription (NOTIFY/INDICATE)
- Periodic data transmission to writable characteristics
- Security pairing support (SMP)
- Connection parameter management

**Use Cases:**
- BLE gateway devices
- Data collectors and aggregators
- Control center devices
- Health monitoring hubs
- Industrial monitoring systems

**Workflow:**
1. Starts scanning for BLE devices
2. Filters devices by configured name
3. Automatically connects to matching devices
4. Performs service discovery
5. Enables notifications on first available characteristic
6. Starts periodic write operations to first writable characteristic

---

### 4. blemesh_cli - BLE Mesh Network Command Line Example

**Description:**
Complete BLE Mesh network implementation supporting both Node and Provisioner modes with rich command interface.

**Key Features:**
- **Dual Mode Operation**: Node and Provisioner modes
- **Mesh Network Management**: Configuration, provisioning, and control
- **Rich CLI Interface**: Comprehensive mesh command set
- **Generic On Off Server Model**: Basic mesh control functionality
- **Application Key Management**: Secure key distribution
- **Node Composition Data**: Device capability discovery
- **Group Control**: Multicast and unicast communication

**Use Cases:**
- Smart lighting systems
- Building automation
- Industrial control networks
- Smart city applications
- Large-scale IoT deployments

**Key Commands:**
- `blemesh_init`: Initialize mesh stack
- `blemesh_pb <mode> <role>`: Provisioning configuration
- `blemesh_pvnr_init`: Initialize provisioner mode
- `blemesh_provision_adv <uuid> <net_idx> <addr> <duration>`: Provision device
- `blemesh_get_comp <net_idx> <addr> <page>`: Get composition data
- `blemesh_app_key_add <net_idx> <addr> <net_key_idx> <app_key_idx>`: Add app key

**Mobile App Workflow:**
1. Add node to network through provisioning
2. Configure application keys and bindings
3. Control devices through on/off commands
4. Set up group subscriptions for multicast control

**Provisioner Mode Configuration:**
To enable Provisioner mode, add the following configuration to the `defconfig` file:
```bash
CONFIG_BT_MESH_PROVISIONER =y
```
This configuration enables the device to act as a mesh network provisioner that can add and configure other mesh nodes.

**How to Use Provisioner Mode:**

**Provisionee Device (Node to be provisioned):**
1. Flash the blemesh_cli example without CONFIG_BT_MESH_PROVISIONER configuration
2. Run the following commands in the serial terminal:
   ```bash
   qcc74x />blemesh_init
   # Wait for initialization to complete
   
   qcc74x />blemesh_pb 2 1
   # Set device as unprovisioned node ready for provisioning
   ```
3. The device will start advertising as an unprovisioned beacon, waiting for provisioner

**Provisioner Device (Device that provisions others):**
1. Flash the blemesh_cli example with CONFIG_BT_MESH_PROVISIONER =y in defconfig
2. Run the following commands in the serial terminal:
   ```bash
   qcc74x />blemesh_init
   # Wait for initialization to complete
   
   qcc74x />blemesh_pb 1 1
   # Set device as provisioner
   
   qcc74x />blemesh_pvnr_init
   # Initialize provisioner mode
   # Output will show local node information:
   # Local node provisioned, net_idx 0x0000 address 0x0001
   # Network key: [network_key_value]
   # Dev key: [dev_key_value]
   # Provisioning completed
   
   qcc74x />blemesh_beacon_listen 0x01
   # Enable unprovisioned beacon listening
   # UUID information of nearby unprovisioned devices will be printed
   ```
3. Provision the unprovisioned node:
   ```bash
   qcc74x />blemesh_provision_adv <uuid> <net_idx> <addr> <duration>
   # Example: blemesh_provision_adv 000a000011112222333318b905de96c2 0 3 0
   # Parameters:
   # - uuid: UUID of the unprovisioned device (from beacon_listen output)
   # - net_idx: Network index (usually 0)
   # - addr: Unicast address to assign to the node
   # - duration: Attention duration in seconds (0 for default)
   ```
4. After successful provisioning, configure the node:
   ```bash
   qcc74x />blemesh_get_comp 0x0000 0x0003 0x00
   # Get composition data of the newly provisioned node
   # Parameters: net_idx, dst_address, page_number
   
   qcc74x />blemesh_app_key_add 0x0000 0x0003 0x0000 0x0000
   # Add application key to the node
   # Parameters: net_idx, dst_address, net_key_idx, app_key_idx
   ```
5. The provisioned node is now ready to receive mesh control commands

---

### 5. blemesh_simple - BLE Mesh Simple Example

**Description:**
Simplified BLE Mesh implementation for rapid prototyping and basic functionality verification.

**Key Features:**
- Automatic mesh network startup on boot
- Simplified command interface
- Basic mesh control functionality
- Group control support
- Minimal configuration required

**Use Cases:**
- Rapid mesh prototyping
- Basic mesh functionality verification
- Educational purposes
- Simple mesh applications

**Usage:**
- Mesh starts automatically after power-on
- Use `gen_cli_send <dst_address>` to send control commands
- Refer to blemesh_cli documentation for advanced configuration

---

### 6. remote_control - BLE Remote Control Example

**Description:**
Implements HOGP (Human Interface Device Profile) GATT service for remote control device simulation.

**Key Features:**
- **HOGP Service Implementation**: Human Interface Device Profile
- **HID Device Simulation**: Remote control functionality
- **Key Handling**: Remote control key press/release events
- **Standard HID Compliance**: Compatible with HID hosts

**Use Cases:**
- Remote control devices
- Game controllers
- Human interface devices
- Multimedia control devices
- Presentation remotes

---

### 7. btble_pts - BLE PTS Test Example

**Description:**
Specialized example for Bluetooth certification testing using PTS (Profile Tuning Suite). This example is specifically designed to work with Bluetooth SIG's PTS testing tool for protocol conformance and interoperability certification.

**Key Features:**
- **PTS Test Support**: Optimized for Bluetooth SIG certification testing
- **Conformance Testing**: Protocol compliance verification
- **Interoperability Testing**: Cross-vendor compatibility
- **PTS-Specific Implementations**: Tailored behavior for test case requirements
- **Certification-Ready**: Pre-configured for standard Bluetooth test profiles

**Use Cases:**
- Bluetooth SIG certification testing
- Protocol conformance verification
- Interoperability testing with other Bluetooth devices
- Quality assurance for commercial products
- Regulatory compliance testing

**Difference from btble_cli:**
- **btble_cli**: General-purpose BLE testing and development tool with rich CLI commands for manual testing and debugging
- **btble_pts**: Specialized certification tool with behavior optimized for automated PTS test cases, may have limited manual CLI functionality
- **Target Users**: btble_cli is for developers and testers; btble_pts is for certification engineers and compliance testing
- **Testing Approach**: btble_cli supports interactive testing; btble_pts supports automated test case execution
- **Compliance Focus**: btble_pts ensures strict adherence to Bluetooth specification requirements

**PTS Testing Setup:**
1. Connect QCC74x device to PTS test system via UART
2. Configure PTS test parameters according to test case requirements
3. Run specific test cases from PTS software
4. Monitor test results and logs for compliance verification

---

### 8. btblecontroller_test - BLE Controller Test Example

**Description:**
Low-level BLE controller functionality testing for hardware and protocol verification.

**Key Features:**
- **Controller Testing**: Low-level protocol stack verification
- **Hardware Validation**: RF and baseband functionality
- **Performance Testing**: Throughput and latency measurements
- **Regression Testing**: Continuous integration support

**Use Cases:**
- Controller functionality verification
- Low-level protocol testing
- Hardware functionality testing
- Performance benchmarking
- Development validation

## Related Documentation

- **BLE Protocol Stack Documentation**: [ble stack](../../docs/en/api_reference/ble.rst)
- **Mesh Protocol Documentation**: [mesh stack](../../docs/en/api_reference/ble_mesh.rst)
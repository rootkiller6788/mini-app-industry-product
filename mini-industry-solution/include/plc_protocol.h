/**
 * mini-industry-solution: plc_protocol.h
 *
 * Industrial Protocol Stack - Modbus RTU/ASCII/TCP & IEC 61131-3
 * ================================================================
 *
 * L1 Definitions: PLCFrame, ModbusFrame, ModbusException, OPCUANode
 * L2 Core Concepts: Master-slave protocol, Polling cycle, Exception codes
 * L3 Engineering: Frame serialization/deserialization pipeline
 * L4 Standards: Modbus (Modicon 1979), IEC 61131-3 PLC languages,
 *               IEC 62541 OPC UA, IEEE 802.1 TSN
 * L5 Algorithms: CRC-16 (Modbus), LRC, Frame parsing
 * L6 Canonical Problems: Modbus master/slave simulation
 * L7 Applications: Industrial IoT gateway, OPC UA data model
 * L8 Advanced: Safety PLC (SIL 1-4), TSN scheduling
 * L9 Industry Frontiers: Time-Sensitive Networking, APL
 */

#ifndef PLC_PROTOCOL_H
#define PLC_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define MODBUS_MAX_FRAME_SIZE 256
#define MODBUS_BROADCAST_ADDR 0

typedef enum {
    MODBUS_RTU,
    MODBUS_ASCII,
    MODBUS_TCP
} ModbusMode;

typedef enum {
    MODBUS_FC_READ_COILS          = 0x01,
    MODBUS_FC_READ_DISCRETE_INPUTS = 0x02,
    MODBUS_FC_READ_HOLDING_REGS   = 0x03,
    MODBUS_FC_READ_INPUT_REGS     = 0x04,
    MODBUS_FC_WRITE_SINGLE_COIL   = 0x05,
    MODBUS_FC_WRITE_SINGLE_REG    = 0x06,
    MODBUS_FC_WRITE_MULTI_COILS   = 0x0F,
    MODBUS_FC_WRITE_MULTI_REGS    = 0x10,
    MODBUS_FC_REPORT_SLAVE_ID     = 0x11
} ModbusFunctionCode;

typedef enum {
    MODBUS_EXC_NONE              = 0x00,
    MODBUS_EXC_ILLEGAL_FUNCTION  = 0x01,
    MODBUS_EXC_ILLEGAL_ADDRESS   = 0x02,
    MODBUS_EXC_ILLEGAL_VALUE     = 0x03,
    MODBUS_EXC_SLAVE_FAILURE     = 0x04,
    MODBUS_EXC_ACKNOWLEDGE       = 0x05,
    MODBUS_EXC_SLAVE_BUSY        = 0x06,
    MODBUS_EXC_MEMORY_PARITY     = 0x08,
    MODBUS_EXC_GATEWAY_PATH      = 0x0A,
    MODBUS_EXC_GATEWAY_FAILED    = 0x0B
} ModbusExceptionCode;

typedef struct {
    uint8_t slave_addr;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t quantity;
    uint8_t  data[MODBUS_MAX_FRAME_SIZE - 6];
    uint8_t  data_len;
    uint16_t crc;
    ModbusExceptionCode exception;
} ModbusFrame;

typedef struct {
    uint16_t coils[4096];
    uint16_t discrete_inputs[4096];
    uint16_t holding_registers[4096];
    uint16_t input_registers[4096];
    uint8_t  slave_id;
    int      slave_id_valid;
} ModbusSlaveDevice;

typedef struct {
    char name[64];
    uint16_t start_addr;
    uint16_t length;
    double   scale;
    double   offset;
    char     unit[16];
    int      is_signed;
} ModbusRegisterMap;

typedef enum { OPCUA_INT32, OPCUA_FLOAT, OPCUA_STRING, OPCUA_BOOL,
               OPCUA_DATETIME, OPCUA_BYTESTRING } OPCUADataType;

typedef struct {
    char node_id[64];
    char display_name[64];
    OPCUADataType data_type;
    double numeric_value;
    char string_value[128];
    int bool_value;
    int64_t timestamp_ms;
    int quality_good;
} OPCUANode;

typedef enum { SIL_1, SIL_2, SIL_3, SIL_4 } SafetyIntegrityLevel;

uint16_t modbus_crc16(const uint8_t *data, size_t len);
uint8_t  modbus_lrc(const uint8_t *data, size_t len);

int  modbus_rtu_frame_build(ModbusFrame *frame, uint8_t *out_buf,
                             size_t *out_len);
int  modbus_rtu_frame_parse(const uint8_t *buf, size_t len,
                             ModbusFrame *out_frame);
int  modbus_ascii_frame_build(const ModbusFrame *frame,
                               uint8_t *out_buf, size_t *out_len);
int  modbus_ascii_frame_parse(const uint8_t *buf, size_t len,
                               ModbusFrame *out_frame);
int  modbus_tcp_frame_build(const ModbusFrame *frame, uint16_t transaction_id,
                             uint8_t *out_buf, size_t *out_len);
int  modbus_tcp_frame_parse(const uint8_t *buf, size_t len,
                             ModbusFrame *out_frame, uint16_t *out_tid);

void modbus_slave_init(ModbusSlaveDevice *slave, uint8_t slave_id);
int  modbus_slave_process(ModbusSlaveDevice *slave,
                           const ModbusFrame *request,
                           ModbusFrame *response);
int  modbus_slave_read_coil(const ModbusSlaveDevice *slave,
                             uint16_t addr);
int  modbus_slave_write_coil(ModbusSlaveDevice *slave,
                              uint16_t addr, int value);
int  modbus_slave_read_register(const ModbusSlaveDevice *slave,
                                 uint16_t addr, uint16_t *value);
int  modbus_slave_write_register(ModbusSlaveDevice *slave,
                                  uint16_t addr, uint16_t value);

int  modbus_register_map_lookup(const ModbusRegisterMap *map,
                                 int map_count, const char *name,
                                 uint16_t *out_addr, double *out_scale,
                                 double *out_offset);

void opcua_node_init(OPCUANode *node, const char *node_id,
                     const char *display_name, OPCUADataType type);
void opcua_node_set_int32(OPCUANode *node, int32_t value);
void opcua_node_set_float(OPCUANode *node, double value);
void opcua_node_set_bool(OPCUANode *node, int value);
void opcua_node_set_string(OPCUANode *node, const char *value);

double         sil_pfd_avg(SafetyIntegrityLevel sil);
double         sil_rrf(SafetyIntegrityLevel sil);
const char    *sil_name(SafetyIntegrityLevel sil);

#endif /* PLC_PROTOCOL_H */

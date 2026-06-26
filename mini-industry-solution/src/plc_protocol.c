/**
 * mini-industry-solution: plc_protocol.c
 * Modbus RTU/ASCII/TCP implementation, OPC UA model, SIL analysis
 * Ref: Modicon Modbus Protocol Reference Guide (PI-MBUS-300 Rev. J)
 *      IEC 61131-3, IEC 62541 OPC UA, IEC 61508
 */
#include "plc_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* === CRC-16 Modbus (L5 Algorithm) === */
/*
 * CRC-16 computation per Modbus specification.
 * Polynomial: x^16 + x^15 + x^2 + 1 (0xA001 reflected)
 * Initial value: 0xFFFF
 *
 * This is the standard CRC used in Modbus RTU frames.
 * Ref: "Modbus over Serial Line Specification V1.02"
 */
uint16_t modbus_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/*
 * LRC (Longitudinal Redundancy Check) for Modbus ASCII mode.
 * Sum of all bytes, two's complement.
 * Ref: Modbus ASCII framing specification.
 */
uint8_t modbus_lrc(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return (uint8_t)(-(int8_t)sum);
}

/* === Modbus RTU Frame Build/Parse (L3-L5) === */

int modbus_rtu_frame_build(ModbusFrame *frame, uint8_t *out_buf,
                            size_t *out_len) {
    if (!frame || !out_buf || !out_len) return -1;
    uint8_t *p = out_buf;
    *p++ = frame->slave_addr;
    *p++ = frame->function_code;
    *p++ = (uint8_t)(frame->start_address >> 8);
    *p++ = (uint8_t)(frame->start_address & 0xFF);
    *p++ = (uint8_t)(frame->quantity >> 8);
    *p++ = (uint8_t)(frame->quantity & 0xFF);
    if (frame->data_len > 0 && frame->data_len <= MODBUS_MAX_FRAME_SIZE - 6) {
        memcpy(p, frame->data, frame->data_len);
        p += frame->data_len;
    }
    size_t data_len = (size_t)(p - out_buf);
    frame->crc = modbus_crc16(out_buf, data_len);
    *p++ = (uint8_t)(frame->crc & 0xFF);
    *p++ = (uint8_t)(frame->crc >> 8);
    *out_len = (size_t)(p - out_buf);
    return 0;
}

int modbus_rtu_frame_parse(const uint8_t *buf, size_t len,
                            ModbusFrame *out_frame) {
    if (!buf || !out_frame || len < 4) return -1;
    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->slave_addr = buf[0];
    out_frame->function_code = buf[1];
    if (out_frame->function_code & 0x80) {
        out_frame->function_code &= 0x7F;
        if (len >= 3) out_frame->exception = (ModbusExceptionCode)buf[2];
        out_frame->data_len = 1;
        out_frame->data[0] = buf[2];
        return 0;
    }
    if (len >= 4) {
        out_frame->start_address = ((uint16_t)buf[2] << 8) | buf[3];
    }
    if (len >= 6) {
        out_frame->quantity = ((uint16_t)buf[4] << 8) | buf[5];
    }
    if (len > 6) {
        size_t data_len = len - 6;
        if (data_len > MODBUS_MAX_FRAME_SIZE - 6)
            data_len = MODBUS_MAX_FRAME_SIZE - 6;
        memcpy(out_frame->data, buf + 6, data_len);
        out_frame->data_len = (uint8_t)data_len;
    }
    uint16_t crc_calc = modbus_crc16(buf, len);
    out_frame->crc = crc_calc;
    out_frame->exception = MODBUS_EXC_NONE;
    return 0;
}

/* === Modbus ASCII Frame (L5) === */

static uint8_t hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

static char nibble_to_hex(uint8_t n) {
    return (n < 10) ? (char)('0' + n) : (char)('A' + n - 10);
}

int modbus_ascii_frame_build(const ModbusFrame *frame,
                               uint8_t *out_buf, size_t *out_len) {
    if (!frame || !out_buf || !out_len) return -1;
    uint8_t temp[MODBUS_MAX_FRAME_SIZE];
    temp[0] = frame->slave_addr;
    temp[1] = frame->function_code;
    temp[2] = (uint8_t)(frame->start_address >> 8);
    temp[3] = (uint8_t)(frame->start_address & 0xFF);
    temp[4] = (uint8_t)(frame->quantity >> 8);
    temp[5] = (uint8_t)(frame->quantity & 0xFF);
    size_t tlen = 6;
    if (frame->data_len > 0) {
        memcpy(temp + tlen, frame->data, frame->data_len);
        tlen += frame->data_len;
    }
    uint8_t lrc = modbus_lrc(temp, tlen);
    uint8_t *p = out_buf;
    *p++ = ':';
    for (size_t i = 0; i < tlen; i++) {
        *p++ = nibble_to_hex(temp[i] >> 4);
        *p++ = nibble_to_hex(temp[i] & 0x0F);
    }
    *p++ = nibble_to_hex(lrc >> 4);
    *p++ = nibble_to_hex(lrc & 0x0F);
    *p++ = '\r'; *p++ = '\n';
    *out_len = (size_t)(p - out_buf);
    return 0;
}

int modbus_ascii_frame_parse(const uint8_t *buf, size_t len,
                               ModbusFrame *out_frame) {
    if (!buf || !out_frame || len < 9 || buf[0] != ':') return -1;
    memset(out_frame, 0, sizeof(*out_frame));
    size_t byte_count = (len - 5) / 2;
    uint8_t raw[MODBUS_MAX_FRAME_SIZE];
    for (size_t i = 0; i < byte_count && i < MODBUS_MAX_FRAME_SIZE; i++) {
        raw[i] = (uint8_t)((hex_to_nibble((char)buf[1 + 2*i]) << 4)
                | hex_to_nibble((char)buf[2 + 2*i]));
    }
    out_frame->slave_addr = raw[0];
    out_frame->function_code = raw[1];
    if (byte_count >= 4) {
        out_frame->start_address = ((uint16_t)raw[2] << 8) | raw[3];
        out_frame->quantity = ((uint16_t)raw[4] << 8) | raw[5];
    }
    if (byte_count > 6) {
        out_frame->data_len = (uint8_t)(byte_count - 6);
        memcpy(out_frame->data, raw + 6, byte_count - 6);
    }
    out_frame->exception = MODBUS_EXC_NONE;
    return 0;
}

/* === Modbus TCP Frame (L5-L7) === */

int modbus_tcp_frame_build(const ModbusFrame *frame, uint16_t transaction_id,
                            uint8_t *out_buf, size_t *out_len) {
    if (!frame || !out_buf || !out_len) return -1;
    uint8_t *p = out_buf;
    *p++ = (uint8_t)(transaction_id >> 8);
    *p++ = (uint8_t)(transaction_id & 0xFF);
    *p++ = 0x00; *p++ = 0x00;
    uint16_t remaining = 2 + frame->data_len + 4;
    *p++ = (uint8_t)(remaining >> 8);
    *p++ = (uint8_t)(remaining & 0xFF);
    *p++ = frame->slave_addr;
    *p++ = frame->function_code;
    *p++ = (uint8_t)(frame->start_address >> 8);
    *p++ = (uint8_t)(frame->start_address & 0xFF);
    *p++ = (uint8_t)(frame->quantity >> 8);
    *p++ = (uint8_t)(frame->quantity & 0xFF);
    if (frame->data_len > 0) {
        memcpy(p, frame->data, frame->data_len);
        p += frame->data_len;
    }
    *out_len = (size_t)(p - out_buf);
    return 0;
}

int modbus_tcp_frame_parse(const uint8_t *buf, size_t len,
                            ModbusFrame *out_frame, uint16_t *out_tid) {
    if (!buf || !out_frame || len < 8) return -1;
    memset(out_frame, 0, sizeof(*out_frame));
    if (out_tid) *out_tid = ((uint16_t)buf[0] << 8) | buf[1];
    out_frame->slave_addr = buf[6];
    out_frame->function_code = buf[7];
    if (len >= 10) {
        out_frame->start_address = ((uint16_t)buf[8] << 8) | buf[9];
        out_frame->quantity = ((uint16_t)buf[10] << 8) | buf[11];
    }
    if (len > 12) {
        out_frame->data_len = (uint8_t)(len - 12);
        if (out_frame->data_len > MODBUS_MAX_FRAME_SIZE - 6)
            out_frame->data_len = MODBUS_MAX_FRAME_SIZE - 6;
        memcpy(out_frame->data, buf + 12, out_frame->data_len);
    }
    out_frame->exception = MODBUS_EXC_NONE;
    return 0;
}

/* === Modbus Slave Device (L6) === */

void modbus_slave_init(ModbusSlaveDevice *slave, uint8_t slave_id) {
    if (!slave) return;
    memset(slave, 0, sizeof(*slave));
    slave->slave_id = slave_id;
    slave->slave_id_valid = 1;
}

int modbus_slave_process(ModbusSlaveDevice *slave,
                          const ModbusFrame *request,
                          ModbusFrame *response) {
    if (!slave || !request || !response) return -1;
    memset(response, 0, sizeof(*response));
    response->slave_addr = request->slave_addr;
    response->function_code = request->function_code;

    if (request->slave_addr != slave->slave_id
        && request->slave_addr != MODBUS_BROADCAST_ADDR) {
        response->function_code |= 0x80;
        response->exception = MODBUS_EXC_GATEWAY_PATH;
        response->data[0] = MODBUS_EXC_GATEWAY_PATH;
        response->data_len = 1;
        return -1;
    }

    switch (request->function_code) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS: {
        if (request->quantity < 1 || request->quantity > 2000) {
            response->function_code |= 0x80;
            response->exception = MODBUS_EXC_ILLEGAL_VALUE;
            return -1;
        }
        uint8_t byte_count = (uint8_t)((request->quantity + 7) / 8);
        response->data[0] = byte_count;
        response->data_len = byte_count + 1;
        return 0;
    }
    case MODBUS_FC_READ_HOLDING_REGS:
    case MODBUS_FC_READ_INPUT_REGS: {
        if (request->quantity < 1 || request->quantity > 125) {
            response->function_code |= 0x80;
            response->exception = MODBUS_EXC_ILLEGAL_VALUE;
            return -1;
        }
        uint8_t byte_count = (uint8_t)(request->quantity * 2);
        response->data[0] = byte_count;
        for (int i = 0; i < request->quantity && i < 125; i++) {
            uint16_t addr = request->start_address + i;
            if (addr < 4096) {
                uint16_t val = (request->function_code == MODBUS_FC_READ_HOLDING_REGS)
                    ? slave->holding_registers[addr]
                    : slave->input_registers[addr];
                response->data[1 + 2*i] = (uint8_t)(val >> 8);
                response->data[2 + 2*i] = (uint8_t)(val & 0xFF);
            }
        }
        response->data_len = byte_count + 1;
        return 0;
    }
    case MODBUS_FC_WRITE_SINGLE_COIL: {
        uint16_t addr = request->start_address;
        if (addr >= 4096) {
            response->function_code |= 0x80;
            response->exception = MODBUS_EXC_ILLEGAL_ADDRESS;
            return -1;
        }
        int val = (request->quantity == 0xFF00) ? 1 : 0;
        slave->coils[addr] = val;
        memcpy(response, request, sizeof(ModbusFrame));
        return 0;
    }
    case MODBUS_FC_WRITE_SINGLE_REG: {
        uint16_t addr = request->start_address;
        if (addr >= 4096) {
            response->function_code |= 0x80;
            response->exception = MODBUS_EXC_ILLEGAL_ADDRESS;
            return -1;
        }
        slave->holding_registers[addr] = request->quantity;
        memcpy(response, request, sizeof(ModbusFrame));
        return 0;
    }
    default:
        response->function_code |= 0x80;
        response->exception = MODBUS_EXC_ILLEGAL_FUNCTION;
        return -1;
    }
}

int modbus_slave_read_coil(const ModbusSlaveDevice *slave, uint16_t addr) {
    if (!slave || addr >= 4096) return -1;
    return slave->coils[addr];
}

int modbus_slave_write_coil(ModbusSlaveDevice *slave, uint16_t addr,
                             int value) {
    if (!slave || addr >= 4096) return -1;
    slave->coils[addr] = value ? 1 : 0;
    return 0;
}

int modbus_slave_read_register(const ModbusSlaveDevice *slave,
                                uint16_t addr, uint16_t *value) {
    if (!slave || !value || addr >= 4096) return -1;
    *value = slave->holding_registers[addr];
    return 0;
}

int modbus_slave_write_register(ModbusSlaveDevice *slave,
                                 uint16_t addr, uint16_t value) {
    if (!slave || addr >= 4096) return -1;
    slave->holding_registers[addr] = value;
    return 0;
}

int modbus_register_map_lookup(const ModbusRegisterMap *map,
                                int map_count, const char *name,
                                uint16_t *out_addr, double *out_scale,
                                double *out_offset) {
    if (!map || !name || map_count <= 0) return -1;
    for (int i = 0; i < map_count; i++) {
        if (strcmp(map[i].name, name) == 0) {
            if (out_addr) *out_addr = map[i].start_addr;
            if (out_scale) *out_scale = map[i].scale;
            if (out_offset) *out_offset = map[i].offset;
            return 0;
        }
    }
    return -1;
}

/* === OPC UA Node Model (L7) === */

void opcua_node_init(OPCUANode *node, const char *node_id,
                     const char *display_name, OPCUADataType type) {
    if (!node) return;
    memset(node, 0, sizeof(*node));
    if (node_id) {
        strncpy(node->node_id, node_id, sizeof(node->node_id) - 1);
        node->node_id[sizeof(node->node_id) - 1] = '\0';
    }
    if (display_name) {
        strncpy(node->display_name, display_name,
                sizeof(node->display_name) - 1);
        node->display_name[sizeof(node->display_name) - 1] = '\0';
    }
    node->data_type = type;
    node->quality_good = 1;
}

void opcua_node_set_int32(OPCUANode *node, int32_t value) {
    if (!node) return;
    node->numeric_value = (double)value;
    node->data_type = OPCUA_INT32;
}

void opcua_node_set_float(OPCUANode *node, double value) {
    if (!node) return;
    node->numeric_value = value;
    node->data_type = OPCUA_FLOAT;
}

void opcua_node_set_bool(OPCUANode *node, int value) {
    if (!node) return;
    node->bool_value = value ? 1 : 0;
    node->data_type = OPCUA_BOOL;
}

void opcua_node_set_string(OPCUANode *node, const char *value) {
    if (!node || !value) return;
    strncpy(node->string_value, value, sizeof(node->string_value) - 1);
    node->string_value[sizeof(node->string_value) - 1] = '\0';
    node->data_type = OPCUA_STRING;
}

/* === Safety Integrity Level (L8) === */
/*
 * Per IEC 61508:
 * SIL 1: PFDavg < 10^-1, RRF >= 10
 * SIL 2: PFDavg < 10^-2, RRF >= 100
 * SIL 3: PFDavg < 10^-3, RRF >= 1000
 * SIL 4: PFDavg < 10^-4, RRF >= 10000
 */

double sil_pfd_avg(SafetyIntegrityLevel sil) {
    switch (sil) {
    case SIL_1: return 1e-1;
    case SIL_2: return 1e-2;
    case SIL_3: return 1e-3;
    case SIL_4: return 1e-4;
    default:    return 1.0;
    }
}

double sil_rrf(SafetyIntegrityLevel sil) {
    switch (sil) {
    case SIL_1: return 10.0;
    case SIL_2: return 100.0;
    case SIL_3: return 1000.0;
    case SIL_4: return 10000.0;
    default:    return 1.0;
    }
}

const char *sil_name(SafetyIntegrityLevel sil) {
    switch (sil) {
    case SIL_1: return "SIL 1";
    case SIL_2: return "SIL 2";
    case SIL_3: return "SIL 3";
    case SIL_4: return "SIL 4";
    default:    return "No SIL";
    }
}

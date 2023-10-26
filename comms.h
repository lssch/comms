//
// Created by Lars Schwarz on 04.10.2023.
//

#ifndef COMMS_H
#define COMMS_H

#include <cstdint>
#include "types/types.h"

#define CURR 0
#define OLD 1
#define SYNC 0xAA

typedef enum _comms_access_type_e {
  COMMS_ACCESS_REQUEST_IGNORE = 0,
  COMMS_ACCESS_REQUEST_SET,
  COMMS_ACCESS_REQUEST_GET,
} __attribute__((packed)) comms_access_type_e;

typedef struct _comms_access_request_t {
  comms_access_type_e state;
  comms_access_type_e sensor;
  comms_access_type_e data;
  comms_access_type_e parameter;
  comms_access_type_e request;
} __attribute__((packed)) comms_access_request_t;

typedef union _comms_packet_header_t {
  struct {
    uint8_t sync;
    uint16_t access_request;
    uint16_t package_length;
    uint8_t checksum;
  } __attribute__((packed));
  uint8_t buffer[6];
} __attribute__((packed)) comms_packet_header_t;

typedef union _comms_packet_t {
  struct {
    comms_packet_header_t header;
    robocar_data_t data;
  } __attribute__((packed)) ;
  uint8_t buffer[sizeof(comms_packet_header_t) + sizeof(robocar_data_t)];
} comms_packet_t;

const uint16_t MAX_DATA_LENGTH = sizeof(robocar_data_t);
const uint16_t MAX_PACKET_LENGTH = sizeof(comms_packet_header_t) + sizeof(robocar_data_t);

comms_packet_t* rx_packet;
comms_packet_t* tx_packet;
robocar_data_t* data;

class CommsMaster {
public:
  CommsMaster(comms_packet_t* prx_data, comms_packet_t* ptx_data, robocar_data_t* pdata_);
  uint8_t transmit();
  uint8_t receive();
private:
  uint8_t checksum(const uint8_t *data_, uint16_t length);
  uint8_t bitMask(uint8_t lower, uint8_t upper);

  comms_packet_t* prx_packet;
  comms_packet_t* ptx_packet;
  robocar_data_t* pdata;
  _comms_access_request_t last_access_request;
  _comms_access_request_t current_access_request;
};

class CommsSlave {
public:
  CommsSlave(comms_packet_t* prx_data, comms_packet_t* ptx_data, robocar_data_t* pdata_);
  uint8_t transmit();
  uint8_t receive();
private:
  uint8_t checksum(const uint8_t *data_, uint16_t length);
  uint8_t bitMask(uint8_t lower, uint8_t upper);

  comms_packet_t* prx_packet;
  comms_packet_t* ptx_packet;
  robocar_data_t* pdata;
};

#endif //COMMS_H

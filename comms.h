//
// Created by Lars Schwarz on 04.10.2023.
//

#ifndef COMMS_H
#define COMMS_H

#include <cstdint>
#include "../types/types.h"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define SYNC 0xAA

class Comms {
public:
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
      uint16_t access_type;
      uint16_t request_data_length;
      uint16_t response_data_length;
      uint8_t crc;
    } __attribute__((packed));
    uint8_t buffer[8];
  } __attribute__((packed)) comms_packet_header_t;

  typedef union _comms_packet_t {
    struct {
      comms_packet_header_t header;
      robocar_data_t data;
    } __attribute__((packed)) ;
    uint8_t buffer[sizeof(comms_packet_header_t) + sizeof(robocar_data_t)];
  } comms_packet_t;

  Comms(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data);

protected:
  inline uint8_t crc(const uint8_t *data_, uint16_t length);

  comms_packet_t* rx_packet;
  comms_packet_t* tx_packet;
  robocar_data_t* data;
};

class CommsMaster: public Comms {
public:
  CommsMaster(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data)
  : Comms(ptr_rx_data, ptr_tx_data, ptr_data) {};
  uint8_t exchange(comms_access_request_t access_request);

  //comms_access_request_t access_type;
private:
  comms_access_request_t access_request_old;
  comms_packet_header_t header_old;
};

class CommsSlave: public Comms {
public:
  CommsSlave(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data)
  : Comms(ptr_rx_data, ptr_tx_data, ptr_data) {};
  uint8_t response();
};

#endif //COMMS_H

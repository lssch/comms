//
// Created by Lars Schwarz on 04.10.2023.
//

#include "comms.h"
#include <iostream>
#include <bitset>
#include <cstring>

inline uint8_t Comms::Comms::crc(const uint8_t *bytes, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum += bytes[i];
    //std::cout << "Index: " << +i << " current sum value: " << +sum << " with byte value: " << +bytes[i] << std::endl;
  }
  return sum;
}

uint8_t Comms::Comms::exchange() {
  return 0;
}

uint8_t Comms::CommsMaster::exchange(AccessRequest access_request) {
#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "MASTER" << std::endl;
#endif
  // Generate a new request header based on the current request
// TODO: WTF why? It works on ARM Platform but not on AVR! Same implementation on both devices
#if TARGET_PLATFORM == PLATFORM_ARM || TARGET_PLATFORM == PLATFORM_STM
  comms_packet_header_t header = {
          .sync = SYNC,
          .access_type = static_cast<uint16_t>((static_cast<uint8_t>(access_request.request) << 8) |
                                               (static_cast<uint8_t>(access_request.parameter) << 6) |
                                               (static_cast<uint8_t>(access_request.data) << 4) |
                                               (static_cast<uint8_t>(access_request.sensor) << 2) |
                                               (static_cast<uint8_t>(access_request.state) << 0)),
          .crc = 0
  };
#endif
#if TARGET_PLATFORM == PLATFORM_ESP
  comms_packet_header_t header;
  header.sync = SYNC;
  header.access_type = static_cast<uint16_t>((static_cast<uint8_t>(access_request.request) << 8) |
                                       (static_cast<uint8_t>(access_request.parameter) << 6) |
                                       (static_cast<uint8_t>(access_request.data) << 4) |
                                       (static_cast<uint8_t>(access_request.sensor) << 2) |
                                       (static_cast<uint8_t>(access_request.state) << 0));
  header.crc = 0;
#endif

  // Copy current values to the tx buffer based on the current request and calculate the corresponding excange package length.
  uint16_t tx_index{0};
  if (access_request.state == AccessRequestTypes::SET) {
    memcpy(&tx_packet->data.buffer[tx_index], &data->state, sizeof(State::State));
    tx_index += sizeof(State::State);
  }
  if (access_request.sensor == AccessRequestTypes::SET) {
    memcpy(&tx_packet->data.buffer[tx_index], &data->sensor, sizeof(Sensor::Sensor));
    tx_index += sizeof(Sensor::Sensor);
  }
  if (access_request.data == AccessRequestTypes::SET) {
    memcpy(&tx_packet->data.buffer[tx_index], &data->data, sizeof(Data::Data));
    tx_index += sizeof(Data::Data);
  }
  if (access_request.parameter == AccessRequestTypes::SET) {
    memcpy(&tx_packet->data.buffer[tx_index], &data->parameter, sizeof(Parameter::Parameter));
    tx_index += sizeof(Parameter::Parameter);
  }
  if (access_request.request == AccessRequestTypes::SET) {
    memcpy(&tx_packet->data.buffer[tx_index], &data->request, sizeof(Request::Request));
    tx_index += sizeof(Request::Request);
  }

  for (int i = tx_index; i < sizeof(robocar_data_t); ++i)
    tx_packet->data.buffer[i] = 0;

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, sizeof(robocar_data_t));
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "tx-packet: " << RED;
  for (uint8_t byte : tx_packet->header.buffer) std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint8_t byte: tx_packet->data.buffer) std::cout << +byte << " ";
  std::cout<< RESET << std::endl;
#endif

#if TARGET_PLATFORM == PLATFORM_ESP
    spi->queue(tx_packet->buffer, rx_packet->buffer, sizeof(comms_packet_t));
    spi->yield();
#endif

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "rx-packet: " << RED;
  for (uint8_t byte : rx_packet->header.buffer) std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint8_t byte: rx_packet->data.buffer) std::cout << +byte << " ";
  std::cout << RESET << std::endl;
#endif

  // Validate the received package
  uint8_t calculated_checksum = crc(rx_packet->data.buffer, sizeof(robocar_data_t));
  if (rx_packet->header.sync == SYNC && rx_packet->header.crc == calculated_checksum) {
    AccessRequest access_response = {
            .state = static_cast<AccessRequestTypes>(rx_packet->header.access_type & 0b00000000000011),
            .sensor = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000001100) >> 2),
            .data = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000110000) >> 4),
            .parameter = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000011000000) >> 6),
            .request = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000001100000000) >> 8),
    };

    uint16_t rx_index = 0;
    // Coppy the valid data witch was generated by the access request from the last cycle into the local ram
    if (access_response.state == AccessRequestTypes::GET) {
      memcpy(&data->state, &rx_packet->data.buffer[rx_index], sizeof(State::State));
      rx_index += sizeof(State::State);
    }
    if (access_response.sensor == AccessRequestTypes::GET) {
      memcpy(&data->sensor, &rx_packet->data.buffer[rx_index], sizeof(Sensor::Sensor));
      rx_index += sizeof(Sensor::Sensor);
    }
    if (access_response.data == AccessRequestTypes::GET) {
      memcpy(&data->data, &rx_packet->data.buffer[rx_index], sizeof(Data::Data));
      rx_index += sizeof(Data::Data);
    }
    if (access_response.parameter == AccessRequestTypes::GET) {
      memcpy(&data->parameter, &rx_packet->data.buffer[rx_index], sizeof(Parameter::Parameter));
      rx_index += sizeof(Parameter::Parameter);
    }
    if (access_response.request == AccessRequestTypes::GET) {
      memcpy(&data->request, &rx_packet->data.buffer[rx_index], sizeof(Request::Request));
      rx_index += sizeof(Request::Request);
    }
  } else {
    std::cout <<"Dropping curren package...";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC is invalid";
    if (rx_packet->header.crc != calculated_checksum)
      std::cout << " Checksum is wrong got: " << +calculated_checksum << " expected: " << +rx_packet->header.crc;
    std::cout << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

uint8_t Comms::CommsSlave::exchange() {
#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "SLAVE" << std::endl;
  std::cout << "rx-packet: " << RED;
  for (uint8_t byte : rx_packet->header.buffer) std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint8_t byte: rx_packet->data.buffer) std::cout << +byte << " ";
  std::cout << RESET << std::endl;
#endif
  // Build the current access type structure
  AccessRequest access_request = {
          .state = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b00000000000011)),
          .sensor = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000001100) >> 2),
          .data = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000110000) >> 4),
          .parameter = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000011000000) >> 6),
          .request = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000001100000000) >> 8),
  };

  // Validate the incoming data and excange accordingly
  uint8_t calculated_checksum = crc(rx_packet->data.buffer, sizeof(robocar_data_t));
  if (rx_packet->header.sync != SYNC || rx_packet->header.crc != calculated_checksum) {
    std::cout <<"Dropping curren package...";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC is invalid";
    if (rx_packet->header.crc != calculated_checksum)
      std::cout << " Checksum is wrong got: " << +calculated_checksum << " expected: " << +rx_packet->header.crc;
    std::cout << std::endl;
    return EXIT_FAILURE;
  }

  // Generate a excange header
  // TODO: WTF why? It works on ARM Platform but not on AVR
#if TARGET_PLATFORM == PLATFORM_ARM || TARGET_PLATFORM == PLATFORM_STM
  comms_packet_header_t header = {
          .sync = SYNC,
          .access_type = static_cast<uint16_t>((static_cast<uint8_t>(access_request.request) << 8) |
                                               (static_cast<uint8_t>(access_request.parameter) << 6) |
                                               (static_cast<uint8_t>(access_request.data) << 4) |
                                               (static_cast<uint8_t>(access_request.sensor) << 2) |
                                               (static_cast<uint8_t>(access_request.state) << 0)),
          .crc = 0
  };
#elif TARGET_PLATFORM == PLATFORM_ESP
  comms_packet_header_t header;
  header.sync = SYNC;
  header.access_type = static_cast<uint16_t>((static_cast<uint8_t>(access_request.request) << 8) |
                                             (static_cast<uint8_t>(access_request.parameter) << 6) |
                                             (static_cast<uint8_t>(access_request.data) << 4) |
                                             (static_cast<uint8_t>(access_request.sensor) << 2) |
                                             (static_cast<uint8_t>(access_request.state) << 0)),
  header.crc = 0;
#endif

  uint16_t tx_index{0}, rx_index{0};
  switch (access_request.state) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->state, sizeof(State::State));
      tx_index += sizeof(State::State);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->state, &rx_packet->data.buffer[rx_index], sizeof(State::State));
      rx_index += sizeof(State::State);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.sensor) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->sensor, sizeof(Sensor::Sensor));
      tx_index += sizeof(Sensor::Sensor);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->sensor, &rx_packet->data.buffer[rx_index], sizeof(Sensor::Sensor));
      rx_index += sizeof(Sensor::Sensor);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.data) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->data, sizeof(Data::Data));
      tx_index += sizeof(Data::Data);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->data, &rx_packet->data.buffer[rx_index], sizeof(Data::Data));
      rx_index += sizeof(Data::Data);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.parameter) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->parameter, sizeof(Parameter::Parameter));
      tx_index += sizeof(Parameter::Parameter);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->parameter, &rx_packet->data.buffer[rx_index], sizeof(Parameter::Parameter));
      rx_index += sizeof(Parameter::Parameter);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.request) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->request, sizeof(Request::Request));
      tx_index += sizeof(Request::Request);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->request, &rx_packet->data.buffer[rx_index], sizeof(Request::Request));
      rx_index += sizeof(Request::Request);
    case AccessRequestTypes::IGNORE:
      break;
  }

  for (int i = tx_index; i < sizeof(robocar_data_t); ++i)
    tx_packet->data.buffer[i] = 0;

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, tx_index);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "tx-packet: " << RED;
  for (uint8_t byte : tx_packet->header.buffer) std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint8_t byte: tx_packet->data.buffer) std::cout << +byte << " ";
  std::cout << RESET << std::endl;
#endif

  return EXIT_SUCCESS;
}
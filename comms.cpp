//
// Created by Lars Schwarz on 04.10.2023.
//

#include "comms.h"
#include <iostream>
#include <bitset>
#include <cstring>

inline uint8_t Comms::Comms::crc(const uint8_t *data_, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum += data_[i];
  }
  return sum;
}

uint8_t Comms::CommsMaster::exchange(AccessRequest access_request) {
  std::cout << "MASTER" << std::endl;
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
          .request_data_length = 0,
          .response_data_length = 0,
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
  header.request_data_length = 0;
  header.response_data_length = 0;
  header.crc = 0;
#endif

  // Copy current values to the tx buffer based on the current request and calculate the corresponding response package length.
  switch (access_request.state) {
    case AccessRequestTypes::SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->state, sizeof(State::State));
      header.request_data_length += sizeof(State::State);
      break;
    case AccessRequestTypes::GET:
      header.response_data_length += sizeof(State::State);
      break;
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.sensor) {
    case AccessRequestTypes::SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length],  &data->sensor, sizeof(Sensor::Sensor));
      header.request_data_length += sizeof(Sensor::Sensor);
      break;
    case AccessRequestTypes::GET:
      header.response_data_length += sizeof(Sensor::Sensor);
      break;
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.data) {
    case AccessRequestTypes::SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->data, sizeof(Data::Data));
      header.request_data_length += sizeof(Data::Data);
      break;
    case AccessRequestTypes::GET:
      header.response_data_length += sizeof(Data::Data);
      break;
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.parameter) {
    case AccessRequestTypes::SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->parameter, sizeof(Parameter::Parameter));
      header.request_data_length += sizeof(Parameter::Parameter);
      break;
    case AccessRequestTypes::GET:
      header.response_data_length += sizeof(Parameter::Parameter);
      break;
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.request) {
    case AccessRequestTypes::SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->request, sizeof(Request::Request));
      header.request_data_length += sizeof(Request::Request);
      break;
    case AccessRequestTypes::GET:
      header.response_data_length += sizeof(Request::Request);
      break;
    case AccessRequestTypes::IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, header.response_data_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "tx-packet: " << RED;
  for (uint8_t byte : tx_packet->header.buffer)
    std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint32_t i = 0; i < header.request_data_length; ++i)
    std::cout << +tx_packet->data.buffer[i] << " ";
  std::cout << RESET << std::endl;

  std::cout << "tx-length: ";
  if (header_old.response_data_length > header.request_data_length)
    std::cout << header_old.response_data_length + sizeof(comms_packet_header_t);
  else
    std::cout << header.request_data_length + sizeof(comms_packet_header_t);
  std::cout << RESET << " (" << RED << sizeof(comms_packet_header_t) << RESET << ", "
            << GREEN << +tx_packet->header.request_data_length << RESET << ", ";
  if (header_old.response_data_length > header.request_data_length)
    std::cout << YELLOW << header_old.response_data_length - header.request_data_length;
  else
    std::cout << YELLOW << "0";
  std::cout << RESET << ") -> (H, D, B)" << std::endl;
#endif

#if TARGET_PLATFORM == PLATFORM_ESP
  // TODO: Not sure if the length must be a multiple of 4. Got some warning in the code: [WARN] DMA buffer size must be multiples of 4 bytes
  if (header_old.response_data_length > header.request_data_length)
    spi->transfer(tx_packet->buffer, rx_packet->buffer, header_old.response_data_length + sizeof(comms_packet_header_t));
  else
    spi->transfer(tx_packet->buffer, rx_packet->buffer, header.request_data_length + sizeof(comms_packet_header_t));
#endif

  // Update data for the next request
  header_old = header;

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "rx-packet: " << RED;
  for (uint8_t byte : rx_packet->header.buffer)
    std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint32_t i = 0; i < rx_packet->header.response_data_length; ++i)
    std::cout << +rx_packet->data.buffer[i] << " ";
  std::cout << RESET << std::endl;

  std::cout << "rx-length: " << +rx_packet->header.response_data_length + sizeof(comms_packet_header_t);
  std::cout << RESET << " (" << RED << sizeof(comms_packet_header_t) << RESET << ", "
            << GREEN << +rx_packet->header.response_data_length << RESET
            << ") -> (H, D)" << std::endl;
#endif

  // Validate the received package
  uint8_t calculated_checksum = crc(rx_packet->data.buffer, rx_packet->header.response_data_length);
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
    std::cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC byte is invalid";
    if (rx_packet->header.crc != calculated_checksum)
      std::cout << " Checksum is wrong got: " << +calculated_checksum << " expected: " << +rx_packet->header.crc;
    std::cout << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

uint8_t Comms::CommsSlave::response() {
#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "SLAVE" << std::endl;
  std::cout << "rx-packet: " << RED;
  for (unsigned char byte : rx_packet->header.buffer)
    std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint32_t i = 0; i < rx_packet->header.request_data_length; ++i)
    std::cout << +rx_packet->data.buffer[i] << " ";
  std::cout << RESET << std::endl;

  std::cout << "rx-length: " << +rx_packet->header.request_data_length + sizeof(comms_packet_header_t);
  std::cout << RESET << " (" << RED << sizeof(comms_packet_header_t) << RESET << ", "
            << GREEN << +rx_packet->header.request_data_length << RESET
            << ") -> (H, D)" << std::endl;
#endif
  // Build the current access type structure
  AccessRequest access_request = {
          .state = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b00000000000011)),
          .sensor = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000001100) >> 2),
          .data = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000000110000) >> 4),
          .parameter = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000000011000000) >> 6),
          .request = static_cast<AccessRequestTypes>((rx_packet->header.access_type & 0b0000001100000000) >> 8),
  };

  // Validate the incoming data and response accordingly
  uint8_t calculated_checksum = crc(rx_packet->data.buffer, rx_packet->header.response_data_length);
  if (rx_packet->header.sync != SYNC || rx_packet->header.crc != calculated_checksum) {
    std::cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC byte is invalid";
    if (rx_packet->header.crc != calculated_checksum)
      std::cout << " Checksum is wrong got: " << +calculated_checksum << " expected: " << +rx_packet->header.crc;
    std::cout << std::endl;
    return EXIT_FAILURE;
  }

  // Generate a response header
  // TODO: WTF why? It works on ARM Platform but not on AVR
#if TARGET_PLATFORM == PLATFORM_ARM || TARGET_PLATFORM == PLATFORM_STM
  comms_packet_header_t header = {
          .sync = SYNC,
          .access_type = static_cast<uint16_t>((static_cast<uint8_t>(access_request.request) << 8) |
                                               (static_cast<uint8_t>(access_request.parameter) << 6) |
                                               (static_cast<uint8_t>(access_request.data) << 4) |
                                               (static_cast<uint8_t>(access_request.sensor) << 2) |
                                               (static_cast<uint8_t>(access_request.state) << 0)),
          .request_data_length = 0,
          .response_data_length = 0,
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
  header.request_data_length = 0;
  header.response_data_length = 0;
  header.crc = 0;
#endif

  switch (access_request.state) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->state, sizeof(State::State));
      header.response_data_length += sizeof(State::State);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->state, &rx_packet->data.buffer[header.request_data_length], sizeof(State::State));
      header.request_data_length += sizeof(State::State);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.sensor) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->sensor, sizeof(Sensor::Sensor));
      header.response_data_length += sizeof(Sensor::Sensor);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->sensor, &rx_packet->data.buffer[header.request_data_length], sizeof(Sensor::Sensor));
      header.request_data_length += sizeof(Sensor::Sensor);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.data) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->data, sizeof(Data::Data));
      header.response_data_length += sizeof(Data::Data);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->data, &rx_packet->data.buffer[header.request_data_length], sizeof(Data::Data));
      header.request_data_length += sizeof(Data::Data);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.parameter) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->parameter, sizeof(Parameter::Parameter));
      header.response_data_length += sizeof(Parameter::Parameter);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->parameter, &rx_packet->data.buffer[header.request_data_length], sizeof(Parameter::Parameter));
      header.request_data_length += sizeof(Parameter::Parameter);
    case AccessRequestTypes::IGNORE:
      break;
  }
  switch (access_request.request) {
    case AccessRequestTypes::GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->request, sizeof(Request::Request));
      header.response_data_length += sizeof(Request::Request);
      break;
    case AccessRequestTypes::SET:
      memcpy(&data->request, &rx_packet->data.buffer[header.request_data_length], sizeof(Request::Request));
      header.request_data_length += sizeof(Request::Request);
    case AccessRequestTypes::IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, header.response_data_length);
  // TODO: This can be optimized with pointers.
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

#if TARGET_PLATFORM == PLATFORM_ARM
  std::cout << "tx-packet: " << RED;
  for (unsigned char byte : tx_packet->header.buffer)
    std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint32_t i = 0; i < header.response_data_length; ++i)
    std::cout << +tx_packet->data.buffer[i] << " ";
  std::cout << RESET << std::endl;

  std::cout << "tx-length: " << header.response_data_length + sizeof(comms_packet_header_t);
  std::cout << RESET << " (" << RED << sizeof(comms_packet_header_t) << RESET << ", "
            << GREEN << +tx_packet->header.response_data_length << RESET
            << ") -> (H, D)" << std::endl;
#endif

  return EXIT_SUCCESS;
}
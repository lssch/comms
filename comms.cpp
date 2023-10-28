//
// Created by Lars Schwarz on 04.10.2023.
//

#include "comms.h"
#include <iostream>
#include <bitset>

Comms::Comms(Comms::comms_packet_t *ptr_rx_data, Comms::comms_packet_t *ptr_tx_data, robocar_data_t *ptr_data):
  rx_packet(ptr_rx_data),
  tx_packet(ptr_tx_data),
  data(ptr_data) {}

inline uint8_t Comms::crc(const uint8_t *data_, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum += data_[i];
  }
  return sum;
}

uint8_t CommsMaster::exchange(comms_access_request_t access_request) {
  std::cout << "MASTER" << std::endl;
  // Generate a new request header based on the current request
  comms_packet_header_t header = {
          .sync = SYNC,
          .access_type = static_cast<uint16_t>((access_request.request << 8) |
                                               (access_request.parameter << 6) |
                                               (access_request.data << 4) |
                                               (access_request.sensor << 2) |
                                               (access_request.state << 0)),
          .request_data_length = 0,
          .response_data_length = 0,
          .crc = 0
  };

  // Copy current values to the tx buffer based on the current request and calculate the corresponding response package length.
  switch (access_request.state) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->state, sizeof(state_t));
      header.request_data_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_data_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.sensor) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->sensor, sizeof(sensor_t));
      header.request_data_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_data_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.data) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->data, sizeof(data_t));
      header.request_data_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_data_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.parameter) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->parameter, sizeof(parameter_t));
      header.request_data_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_data_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.request) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_data_length], &data->request, sizeof(request_t));
      header.request_data_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_data_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, header.response_data_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

  std::cout << "tx-packet: " << RED;
  for (unsigned char byte : tx_packet->header.buffer)
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

  std::cout << "Expected response length: " << header_old.response_data_length << std::endl;

  // Update data for the next request
  access_request_old = access_request;
  header_old = header;

  // TODO: SPI exchange

  std::cout << "rx-packet: " << RED;
  for (unsigned char byte : rx_packet->header.buffer)
    std::cout << +byte << " ";
  std::cout << GREEN;
  for (uint32_t i = 0; i < rx_packet->header.response_data_length; ++i)
    std::cout << +rx_packet->data.buffer[i] << " ";
  std::cout << RESET << std::endl;

  std::cout << "rx-length: " << +rx_packet->header.response_data_length + sizeof(comms_packet_header_t);
  std::cout << RESET << " (" << RED << sizeof(comms_packet_header_t) << RESET << ", "
            << GREEN << +rx_packet->header.response_data_length << RESET
            << ") -> (H, D)" << std::endl;

  // Validate the received package
  uint16_t rx_packet_calculated_checksum = crc(rx_packet->data.buffer, rx_packet->header.response_data_length);
  if (rx_packet->header.sync == SYNC && rx_packet->header.crc == rx_packet_calculated_checksum) {
    uint16_t rx_index = 0;
    // Coppy the valid data witch was generated by the access request from the last cycle into the local ram
    if (access_request_old.state == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->state, &rx_packet->data.buffer[rx_index], sizeof(state_t));
      rx_index += sizeof(state_t);
    }
    if (access_request_old.sensor == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->sensor, &rx_packet->data.buffer[rx_index], sizeof(sensor_t));
      rx_index += sizeof(sensor_t);
    }
    if (access_request_old.data == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->data, &rx_packet->data.buffer[rx_index], sizeof(data_t));
      rx_index += sizeof(data_t);
    }
    if (access_request_old.parameter == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->parameter, &rx_packet->data.buffer[rx_index], sizeof(parameter_t));
      rx_index += sizeof(parameter_t);
    }
    if (access_request_old.request == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->request, &rx_packet->data.buffer[rx_index], sizeof(request_t));
      rx_index += sizeof(request_t);
    }
  } else {
    std::cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC byte is invalid";
    if (rx_packet->header.crc != rx_packet_calculated_checksum)
      std::cout << " Checksum is wrong got: " << rx_packet_calculated_checksum << " expected: " << +rx_packet->header.crc;
    std::cout << std::endl;
  }

  return 1;
}

uint8_t CommsSlave::response() {
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

  // Build the current access type structure
  comms_access_request_t access_type = {
          .state = static_cast<comms_access_type_e>((rx_packet->header.access_type & 0b00000000000011)),
          .sensor = static_cast<comms_access_type_e>((rx_packet->header.access_type & 0b0000000000001100) >> 2),
          .data = static_cast<comms_access_type_e>((rx_packet->header.access_type & 0b0000000000110000) >> 4),
          .parameter = static_cast<comms_access_type_e>((rx_packet->header.access_type & 0b0000000011000000) >> 6),
          .request = static_cast<comms_access_type_e>((rx_packet->header.access_type & 0b0000001100000000) >> 8),
  };

  // Validate the incoming data and response accordingly
  uint8_t calculated_checksum = crc(rx_packet->data.buffer, rx_packet->header.request_data_length);
  if (rx_packet->header.sync != SYNC || rx_packet->header.crc != calculated_checksum) {
    std::cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) std::cout << " SYNC byte is invalid";
    if (rx_packet->header.crc != calculated_checksum)
      std::cout << " Checksum is wrong got: " << calculated_checksum << " expected: " << +rx_packet->header.crc << std::endl;
    return EXIT_FAILURE;
  }

  // Generate a response header
  comms_packet_header_t header = {
          .sync = SYNC,
          .request_data_length = 0,
          .response_data_length = 0,
          .crc = 0
  };

  switch (access_type.state) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->state, sizeof(state_t));
      header.response_data_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->state, &rx_packet->data.buffer[header.request_data_length], sizeof(state_t));
      header.request_data_length += sizeof(state_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.sensor) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->sensor, sizeof(sensor_t));
      header.response_data_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->sensor, &rx_packet->data.buffer[header.request_data_length], sizeof(sensor_t));
      header.request_data_length += sizeof(sensor_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.data) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->data, sizeof(data_t));
      header.response_data_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->data, &rx_packet->data.buffer[header.request_data_length], sizeof(data_t));
      header.request_data_length += sizeof(data_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.parameter) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->parameter, sizeof(parameter_t));
      header.response_data_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->parameter, &rx_packet->data.buffer[header.request_data_length], sizeof(parameter_t));
      header.request_data_length += sizeof(parameter_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.request) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_data_length], &data->request, sizeof(request_t));
      header.response_data_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->request, &rx_packet->data.buffer[header.request_data_length], sizeof(request_t));
      header.request_data_length += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.crc = crc(tx_packet->data.buffer, header.response_data_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

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

  return EXIT_SUCCESS;
}
//
// Created by Lars Schwarz on 04.10.2023.
//

#include "comms.h"
#include <iostream>
#include <bitset>

using namespace std;

Comms::Comms(Comms::comms_packet_t *ptr_rx_data, Comms::comms_packet_t *ptr_tx_data, robocar_data_t *ptr_data) {
  rx_packet = ptr_rx_data;
  tx_packet = ptr_tx_data;
  data = ptr_data;
}

uint8_t Comms::checksum(const uint8_t *data_, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum += data_[i];
  }
  return sum;
}

void CommsMaster::transmit() {
  printf("Master: TX-PART\n");
  // Generate a new request header based on the current request
  comms_packet_header_t header = {
          .sync = SYNC,
          .access_request = static_cast<uint16_t>((access_request.state << 8) |
                                                  (access_request.parameter << 6) |
                                                  (access_request.data << 4) |
                                                  (access_request.sensor << 2) |
                                                  (access_request.state << 0)),
          .request_package_length = 0,
          .response_package_length = 0,
          .checksum = 0
  };

  // Copy current values to the tx buffer based on the current request
  switch (access_request.state) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_package_length], &data->state, sizeof(state_t));
      header.request_package_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_package_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.sensor) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_package_length], &data->sensor, sizeof(sensor_t));
      header.request_package_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_package_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.data) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_package_length], &data->data, sizeof(data_t));
      header.request_package_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_package_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.parameter) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_package_length], &data->parameter, sizeof(parameter_t));
      header.request_package_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_package_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_request.request) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[header.request_package_length], &data->request, sizeof(request_t));
      header.request_package_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      header.response_package_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.request_package_length += sizeof(comms_packet_header_t);
  header.response_package_length += sizeof(comms_packet_header_t);
  header.checksum = checksum(tx_packet->data.buffer, header.request_package_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

  cout << "tx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->header.buffer[i] << " ";
  }
  cout << "(" << sizeof(comms_packet_header_t) << ")" << endl;

  cout << "tx_packet: ";
  for (uint32_t i = 0; i < header.request_package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->data.buffer[i] << " ";
  }
  cout << "(" << +tx_packet->header.request_package_length - sizeof(comms_packet_header_t) << ")" << endl;

  cout << "tx_packet length: ";
  if (header_old.response_package_length > header.request_package_length)
    cout << header_old.response_package_length;
  else
    cout << header.request_package_length;
  cout << " (" << sizeof(comms_packet_header_t) << ", " << +tx_packet->header.request_package_length - sizeof(comms_packet_header_t) << ", ";
  if (header_old.response_package_length > header.request_package_length)
    cout << header_old.response_package_length - header.request_package_length;
  else
    cout << "0";
  cout << ") -> (H, D, B)" << endl;

  // Update data for the next request
  access_request_old = access_request;
  header_old = header;
}

uint8_t CommsMaster::receive() {
  cout << "Master: RX-PART\n";
  cout << "rx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->header.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_packet content: ";
  for (uint32_t i = 0; i < rx_packet->header.response_package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_package length: " << +rx_packet->header.response_package_length << " bytes" << endl;

  // Validate the received package
  uint16_t rx_packet_calculated_checksum = checksum(rx_packet->data.buffer, rx_packet->header.response_package_length);
  if (rx_packet->header.sync == SYNC && rx_packet->header.checksum == rx_packet_calculated_checksum) {
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
    cout <<"RX-PART SUCCESSFUL \n" << endl;
    return EXIT_SUCCESS;
  } else {
    cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) cout << " SYNC byte is invalid";
    if (rx_packet->header.checksum != rx_packet_calculated_checksum)
      cout << " Checksum is wrong got: " << rx_packet_calculated_checksum << " expected: " << +rx_packet->header.checksum << endl;
  }
  return EXIT_FAILURE;
}

uint8_t CommsSlave::response() {
  cout << "SLAVE: RX-PART\n";
  cout << "rx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->header.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_packet content: ";
  for (uint32_t i = 0; i < rx_packet->header.request_package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_package length: " << +rx_packet->header.request_package_length << endl;

  // Build the current access type structure
  comms_access_request_t access_type = {
          .state = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b00000000000011)),
          .sensor = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000000001100) >> 2),
          .data = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000000110000) >> 4),
          .parameter = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000011000000) >> 6),
          .request = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000001100000000) >> 8),
  };

  // Validate the incoming data and response accordingly
  uint8_t calculated_checksum = checksum(rx_packet->data.buffer, rx_packet->header.request_package_length);
  if (rx_packet->header.sync != SYNC || rx_packet->header.checksum != calculated_checksum) {
    cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) cout << " SYNC byte is invalid";
    if (rx_packet->header.checksum != calculated_checksum)
      cout << " Checksum is wrong got: " << calculated_checksum << " expected: " << +rx_packet->header.checksum << endl;
    return EXIT_FAILURE;
  }

  // Generate a response header
  comms_packet_header_t header = {
          .sync = SYNC,
          .request_package_length = 0,
          .response_package_length = 0,
          .checksum = 0
  };

  switch (access_type.state) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_package_length], &data->state, sizeof(state_t));
      header.response_package_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->state, &rx_packet->data.buffer[header.request_package_length], sizeof(state_t));
      header.request_package_length += sizeof(state_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.sensor) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_package_length], &data->sensor, sizeof(sensor_t));
      header.response_package_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->sensor, &rx_packet->data.buffer[header.request_package_length], sizeof(sensor_t));
      header.request_package_length += sizeof(sensor_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.data) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_package_length], &data->data, sizeof(data_t));
      header.response_package_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->data, &rx_packet->data.buffer[header.request_package_length], sizeof(data_t));
      header.request_package_length += sizeof(data_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.parameter) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_package_length], &data->parameter, sizeof(parameter_t));
      header.response_package_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->parameter, &rx_packet->data.buffer[header.request_package_length], sizeof(parameter_t));
      header.request_package_length += sizeof(parameter_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (access_type.request) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[header.response_package_length], &data->request, sizeof(request_t));
      header.response_package_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->request, &rx_packet->data.buffer[header.request_package_length], sizeof(request_t));
      header.request_package_length += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.response_package_length += sizeof(comms_packet_header_t);
  header.request_package_length += sizeof(comms_packet_header_t);
  header.checksum = checksum(tx_packet->data.buffer, header.response_package_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

  printf("SLAVE: TX-PART\n");

  cout << "tx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->header.buffer[i] << " ";
  }
  cout << "(" << sizeof(comms_packet_header_t) << ")" << endl;

  cout << "tx_packet content: ";
  for (uint32_t i = 0; i < header.response_package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->data.buffer[i] << " ";
  }
  cout << "(" << +header.response_package_length - sizeof(comms_packet_header_t) << ")" << endl;

  cout << "tx_packet length: " << +header.response_package_length << " (" << sizeof(comms_packet_header_t) << ", "
  << +header.response_package_length - sizeof(comms_packet_header_t) << ", 0) -> (H, D, B)" << endl;

  return EXIT_SUCCESS;
}
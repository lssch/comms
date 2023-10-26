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
          .access_request = static_cast<uint16_t>((current_access_request.state << 8) |
                                                  (current_access_request.parameter << 6) |
                                                  (current_access_request.data << 4) |
                                                  (current_access_request.sensor << 2) |
                                                  (current_access_request.state << 0)),
          .package_length = 0,
          .checksum = 0
  };

  // Copy current values to the tx buffer based on the current request
  uint16_t tx_index = 0;
  last_rx_data_length = sizeof(comms_packet_header_t);
  switch (current_access_request.state) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->state, sizeof(state_t));
      tx_index += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      last_rx_data_length += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (current_access_request.sensor) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->sensor, sizeof(sensor_t));
      tx_index += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      last_rx_data_length += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (current_access_request.data) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->data, sizeof(data_t));
      tx_index += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      last_rx_data_length += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (current_access_request.parameter) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->parameter, sizeof(parameter_t));
      tx_index += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      last_rx_data_length += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }
  switch (current_access_request.request) {
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->request, sizeof(request_t));
      tx_index += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_GET:
      last_rx_data_length += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.package_length = sizeof(comms_packet_header_t) + tx_index;
  header.checksum = checksum(tx_packet->data.buffer, header.package_length);
  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

  // Update data for the next request
  last_access_request = current_access_request;

  cout << "tx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->header.buffer[i] << " ";
  }
  cout << endl;

  current_tx_data_length = (last_rx_data_length > tx_packet->header.package_length) ? last_rx_data_length : tx_packet->header.package_length;
  cout << "tx_packet: ";
  for (uint32_t i = 0; i < current_tx_data_length - sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "tx_package is " << +tx_packet->header.package_length << " bytes long";
  if (last_rx_data_length > tx_packet->header.package_length)
    cout << " Adding " << +last_rx_data_length - tx_packet->header.package_length << " extra Bytes for the rx_packet of the last request";
  cout << endl;
}

uint8_t CommsMaster::receive() {
  cout << "Master: RX-PART\n";
  cout << "rx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->header.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_packet: ";
  for (uint32_t i = 0; i < rx_packet->header.package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_package is " << +rx_packet->header.package_length << " bytes long" << endl;

  // Validate the received package
  uint16_t rx_packet_calculated_checksum = checksum(rx_packet->data.buffer, rx_packet->header.package_length);
  if (rx_packet->header.sync == SYNC && rx_packet->header.checksum == rx_packet_calculated_checksum) {
    uint16_t rx_index = 0;
    // Coppy the valid data witch was generated by the access request from the last cycle into the local ram
    if (last_access_request.state == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->state, &rx_packet->data.buffer[rx_index], sizeof(state_t));
      rx_index += sizeof(state_t);
    }
    if (last_access_request.sensor == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->sensor, &rx_packet->data.buffer[rx_index], sizeof(sensor_t));
      rx_index += sizeof(sensor_t);
    }
    if (last_access_request.data == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->data, &rx_packet->data.buffer[rx_index], sizeof(data_t));
      rx_index += sizeof(data_t);
    }
    if (last_access_request.parameter == COMMS_ACCESS_REQUEST_GET) {
      memcpy(&data->parameter, &rx_packet->data.buffer[rx_index], sizeof(parameter_t));
      rx_index += sizeof(parameter_t);
    }
    if (last_access_request.request == COMMS_ACCESS_REQUEST_GET) {
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

  cout << "rx_packet: ";
  for (uint32_t i = 0; i < rx_packet->header.package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +rx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "rx_package is " << +rx_packet->header.package_length << " bytes long" << endl;

  // Build the current access type structure
  comms_access_request_t access_type = {
          .state = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b00000000000011)),
          .sensor = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000000001100) >> 2),
          .data = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000000110000) >> 4),
          .parameter = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000000011000000) >> 6),
          .request = static_cast<comms_access_type_e>((rx_packet->header.access_request & 0b0000001100000000) >> 8),
  };

  // Validate the incoming data and response accordingly
  uint8_t calculated_checksum = checksum(rx_packet->data.buffer, rx_packet->header.package_length);
  if (rx_packet->header.sync != SYNC || rx_packet->header.checksum != calculated_checksum) {
    cout <<"Dropping curren package... Package is corrupted.";
    if (rx_packet->header.sync != SYNC) cout << " SYNC byte is invalid";
    if (rx_packet->header.checksum != calculated_checksum)
      cout << " Checksum is wrong got: " << calculated_checksum << " expected: " << +rx_packet->header.checksum << endl;
    return EXIT_FAILURE;
  }

  uint16_t rx_index = 0, tx_index = 0;

  // Generate a response header
  comms_packet_header_t header = {
          .sync = SYNC,
          .package_length = 0,
          .checksum = 0
  };

  switch (access_type.state) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->state, sizeof(state_t));
      tx_index += sizeof(state_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->state, &rx_packet->data.buffer[rx_index], sizeof(state_t));
      rx_index += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  switch (access_type.sensor) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->sensor, sizeof(sensor_t));
      tx_index += sizeof(sensor_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->sensor, &rx_packet->data.buffer[rx_index], sizeof(sensor_t));
      rx_index += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  switch (access_type.data) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->data, sizeof(data_t));
      tx_index += sizeof(data_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->data, &rx_packet->data.buffer[rx_index], sizeof(data_t));
      rx_index += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  switch (access_type.parameter) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->parameter, sizeof(parameter_t));
      tx_index += sizeof(parameter_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->parameter, &rx_packet->data.buffer[rx_index], sizeof(parameter_t));
      rx_index += sizeof(parameter_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  switch (access_type.request) {
    case COMMS_ACCESS_REQUEST_GET:
      memcpy(&tx_packet->data.buffer[tx_index], &data->request, sizeof(request_t));
      tx_index += sizeof(request_t);
      break;
    case COMMS_ACCESS_REQUEST_SET:
      memcpy(&data->request, &rx_packet->data.buffer[rx_index], sizeof(request_t));
      rx_index += sizeof(request_t);
    case COMMS_ACCESS_REQUEST_IGNORE:
      break;
  }

  // Set the dynamic part of the header
  header.package_length = sizeof(comms_packet_header_t) + tx_index;
  header.checksum = checksum(tx_packet->data.buffer, header.package_length);

  memcpy(tx_packet->header.buffer, header.buffer, sizeof(comms_packet_header_t));

  printf("SLAVE: TX-PART\n");

  cout << "tx_header: ";
  for (uint32_t i = 0; i < sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->header.buffer[i] << " ";
  }
  cout << endl;

  cout << "tx_packet: ";
  for (uint32_t i = 0; i < tx_packet->header.package_length - sizeof(comms_packet_header_t); ++i) {
    cout << +tx_packet->data.buffer[i] << " ";
  }
  cout << endl;

  cout << "tx_package access request is: " << +tx_packet->header.access_request << endl;
  cout << "tx_package is " << +tx_packet->header.package_length << " bytes long" << endl;

  return EXIT_SUCCESS;
}
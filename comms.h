//
// Created by Lars Schwarz on 04.10.2023.
//

#ifndef COMMS_H
#define COMMS_H

# define PLATFORM_ARM 0
# define PLATFORM_ESP 1
# define PLATFORM_STM 2


#define TARGET_PLATFORM PLATFORM_STM

#if TARGET_PLATFORM == PLATFORM_STM
#include "stm32f4xx_hal.h"
#include "types/types.h"
#define RESET ""
#define RED ""
#define GREEN ""
#define YELLOW ""
# elif TARGET_PLATFORM == PLATFORM_ESP
#include "ESP32DMASPIMaster.h"
#include "types.h"
#include "FastLED.h"
#include "main.h"
#define RESET ""
#define RED ""
#define GREEN ""
#define YELLOW ""
# elif TARGET_PLATFORM == PLATFORM_ARM
#include "../types/types.h"
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#endif

#define SYNC 0xAA

namespace Comms {
  enum class AccessRequestTypes: uint8_t {
    IGNORE = 0,
    SET,
    GET,
  };

  class AccessRequest {
  public:
    AccessRequestTypes state;
    AccessRequestTypes sensor;
    AccessRequestTypes data;
    AccessRequestTypes parameter;
    AccessRequestTypes request;
  };

  class Comms {
  public:
    typedef union _comms_packet_header_t {
      struct {
        uint8_t sync;
        uint16_t access_type;
        uint8_t crc;
      } __attribute__((packed));
      uint8_t buffer[4];
    } comms_packet_header_t;

    typedef union _comms_packet_t {
      struct {
        comms_packet_header_t header;
        robocar_data_t data;
      };
      uint8_t buffer[sizeof(comms_packet_header_t) + sizeof(robocar_data_t)];
    } comms_packet_t;

    Comms(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data):
            rx_packet(ptr_rx_data),
            tx_packet(ptr_tx_data),
            data(ptr_data) {};

  protected:
    inline uint8_t crc(const uint8_t *data_, uint16_t length);
    virtual uint8_t exchange();

    comms_packet_t* rx_packet;
    comms_packet_t* tx_packet;
    robocar_data_t* data;
  };

  class CommsMaster: public Comms {
  public:
# if TARGET_PLATFORM == PLATFORM_ESP
    CommsMaster(comms_packet_t *ptr_rx_data, comms_packet_t *ptr_tx_data, robocar_data_t *ptr_data,
                ESP32DMASPI::Master *ptr_spi_master, CRGB* led_)
                : Comms(ptr_rx_data, ptr_tx_data, ptr_data),
                spi(ptr_spi_master),
                led(led_) {};
# elif TARGET_PLATFORM == PLATFORM_ARM
    CommsMaster(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data)
          : Comms(ptr_rx_data, ptr_tx_data, ptr_data) {};
#endif
    virtual uint8_t exchange(AccessRequest access_request);
  private:
#if TARGET_PLATFORM == PLATFORM_ESP
    ESP32DMASPI::Master *spi;
    CRGB* led;
#endif
  };

  class CommsSlave: public Comms {
  public:
#if TARGET_PLATFORM == PLATFORM_STM
  Comms::comms_packet_t rx_packet_dma;
  Comms::comms_packet_t tx_packet_dma;
  CommsSlave(SPI_HandleTypeDef *hspi, robocar_data_t* ptr_data)
    : Comms(&rx_packet_dma, &tx_packet_dma, ptr_data) {};
# elif TARGET_PLATFORM == PLATFORM_ARM
    CommsSlave(comms_packet_t* ptr_rx_data, comms_packet_t* ptr_tx_data, robocar_data_t* ptr_data)
          : Comms(ptr_rx_data, ptr_tx_data, ptr_data) {};
#endif
    virtual uint8_t exchange();
  private:
#if TARGET_PLATFORM == PLATFORM_STM
    SPI_HandleTypeDef *hspi;
#endif
  };
}

#endif //COMMS_H
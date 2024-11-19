/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/vbus/vbus_types.h>

/**
 * \brief Flags for IO memory.
 * \ingroup api_l4io
 */
enum l4io_iomem_flags_t
{
    L4IO_MEM_NONCACHED = 0, /**< Non-cache memory */
    L4IO_MEM_CACHED    = 1, /**< Cache memory */
    L4IO_MEM_USE_MTRR  = 2, /**< Use MTRR */
    L4IO_MEM_ATTR_MASK = 0xf,

    /** Use reserved area for mapping I/O memory. Flag only valid
     *  for l4io_request_iomem_region() */
    L4IO_MEM_USE_RESERVED_AREA = 0x10,

    // combinations
    L4IO_MEM_WRITE_COMBINED = L4IO_MEM_USE_MTRR | L4IO_MEM_CACHED,
};

/**
 * \brief Device types.
 * \ingroup api_l4io
 */
enum l4io_device_types_t {
  L4IO_DEVICE_INVALID = 0, /**< Invalid type */
  L4IO_DEVICE_PCI,         /**< PCI device */
  L4IO_DEVICE_USB,         /**< USB device */
  L4IO_DEVICE_GPIO,        /**< GPIO device */
  L4IO_DEVICE_I2C,         /**< I2C bus device */
  L4IO_DEVICE_MCSPI,	   /**< Multichannel Serial Port Interface */
  L4IO_DEVICE_NET,	   /**< Network device */
  L4IO_DEVICE_OTHER,       /**< Any other device without unique IDs */
  L4IO_DEVICE_ANY = ~0     /**< any type */
};

/**
 * \brief Resource types
 * \ingroup api_l4io
 */
enum l4io_resource_types_t {
  L4IO_RESOURCE_INVALID = 0, /**< Invalid type */
  L4IO_RESOURCE_IRQ,         /**< Interrupt resource */
  L4IO_RESOURCE_MEM,         /**< I/O memory resource */
  L4IO_RESOURCE_PORT,        /**< I/O port resource (x86 only) */
  L4IO_RESOURCE_GPIO_PIN,    /**< GPIO-pin resource */
  L4IO_RESOURCE_ANY = ~0     /**< any type */
};


typedef l4vbus_device_handle_t l4io_device_handle_t;
typedef l4vbus_device_handle_t l4io_resource_handle_t;

/**
 * \brief Resource descriptor.
 * \ingroup api_l4io
 *
 * For IRQ types, the end field is not used, i.e. only a single
 * interrupt can be described with a l4io_resource_t
 */
typedef l4vbus_resource_t l4io_resource_t;

/** Device descriptor.
 * \ingroup api_l4io
 */
typedef l4vbus_device_t l4io_device_t;

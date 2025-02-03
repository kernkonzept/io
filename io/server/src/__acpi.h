/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/compiler.h>

class Pci_survey_config;

class Acpi_config
{
public:
  virtual Pci_survey_config *pci_survey_config() = 0;
  virtual ~Acpi_config() = 0;
};

inline Acpi_config::~Acpi_config() {}

struct acpica_pci_irq
{
  unsigned int irq;
  unsigned char trigger;
  unsigned char polarity;
};

#ifdef CONFIG_L4IO_ACPI
int acpica_init();
void acpi_late_setup();
#else
static inline int acpica_init() { return 0; }
static inline void acpi_late_setup() {}
#endif

#if defined(CONFIG_L4IO_ACPI) && (defined(ARCH_x86) || defined(ARCH_amd64))
int acpi_ecdt_scan();
#else
static inline int acpi_ecdt_scan() { return 0; }
#endif


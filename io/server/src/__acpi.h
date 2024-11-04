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

int acpica_init();
int acpi_ecdt_scan();
void acpi_late_setup();


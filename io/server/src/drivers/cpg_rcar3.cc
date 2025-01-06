/*
 * Copyright (C) 2019, 2024 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "cpg_rcar3.h"

// Static constexpr data member declarations aren't definitions in C++11/14.
constexpr unsigned Rcar3_cpg::rmstpcr[Nr_modules];
constexpr unsigned Rcar3_cpg::smstpcr[Nr_modules];
constexpr unsigned Rcar3_cpg::mstpsr[Nr_modules];

/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *     economic rights: Kerkonzept GmbH (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once


#include <l4/re/event>

namespace Io {

struct Event_source_infos
{
  L4Re::Event_stream_state state;
  L4Re::Event_stream_info info;
};

}



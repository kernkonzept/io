#pragma once

template<typename COND>
bool
poll_loop(unsigned count, COND &&cond)
{
  while (cond())
    {
      if (count == 0)
        return false;

      --count;
    }
  return true;
}

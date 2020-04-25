#pragma once

#ifndef LINKOLLECTOR_UNREACHABLE
#if defined(__GNUC__)
#define LINKOLLECTOR_UNREACHABLE __builtin_unreachable()
#else
#define LINKOLLECTOR_UNREACHABLE __assume(0)
#endif
#endif

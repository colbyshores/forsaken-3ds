/* Minimal main.h for host trig tests: provides only what new3d.h needs. */
#pragma once
#include <stdint.h>
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef int16_t int16_t;
typedef u_int32_t DWORD;
typedef int BOOL;
typedef struct { float x, y, z; } VECTOR;
typedef struct { float _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34,_41,_42,_43,_44; } MATRIX;

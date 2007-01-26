#ifndef STDINT_H
#define STDINT_H

typedef typeof(sizeof(int))	size_t;
typedef signed long		ssize_t;
typedef signed long		off_t;

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef unsigned long long	uint64_t;

typedef signed char		int8_t;
typedef signed short		int16_t;
typedef signed long		int32_t;
typedef signed long long	int64_t;

typedef unsigned long		physaddr_t;
typedef unsigned long		intptr_t;

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#endif /* STDINT_H */

/****************************************************************************
 * Driver for Solarflare network controllers and boards
 *
 * Written by Martin Habets <mhabets@solarflare.com>
 *
 * Copyright 2012-2017 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */
#ifndef SFC_MCDI_H
#define SFC_MCDI_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#define MCDI_SEQ_MASK 0xf

/* We expect that 16- and 32-bit fields in MCDI requests and responses
 * are appropriately aligned, but 64-bit fields are only
 * 32-bit-aligned.  Also, on Siena we must copy to the MC shared
 * memory strictly 32 bits at a time, so add any necessary padding.
 */
#define MCDI_DECLARE_BUF(_name, _len)					\
	efx_dword_t _name[DIV_ROUND_UP(_len, 4)]
#define MCDI_DECLARE_BUF_OUT_OR_ERR(_name, _len)			\
	MCDI_DECLARE_BUF(_name, max_t(size_t, _len, 8))
#define _MCDI_PTR(_buf, _offset)					\
	((u8 *)(_buf) + (_offset))
#define MCDI_PTR(_buf, _field)						\
	_MCDI_PTR(_buf, MC_CMD_ ## _field ## _OFST)
#define _MCDI_CHECK_ALIGN(_ofst, _align)				\
	((_ofst) + BUILD_BUG_ON_ZERO((_ofst) & (_align - 1)))
#define _MCDI_DWORD(_buf, _field)					\
	((_buf) + (_MCDI_CHECK_ALIGN(MC_CMD_ ## _field ## _OFST, 4) >> 2))

#define MCDI_WORD(_buf, _field)						\
	((u16)BUILD_BUG_ON_ZERO(MC_CMD_ ## _field ## _LEN != 2) +	\
	 le16_to_cpu(*(__force const __le16 *)MCDI_PTR(_buf, _field)))
#define MCDI_SET_DWORD(_buf, _field, _value)				\
	EFX_POPULATE_DWORD_1(*_MCDI_DWORD(_buf, _field), EFX_DWORD_0, _value)
#define MCDI_DWORD(_buf, _field)					\
	EFX_DWORD_FIELD(*_MCDI_DWORD(_buf, _field), EFX_DWORD_0)
#define MCDI_POPULATE_DWORD_1(_buf, _field, _name1, _value1)		\
	EFX_POPULATE_DWORD_1(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1)
#define MCDI_POPULATE_DWORD_2(_buf, _field, _name1, _value1,		\
			      _name2, _value2)				\
	EFX_POPULATE_DWORD_2(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2)
#define MCDI_POPULATE_DWORD_3(_buf, _field, _name1, _value1,		\
			      _name2, _value2, _name3, _value3)		\
	EFX_POPULATE_DWORD_3(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2,		\
			     MC_CMD_ ## _name3, _value3)
#define MCDI_POPULATE_DWORD_4(_buf, _field, _name1, _value1,		\
			      _name2, _value2, _name3, _value3,		\
			      _name4, _value4)				\
	EFX_POPULATE_DWORD_4(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2,		\
			     MC_CMD_ ## _name3, _value3,		\
			     MC_CMD_ ## _name4, _value4)
#define MCDI_POPULATE_DWORD_5(_buf, _field, _name1, _value1,		\
			      _name2, _value2, _name3, _value3,		\
			      _name4, _value4, _name5, _value5)		\
	EFX_POPULATE_DWORD_5(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2,		\
			     MC_CMD_ ## _name3, _value3,		\
			     MC_CMD_ ## _name4, _value4,		\
			     MC_CMD_ ## _name5, _value5)
#define MCDI_POPULATE_DWORD_6(_buf, _field, _name1, _value1,		\
			      _name2, _value2, _name3, _value3,		\
			      _name4, _value4, _name5, _value5,		\
			      _name6, _value6)				\
	EFX_POPULATE_DWORD_6(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2,		\
			     MC_CMD_ ## _name3, _value3,		\
			     MC_CMD_ ## _name4, _value4,		\
			     MC_CMD_ ## _name5, _value5,		\
			     MC_CMD_ ## _name6, _value6)
#define MCDI_POPULATE_DWORD_7(_buf, _field, _name1, _value1,		\
			      _name2, _value2, _name3, _value3,		\
			      _name4, _value4, _name5, _value5,		\
			      _name6, _value6, _name7, _value7)		\
	EFX_POPULATE_DWORD_7(*_MCDI_DWORD(_buf, _field),		\
			     MC_CMD_ ## _name1, _value1,		\
			     MC_CMD_ ## _name2, _value2,		\
			     MC_CMD_ ## _name3, _value3,		\
			     MC_CMD_ ## _name4, _value4,		\
			     MC_CMD_ ## _name5, _value5,		\
			     MC_CMD_ ## _name6, _value6,		\
			     MC_CMD_ ## _name7, _value7)
#define MCDI_SET_QWORD(_buf, _field, _value)				\
	do {								\
		EFX_POPULATE_DWORD_1(_MCDI_DWORD(_buf, _field)[0],	\
				     EFX_DWORD_0, (u32)(_value));	\
		EFX_POPULATE_DWORD_1(_MCDI_DWORD(_buf, _field)[1],	\
				     EFX_DWORD_0, (u64)(_value) >> 32);	\
	} while (0)
#define MCDI_QWORD(_buf, _field)					\
	(EFX_DWORD_FIELD(_MCDI_DWORD(_buf, _field)[0], EFX_DWORD_0) |	\
	(u64)EFX_DWORD_FIELD(_MCDI_DWORD(_buf, _field)[1], EFX_DWORD_0) << 32)
#define MCDI_FIELD(_ptr, _type, _field)					\
	EFX_EXTRACT_DWORD(						\
		*(efx_dword_t *)					\
		_MCDI_PTR(_ptr, MC_CMD_ ## _type ## _ ## _field ## _OFST & ~3),\
		MC_CMD_ ## _type ## _ ## _field ## _LBN & 0x1f,	\
		(MC_CMD_ ## _type ## _ ## _field ## _LBN & 0x1f) +	\
		MC_CMD_ ## _type ## _ ## _field ## _WIDTH - 1)

#define _MCDI_ARRAY_PTR(_buf, _field, _index, _align)			\
	(_MCDI_PTR(_buf, _MCDI_CHECK_ALIGN(MC_CMD_ ## _field ## _OFST, _align))\
	 + (_index) * _MCDI_CHECK_ALIGN(MC_CMD_ ## _field ## _LEN, _align))
#define MCDI_DECLARE_STRUCT_PTR(_name)					\
	efx_dword_t *_name
#define MCDI_ARRAY_STRUCT_PTR(_buf, _field, _index)			\
	((efx_dword_t *)_MCDI_ARRAY_PTR(_buf, _field, _index, 4))
#define MCDI_VAR_ARRAY_LEN(_len, _field)				\
	min_t(size_t, MC_CMD_ ## _field ## _MAXNUM,			\
	      ((_len) - MC_CMD_ ## _field ## _OFST) / MC_CMD_ ## _field ## _LEN)
#define MCDI_ARRAY_WORD(_buf, _field, _index)				\
	(BUILD_BUG_ON_ZERO(MC_CMD_ ## _field ## _LEN != 2) +		\
	 le16_to_cpu(*(__force const __le16 *)				\
		     _MCDI_ARRAY_PTR(_buf, _field, _index, 2)))
#define _MCDI_ARRAY_DWORD(_buf, _field, _index)				\
	(BUILD_BUG_ON_ZERO(MC_CMD_ ## _field ## _LEN != 4) +		\
	 (efx_dword_t *)_MCDI_ARRAY_PTR(_buf, _field, _index, 4))
#define MCDI_SET_ARRAY_DWORD(_buf, _field, _index, _value)		\
	EFX_SET_DWORD_FIELD(*_MCDI_ARRAY_DWORD(_buf, _field, _index),	\
			    EFX_DWORD_0, _value)
#define MCDI_ARRAY_DWORD(_buf, _field, _index)				\
	EFX_DWORD_FIELD(*_MCDI_ARRAY_DWORD(_buf, _field, _index), EFX_DWORD_0)
#define _MCDI_ARRAY_QWORD(_buf, _field, _index)				\
	(BUILD_BUG_ON_ZERO(MC_CMD_ ## _field ## _LEN != 8) +		\
	 (efx_dword_t *)_MCDI_ARRAY_PTR(_buf, _field, _index, 4))
#define MCDI_SET_ARRAY_QWORD(_buf, _field, _index, _value)		\
	do {								\
		EFX_SET_DWORD_FIELD(_MCDI_ARRAY_QWORD(_buf, _field, _index)[0],\
				    EFX_DWORD_0, (u32)(_value));	\
		EFX_SET_DWORD_FIELD(_MCDI_ARRAY_QWORD(_buf, _field, _index)[1],\
				    EFX_DWORD_0, (u64)(_value) >> 32);	\
	} while (0)
#define MCDI_ARRAY_FIELD(_buf, _field1, _type, _index, _field2)		\
	MCDI_FIELD(MCDI_ARRAY_STRUCT_PTR(_buf, _field1, _index),	\
		   _type ## _TYPEDEF, _field2)

#define MCDI_EVENT_FIELD(_ev, _field)			\
	EFX_QWORD_FIELD(_ev, MCDI_EVENT_ ## _field)

#endif

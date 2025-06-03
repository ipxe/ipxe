/** @file
  EDKII extented HII IFR guid opcodes.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MDEMODULE_HII_H__
#define __MDEMODULE_HII_H__

FILE_LICENCE ( BSD2_PATENT );

#define NARROW_CHAR        0xFFF0
#define WIDE_CHAR          0xFFF1
#define NON_BREAKING_CHAR  0xFFF2

///
/// State defined for password statemachine .
///
#define BROWSER_STATE_VALIDATE_PASSWORD  0
#define BROWSER_STATE_SET_PASSWORD       1

///
/// GUIDed opcodes defined for EDKII implementation.
///
#define EFI_IFR_TIANO_GUID \
  { 0xf0b1735, 0x87a0, 0x4193, {0xb2, 0x66, 0x53, 0x8c, 0x38, 0xaf, 0x48, 0xce} }

#pragma pack(1)

///
/// EDKII implementation extension opcodes, new extension can be added here later.
///
#define EFI_IFR_EXTEND_OP_LABEL     0x0
#define EFI_IFR_EXTEND_OP_BANNER    0x1
#define EFI_IFR_EXTEND_OP_TIMEOUT   0x2
#define EFI_IFR_EXTEND_OP_CLASS     0x3
#define EFI_IFR_EXTEND_OP_SUBCLASS  0x4

///
/// Label opcode.
///
typedef struct _EFI_IFR_GUID_LABEL {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_TIANO_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_LABEL.
  ///
  UINT8                ExtendOpCode;
  ///
  /// Label Number.
  ///
  UINT16               Number;
} EFI_IFR_GUID_LABEL;

#define EFI_IFR_BANNER_ALIGN_LEFT    0
#define EFI_IFR_BANNER_ALIGN_CENTER  1
#define EFI_IFR_BANNER_ALIGN_RIGHT   2

///
/// Banner opcode.
///
typedef struct _EFI_IFR_GUID_BANNER {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_TIANO_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_BANNER
  ///
  UINT8                ExtendOpCode;
  EFI_STRING_ID        Title;       ///< The string token for the banner title.
  UINT16               LineNumber;  ///< 1-based line number.
  UINT8                Alignment;   ///< left, center, or right-aligned.
} EFI_IFR_GUID_BANNER;

///
/// Timeout opcode.
///
typedef struct _EFI_IFR_GUID_TIMEOUT {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_TIANO_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_TIMEOUT.
  ///
  UINT8                ExtendOpCode;
  UINT16               TimeOut;      ///< TimeOut Value.
} EFI_IFR_GUID_TIMEOUT;

#define EFI_NON_DEVICE_CLASS       0x00
#define EFI_DISK_DEVICE_CLASS      0x01
#define EFI_VIDEO_DEVICE_CLASS     0x02
#define EFI_NETWORK_DEVICE_CLASS   0x04
#define EFI_INPUT_DEVICE_CLASS     0x08
#define EFI_ON_BOARD_DEVICE_CLASS  0x10
#define EFI_OTHER_DEVICE_CLASS     0x20

///
/// Device Class opcode.
///
typedef struct _EFI_IFR_GUID_CLASS {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_TIANO_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_CLASS.
  ///
  UINT8                ExtendOpCode;
  UINT16               Class;          ///< Device Class from the above.
} EFI_IFR_GUID_CLASS;

#define EFI_SETUP_APPLICATION_SUBCLASS    0x00
#define EFI_GENERAL_APPLICATION_SUBCLASS  0x01
#define EFI_FRONT_PAGE_SUBCLASS           0x02
#define EFI_SINGLE_USE_SUBCLASS           0x03

///
/// SubClass opcode
///
typedef struct _EFI_IFR_GUID_SUBCLASS {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_TIANO_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_SUBCLASS.
  ///
  UINT8                ExtendOpCode;
  UINT16               SubClass;     ///< Sub Class type from the above.
} EFI_IFR_GUID_SUBCLASS;

///
/// GUIDed opcodes support for framework vfr.
///
#define EFI_IFR_FRAMEWORK_GUID \
  { 0x31ca5d1a, 0xd511, 0x4931, { 0xb7, 0x82, 0xae, 0x6b, 0x2b, 0x17, 0x8c, 0xd7 } }

///
/// Two extended opcodes are added, and new extensions can be added here later.
/// One is for framework OneOf question Option Key value;
/// another is for framework vareqval.
///
#define EFI_IFR_EXTEND_OP_OPTIONKEY  0x0
#define EFI_IFR_EXTEND_OP_VAREQNAME  0x1

///
/// Store the framework vfr option key value.
///
typedef struct _EFI_IFR_GUID_OPTIONKEY {
  EFI_IFR_OP_HEADER     Header;
  ///
  /// EFI_IFR_FRAMEWORK_GUID.
  ///
  EFI_GUID              Guid;
  ///
  /// EFI_IFR_EXTEND_OP_OPTIONKEY.
  ///
  UINT8                 ExtendOpCode;
  ///
  /// OneOf Questiond ID binded by OneOf Option.
  ///
  EFI_QUESTION_ID       QuestionId;
  ///
  /// The OneOf Option Value.
  ///
  EFI_IFR_TYPE_VALUE    OptionValue;
  ///
  /// The Framework OneOf Option Key Value.
  ///
  UINT16                KeyValue;
} EFI_IFR_GUID_OPTIONKEY;

///
/// Store the framework vfr vareqval name number.
///
typedef struct _EFI_IFR_GUID_VAREQNAME {
  EFI_IFR_OP_HEADER    Header;
  ///
  /// EFI_IFR_FRAMEWORK_GUID.
  ///
  EFI_GUID             Guid;
  ///
  /// EFI_IFR_EXTEND_OP_VAREQNAME.
  ///
  UINT8                ExtendOpCode;
  ///
  /// Question ID of the Numeric Opcode created.
  ///
  EFI_QUESTION_ID      QuestionId;
  ///
  /// For vareqval (0x100), NameId is 0x100.
  /// This value will convert to a Unicode String following this rule;
  ///            sprintf(StringBuffer, "%d", NameId) .
  /// The the Unicode String will be used as a EFI Variable Name.
  ///
  UINT16               NameId;
} EFI_IFR_GUID_VAREQNAME;

///
/// EDKII implementation extension GUID, used to indaicate there are bit fields in the varstore.
///
#define EDKII_IFR_BIT_VARSTORE_GUID \
  {0x82DDD68B, 0x9163, 0x4187, {0x9B, 0x27, 0x20, 0xA8, 0xFD, 0x60,0xA7, 0x1D}}

///
/// EDKII implementation extension flags, used to indaicate the disply style and bit width for bit filed storage.
/// Two high bits for display style and the low six bits for bit width.
///
#define EDKII_IFR_DISPLAY_BIT           0xC0
#define EDKII_IFR_DISPLAY_INT_DEC_BIT   0x00
#define EDKII_IFR_DISPLAY_UINT_DEC_BIT  0x40
#define EDKII_IFR_DISPLAY_UINT_HEX_BIT  0x80

#define EDKII_IFR_NUMERIC_SIZE_BIT  0x3F

#pragma pack()

extern EFI_GUID  gEfiIfrTianoGuid;
extern EFI_GUID  gEfiIfrFrameworkGuid;
extern EFI_GUID  gEdkiiIfrBitVarstoreGuid;

#endif

/** @file
 *
 * "it" keyboard mapping
 *
 * This file is automatically generated; do not edit
 *
 */

FILE_LICENCE(PUBLIC_DOMAIN);

#include <ipxe/keymap.h>

/** "it" basic remapping */
static struct keymap_key it_basic[] = {
    {0x1e, 0x36}, /* 0x1e => '6' */
    {0x26, 0x2f}, /* '&' => '/' */
    {0x28, 0x29}, /* '(' => ')' */
    {0x29, 0x3d}, /* ')' => '=' */
    {0x2a, 0x28}, /* '*' => '(' */
    {0x2b, 0x5e}, /* '+' => '^' */
    {0x2d, 0x27}, /* '-' => '\'' */
    {0x2f, 0x2d}, /* '/' => '-' */
    {0x3c, 0x3b}, /* '<' => ';' */
    {0x3e, 0x3a}, /* '>' => ':' */
    {0x3f, 0x5f}, /* '?' => '_' */
    {0x40, 0x22}, /* '@' => '"' */
    {0x5d, 0x2b}, /* ']' => '+' */
    {0x5e, 0x26}, /* '^' => '&' */
    {0x5f, 0x3f}, /* '_' => '?' */
    {0x60, 0x5c}, /* '`' => '\\' */
    {0x7d, 0x2a}, /* '}' => '*' */
    {0x7e, 0x7c}, /* '~' => '|' */
    {0xdc, 0x3c}, /* Pseudo-'\\' => '<' */
    {0xfc, 0x3e}, /* Pseudo-'|' => '>' */
    {0, 0}};

/** "it" AltGr remapping */
static struct keymap_key it_altgr[] = {
    {0x22, 0x23}, /* '"' => '#' */
    {0x23, 0x7e}, /* '#' => '~' */
    {0x26, 0x7b}, /* '&' => '{' */
    {0x27, 0x23}, /* '\'' => '#' */
    {0x28, 0x5d}, /* '(' => ']' */
    {0x29, 0x7d}, /* ')' => '}' */
    {0x2a, 0x5b}, /* '*' => '[' */
    {0x2d, 0x60}, /* '-' => '`' */
    {0x30, 0x7d}, /* '0' => '}' */
    {0x37, 0x7b}, /* '7' => '{' */
    {0x38, 0x5b}, /* '8' => '[' */
    {0x39, 0x5d}, /* '9' => ']' */
    {0x3a, 0x40}, /* ':' => '@' */
    {0x3b, 0x40}, /* ';' => '@' */
    {0x3d, 0x7e}, /* '=' => '~' */
    {0x40, 0x7e}, /* '@' => '~' */
    {0x51, 0x40}, /* 'Q' => '@' */
    {0x5c, 0x60}, /* '\\' => '`' */
    {0x5f, 0x60}, /* '_' => '`' */
    {0x71, 0x40}, /* 'q' => '@' */
    {0x7c, 0x7e}, /* '|' => '~' */
    {0, 0}};

/** "it" keyboard map */
struct keymap it_keymap __keymap = {
    .name = "it",
    .basic = it_basic,
    .altgr = it_altgr,
};

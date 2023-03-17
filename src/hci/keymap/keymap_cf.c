/** @file
 *
 * "cf" keyboard mapping
 *
 * This file is automatically generated; do not edit
 *
 */

FILE_LICENCE(PUBLIC_DOMAIN);

#include <ipxe/keymap.h>

/** "cf" basic remapping */
static struct keymap_key cf_basic[] = {
    {0x22, 0x60}, /* '"' => '`' */
    {0x23, 0x2f}, /* '#' => '/' */
    {0x27, 0x60}, /* '\'' => '`' */
    {0x3c, 0x27}, /* '<' => '\'' */
    {0x3e, 0x2e}, /* '>' => '.' */
    {0x40, 0x22}, /* '@' => '"' */
    {0x5b, 0x5e}, /* '[' => '^' */
    {0x5c, 0x3c}, /* '\\' => '<' */
    {0x5e, 0x3f}, /* '^' => '?' */
    {0x60, 0x23}, /* '`' => '#' */
    {0x7b, 0x5e}, /* '{' => '^' */
    {0x7c, 0x3e}, /* '|' => '>' */
    {0x7e, 0x7c}, /* '~' => '|' */
    {0, 0}};

/** "cf" AltGr remapping */
static struct keymap_key cf_altgr[] = {
    {0x22, 0x7b}, /* '"' => '{' */
    {0x27, 0x7b}, /* '\'' => '{' */
    {0x32, 0x40}, /* '2' => '@' */
    {0x3a, 0x7e}, /* ':' => '~' */
    {0x3b, 0x7e}, /* ';' => '~' */
    {0x5c, 0x7d}, /* '\\' => '}' */
    {0x60, 0x5c}, /* '`' => '\\' */
    {0x7b, 0x5b}, /* '{' => '[' */
    {0x7c, 0x7d}, /* '|' => '}' */
    {0x7d, 0x5d}, /* '}' => ']' */
    {0x7e, 0x5c}, /* '~' => '\\' */
    {0, 0}};

/** "cf" keyboard map */
struct keymap cf_keymap __keymap = {
    .name = "cf",
    .basic = cf_basic,
    .altgr = cf_altgr,
};

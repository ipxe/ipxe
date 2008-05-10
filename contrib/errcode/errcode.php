<?
/*
 * Copyright (C) 2008 Stefan Hajnoczi <stefanha@gmail.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// The path to the errcode.py script.
$ERRCODE_PATH = './errcode.py';
?>

<html>
    <head>
        <title>gPXE Error Code Lookup</title>
        <style>
            body, pre, div, form, p, h2, b, tt {
                padding: 0;
                border: 0;
                margin: 0;
            }
            body {
                padding: 0.5em;
                width: 750px;
                font-family: sans-serif;
            }
            pre {
                margin: 0.2em;
                padding: 0.1em;
                background-color: #ddd;
            }
            form {
                margin: 0.2em;
            }
            div {
                margin: 0.2em;
                padding: 0.4em;
                border: 1px dashed black;
            }
        </style>
    </head>
    <body>
<?
if (!empty($_REQUEST['e']) && preg_match('/^(0x)?[0-9a-f]{8}$/', $_REQUEST['e'])) {
?>
        <pre>
<?
    system($ERRCODE_PATH . " " . $_REQUEST['e']);
?>
        </pre>
<?
}
?>
        <form action="" method="post">
            <label for="e">Error code:</label>
            <input type="text" name="e" id="e" value="0x12345678"></input>
            <input type="submit" value="Lookup"></input>
        </form>

        <div>
            <h2>Hint:</h2>
            <p>
            Firefox users can right-click on the <b>Error code</b>
            text box and select <b>Add a Keyword for this Search...</b>.
            Set <b>name</b> to <tt>gPXE Error Code Lookup</tt> and
            <b>keyword</b> to <tt>gxpe</tt>  Then you can look up error
            codes by typing something like the following in your address
            bar: <tt>gpxe 0x3c018003</tt>
            <p>
        </div>
    </body>
</html>

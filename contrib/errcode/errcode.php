<?
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

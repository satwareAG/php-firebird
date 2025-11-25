<?php

$file = 'run-tests.php';

if (!file_exists($file)) {
    echo "run-tests.php not found.\n";
    exit(0);
}

$content = file_get_contents($file);

if (strpos($content, 'Firebird Client Version') !== false) {
    echo "run-tests.php already patched.\n";
    exit(0);
}

// Add global $php to get_summary
// We search for the function definition and add the global declaration at the start of the body.
$search_func = "function get_summary(bool \$show_ext_summary): string\n{";
$replace_func = "function get_summary(bool \$show_ext_summary): string\n{\n    global \$php;";

if (strpos($content, $search_func) === false) {
    // fallback for different formatting?
    // try with space instead of \n just in case
    $search_func_alt = "function get_summary(bool \$show_ext_summary): string {";
    if (strpos($content, $search_func_alt) !== false) {
        $content = str_replace($search_func_alt, "function get_summary(bool \$show_ext_summary): string {\n    global \$php;", $content);
    } else {
        echo "Failed to find get_summary function signature.\n";
        // Don't exit with error, just skip patching if signature mismatch, to avoid build failure
        // exit(1);
        echo "Skipping patch (signature mismatch).\n";
        exit(0);
    }
} else {
    $content = str_replace($search_func, $replace_func, $content);
}

// Insert version info in get_summary.
// We look for the end of the EXT summary block.
// Let's use the more robust search string I identified earlier.
$search_summary = "Exts tested     : ' . sprintf('%5d', count(\$exts_tested)) . '\n---------------------------------------------------------------------\n';";

// Note: Single quotes inside double quotes need escaping if they are delimiters, but here they are part of the string.
// PHP string literals in this file need correct escaping.

$replace_summary = "Exts tested     : ' . sprintf('%5d', count(\$exts_tested)) . '\n---------------------------------------------------------------------\n';
        \$cmd = \"\$php -n -d extension_dir=modules/ -d extension=interbase.so -r 'echo \\\"PHP Interbase Version: \\\" . phpversion(\\\"interbase\\\") . \\\"\\\\n\\\"; echo \\\"Firebird Client Version: \\\" . ibase_get_client_version() . \\\"\\\\n\\\";'\";
        \$ver_output = shell_exec(\$cmd);
        if (\$ver_output) {
             \$summary .= \$ver_output;
             \$summary .= \"---------------------------------------------------------------------\\n\";
        }";

if (strpos($content, $search_summary) === false) {
     echo "Failed to find summary block content. Skipping patch.\n";
     exit(0);
}

$content = str_replace($search_summary, $replace_summary, $content);

if (strpos($content, 'Firebird Client Version') === false) {
    echo "Failed to patch content (ver check).\n";
    exit(1);
}

file_put_contents($file, $content);
echo "Patched run-tests.php successfully.\n";
?>

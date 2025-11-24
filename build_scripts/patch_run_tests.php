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
        exit(1);
    }
} else {
    $content = str_replace($search_func, $replace_func, $content);
}

// Insert version info in get_summary.
// We look for the end of the EXT summary block.
$search_summary = "---------------------------------------------------------------------\n';\n    }";

// If indent match fails, we might need to be more flexible.
// But let's try to match what we saw in read_file.
// In read_file:
//     if ($show_ext_summary) {
//         $summary .= '
// ...
// ---------------------------------------------------------------------
// ';
//     }

// The closing brace is indented with 4 spaces.
// The '; is indented with 0 spaces in the echo string? No, $summary .= '...';
// The closing '; is on start of line? No.

// Let's look at the exact block from read_file output:
/*
    if ($show_ext_summary) {
        $summary .= '
=====================================================================
TEST RESULT SUMMARY
---------------------------------------------------------------------
Exts skipped    : ' . sprintf('%5d', count($exts_skipped)) . ($exts_skipped ? ' (' . implode(', ', $exts_skipped) . ')' : '') . '
Exts tested     : ' . sprintf('%5d', count($exts_tested)) . '
---------------------------------------------------------------------
';
    }
*/

// So search target:
$search_summary = "---------------------------------------------------------------------\n';\n    }";

// But wait, in PHP string literal, newlines are preserved.
// It ends with:
// ---------------------------------------------------------------------
// ';
//     }

// So:
$search_summary = "---------------------------------------------------------------------\n';\n    }";

$replace_summary = "---------------------------------------------------------------------\n';\n        \$cmd = \"\$php -n -d extension_dir=modules/ -d extension=interbase.so -r 'echo \\\"PHP Interbase Version: \\\" . phpversion(\\\"interbase\\\") . \\\"\\\\n\\\"; echo \\\"Firebird Client Version: \\\" . ibase_get_client_version() . \\\"\\\\n\\\";'\";\n        \$ver_output = shell_exec(\$cmd);\n        if (\$ver_output) {\n             \$summary .= \$ver_output;\n             \$summary .= \"---------------------------------------------------------------------\\n\";\n        }\n    }";

if (strpos($content, $search_summary) === false) {
    // Try matching longer block to be sure, or print error
    echo "Failed to find summary block end.\n";
    // Actually let's try to match larger chunk to avoid ambiguity
    $search_summary = "Exts tested     : ' . sprintf('%5d', count(\$exts_tested)) . '\n---------------------------------------------------------------------\n';";

    if (strpos($content, $search_summary) === false) {
         echo "Failed to find summary block content.\n";
         exit(1);
    }

    $replace_summary = "Exts tested     : ' . sprintf('%5d', count(\$exts_tested)) . '\n---------------------------------------------------------------------\n';
        \$cmd = \"\$php -n -d extension_dir=modules/ -d extension=interbase.so -r 'echo \\\"PHP Interbase Version: \\\" . phpversion(\\\"interbase\\\") . \\\"\\\\n\\\"; echo \\\"Firebird Client Version: \\\" . ibase_get_client_version() . \\\"\\\\n\\\";'\";
        \$ver_output = shell_exec(\$cmd);
        if (\$ver_output) {
             \$summary .= \$ver_output;
             \$summary .= \"---------------------------------------------------------------------\\n\";
        }";

    $content = str_replace($search_summary, $replace_summary, $content);
} else {
    $content = str_replace($search_summary, $replace_summary, $content);
}

if (strpos($content, 'Firebird Client Version') === false) {
    echo "Failed to patch content (ver check).\n";
    exit(1);
}

file_put_contents($file, $content);
echo "Patched run-tests.php successfully.\n";
?>

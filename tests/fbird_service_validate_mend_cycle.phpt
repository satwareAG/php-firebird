--TEST--
Service: Full validate -> mend -> re-validate cycle (OC-10)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$service = fbird_service_attach($host, $user, $password);
if (!$service) die("Skip: Check service connection");

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Step 1: Validate DB (check for errors)
echo "--- Step 1: Validate DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

// Step 2: Mend DB (repair any errors found)
echo "--- Step 2: Mend DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_RPR_MEND_DB, FBIRD_RPR_FULL);
var_dump($res);

// Step 3: Re-validate DB (confirm no remaining errors)
echo "--- Step 3: Re-validate DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

fbird_service_detach($service);
echo "done\n";
?>
--EXPECT--
--- Step 1: Validate DB ---
bool(true)
--- Step 2: Mend DB ---
bool(true)
--- Step 3: Re-validate DB ---
bool(true)
done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

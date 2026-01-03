--TEST--
Bug Issue #56: Segfault in fb::ServiceWrapper::detach() during PHP request shutdown
--EXTENSIONS--
firebird
--FILE--
<?php
require_once 'config.inc';

echo "Attaching to service manager...\n";
// Uses INI defaults from config.inc
$service = fbird_service_attach($host, $user, $password);

if ($service) {
    echo "Attached successfully.\n";
    $info = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
    echo "Server version: " . (strlen($info) > 0 ? "OK" : "FAIL") . "\n";
} else {
    echo "Failed to attach.\n";
}

echo "Exiting without explicit detach (relying on RSHUTDOWN cleanup)...\n";
?>
--EXPECTF--
Attaching to service manager...
Attached successfully.
Server version: OK
Exiting without explicit detach (relying on RSHUTDOWN cleanup)...

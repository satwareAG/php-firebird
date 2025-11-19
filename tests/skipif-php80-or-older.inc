<?php
if (PHP_MAJOR_VERSION < 8 || (PHP_MAJOR_VERSION == 8 && PHP_MINOR_VERSION < 1)) {
    die('skip: This test verifies behavior that can only be observed in PHP versions >= 8.1');
}
?>

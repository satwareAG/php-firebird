<?php
require_once('tests/interbase.inc');

echo "Testing event system...\n";

$count = 0;

function event_callback($event)
{
    global $count;
    echo "Event fired: $event (count: $count)\n";
    if ($event == 'TEST1') echo "FAIL TEST1\n";
    return (++$count < 5); /* cancel event after 5 times */
}

try {
    $link = ibase_connect($test_base);

    ibase_query("CREATE PROCEDURE pevent AS BEGIN POST_EVENT 'TEST1'; POST_EVENT 'TEST2'; END");
    ibase_commit();

    echo "Setting event handler for TEST2...\n";
    $event_handler = ibase_set_event_handler('event_callback','TEST2');
    
    echo "Executing procedure to trigger events...\n";
    for ($i = 0; $i < 3; ++$i) {
        ibase_query("EXECUTE PROCEDURE pevent");
        ibase_commit();
        usleep(100000); // 0.1 seconds
    }
    
    echo "Waiting a moment for events...\n";
    usleep(500000); // 0.5 seconds

    echo "Freeing event handler...\n";
    ibase_free_event_handler($event_handler);
    
    echo "Count: $count\n";
    echo "Test completed successfully!\n";

} catch (Exception $e) {
    echo "Error: " . $e->getMessage() . "\n";
}
?>

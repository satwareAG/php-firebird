<?php
require_once('tests/interbase.inc');

echo "Testing simple event (no recursion)...\n";

function simple_event_callback($event, $link)
{
    echo "Event received: $event\n";
    return false; /* Don't continue - one-shot event */
}

try {
    $link = ibase_connect($test_base);

    ibase_query("CREATE PROCEDURE pevent AS BEGIN POST_EVENT 'SIMPLE_TEST'; END");
    ibase_commit();

    echo "Setting one-shot event handler...\n";
    $event_handler = ibase_set_event_handler('simple_event_callback','SIMPLE_TEST');
    
    echo "Triggering single event...\n";
    ibase_query("EXECUTE PROCEDURE pevent");
    ibase_commit();
    
    echo "Waiting 500ms for event...\n";
    usleep(500000);

    echo "Freeing event handler...\n";
    ibase_free_event_handler($event_handler);
    
    echo "Simple test completed successfully!\n";

} catch (Exception $e) {
    echo "Error: " . $e->getMessage() . "\n";
}
?>

<?php

class FuzzHarness {
    private array $operations = [];
    private float $totalWeight = 0.0;
    /** @var array<string> Dictionary tokens loaded from fuzz/dictionary/sql.dict */
    public array $dictionaryTokens = [];
    public array $state = [
        'connections' => [],
        'transactions' => [],
        'statements' => [],
        'blobs' => [],
    ];
    private string $dsn;
    private string $user;
    private string $password;

    public function __construct(string $dsn, string $user, string $password) {
        $this->dsn = $dsn;
        $this->user = $user;
        $this->password = $password;
    }

    public function registerOperation(string $name, callable $fn, float $weight = 1.0): void {
        $this->operations[$name] = [
            'fn' => $fn,
            'weight' => $weight
        ];
        $this->totalWeight += $weight;
    }

    public function loadDefaultOperations(): void {
        // Load operations from static classes
        // ConnectionOps
        $this->registerOperation('connect', ConnectionOps::connect($this), 5.0);
        // DISABLED: pconnect - persistent connections share resources with regular connections, 
        // causing use-after-free when cleanup closes the shared resource.
        // $this->registerOperation('pconnect', ConnectionOps::pconnect($this), 1.0);
        $this->registerOperation('close', ConnectionOps::close($this), 3.0);
        $this->registerOperation('forceNew', ConnectionOps::forceNew($this), 1.0);

        // TransactionOps
        $this->registerOperation('trans_begin', TransactionOps::begin($this), 5.0);
        $this->registerOperation('trans_commit', TransactionOps::commit($this), 4.0);
        $this->registerOperation('trans_rollback', TransactionOps::rollback($this), 2.0);
        $this->registerOperation('trans_commit_ret', TransactionOps::commitRetaining($this), 1.0);
        
        // QueryOps
        $this->registerOperation('query_simple', QueryOps::simpleQuery($this), 10.0);
        $this->registerOperation('query_prepare', QueryOps::prepareExecute($this), 8.0);
        $this->registerOperation('query_insert', QueryOps::parameterizedInsert($this), 5.0);
        $this->registerOperation('query_fetch', QueryOps::fetchAll($this), 8.0);

        // SqlGenerator random query operations (Issue #32)
        $this->registerOperation('query_random_select', QueryOps::randomSelect($this), 8.0);
        $this->registerOperation('query_random_params', QueryOps::randomParams($this), 6.0);
        $this->registerOperation('query_random_insert', QueryOps::randomInsert($this), 4.0);
        $this->registerOperation('query_edge_strings', QueryOps::edgeCaseStrings($this), 5.0);

        // BlobOps
        $this->registerOperation('blob_create', BlobOps::createBlob($this), 3.0);
        $this->registerOperation('blob_stream', BlobOps::streamBlob($this), 3.0);
        $this->registerOperation('blob_large', BlobOps::largeBlob($this), 1.0);
        $this->registerOperation('blob_post_commit', BlobOps::postCommitAccess($this), 0.5); // Edge case

        // LogicOps (2025 Enhancement)
        $this->registerOperation('logic_tlp', LogicOps::tlpCheck($this), 5.0); // High weight to catch logic bugs
        $this->registerOperation('logic_type', LogicOps::typeCheck($this), 3.0);
    }

    public function execute(int $iterations, ?callable $progressCallback = null): FuzzResult {
        $result = new FuzzResult();

        // Bootstrap: establish initial connection before fuzzing
        $this->bootstrap();

        for ($i = 0; $i < $iterations; $i++) {
            $opName = $this->selectOperation();
            
            try {
                $fn = $this->operations[$opName]['fn'];
                $fn();
                $result->recordSuccess($opName);
            } catch (Throwable $e) {
                $error = [
                    'message' => $e->getMessage(),
                    'file' => $e->getFile(),
                    'line' => $e->getLine(),
                    'trace' => $e->getTraceAsString()
                ];
                $result->recordFailure($opName, $error);
            }

            if ($progressCallback && $i % 100 === 0) {
                $progressCallback($i, $iterations);
            }
        }

        $result->finish();
        $this->cleanup();
        return $result;
    }

    protected function selectOperation(): string {
        $rand = mt_rand() / mt_getrandmax() * $this->totalWeight;
        $current = 0.0;

        foreach ($this->operations as $name => $op) {
            $current += $op['weight'];
            if ($rand <= $current) {
                return $name;
            }
        }

        // Fallback to first operation if rounding errors occur
        return array_key_first($this->operations);
    }

    /**
     * Bootstrap: establish initial connection to ensure fuzzer can operate
     * Uses FBIRD_CONNECT_FORCE_NEW to prevent resource sharing with other connections
     * Also initializes the test table if it doesn't exist
     * @throws RuntimeException if connection fails
     */
    private function bootstrap(): void {
        echo "Connecting to: {$this->dsn}\n";
        
        // Set INI defaults for credentials (like tests/config.inc does)
        ini_set('fbird.default_user', $this->user);
        ini_set('fbird.default_password', $this->password);
        
        // Set environment variables for Firebird client authentication
        putenv("ISC_USER={$this->user}");
        putenv("ISC_PASSWORD={$this->password}");
        
        // CRITICAL: Use FBIRD_CONNECT_FORCE_NEW to prevent resource sharing
        // Without this flag, fbird_connect() returns the SAME resource for identical
        // parameters, causing use-after-free when one reference is closed
        $conn = @fbird_connect($this->dsn, $this->user, $this->password, 'UTF8', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
        if (!$conn) {
            $error = fbird_errmsg() ?: 'Unknown connection error';
            throw new RuntimeException("Bootstrap connection failed: {$error}\nDSN: {$this->dsn}");
        }
        
        $this->state['connections'][] = $conn;
        echo "Bootstrap connection established.\n";
        
        // Initialize test table (like tests/firebird.inc init_db() does)
        $this->initializeTestTable($conn);
    }
    
    /**
     * Initialize the test table required for fuzz operations
     * Creates test1 table if it doesn't exist
     */
    private function initializeTestTable($conn): void {
        // Check if test1 table exists
        $result = @fbird_query($conn, "SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TEST1'");
        $exists = $result && fbird_fetch_row($result);
        if ($result) {
            fbird_free_result($result);
        }
        
        if (!$exists) {
            echo "Creating test1 table...\n";
            $trans = fbird_trans($conn, FBIRD_WRITE | FBIRD_COMMITTED);
            if ($trans) {
                @fbird_query($trans, "CREATE TABLE test1 (i INTEGER, c VARCHAR(100))");
                fbird_commit($trans);
                echo "test1 table created.\n";
            }
        } else {
            echo "test1 table already exists.\n";
        }
    }

    private function cleanup(): void {
        // CRITICAL: Copy arrays and clear state BEFORE closing resources
        // This prevents use-after-free when foreach iterator accesses freed memory
        // See: ASan heap-use-after-free in ZEND_FE_FETCH_R_SPEC_VAR_HANDLER
        
        $blobs = $this->state['blobs'];
        $statements = $this->state['statements'];
        $transactions = $this->state['transactions'];
        $connections = $this->state['connections'];
        
        // Clear state first to prevent any callback from accessing freed resources
        $this->state = [
            'connections' => [],
            'transactions' => [],
            'statements' => [],
            'blobs' => [],
        ];
        
        // Track closed resource IDs to avoid double-close
        $closed = [];
        
        // Close blobs first (depend on transactions)
        foreach ($blobs as $blob) {
            if (is_resource($blob)) {
                $id = (int)$blob;
                if (!isset($closed[$id])) {
                    $closed[$id] = true;
                    @fbird_blob_close($blob);
                }
            }
        }
        
        // Free statements (depend on transactions/connections)
        foreach ($statements as $stmt) {
            if (is_resource($stmt)) {
                $id = (int)$stmt;
                if (!isset($closed[$id])) {
                    $closed[$id] = true;
                    @fbird_free_query($stmt);
                }
            }
        }
        
        // Rollback transactions (depend on connections)
        foreach ($transactions as $trans) {
            if (is_resource($trans)) {
                $id = (int)$trans;
                if (!isset($closed[$id])) {
                    $closed[$id] = true;
                    @fbird_rollback($trans);
                }
            }
        }
        
        // Close connections last
        foreach ($connections as $conn) {
            if (is_resource($conn)) {
                $id = (int)$conn;
                if (!isset($closed[$id])) {
                    $closed[$id] = true;
                    @fbird_close($conn);
                }
            }
        }
    }

    // Helper methods for operations
    public function getDsn(): string { return $this->dsn; }
    public function getUser(): string { return $this->user; }
    public function getPassword(): string { return $this->password; }
    
    public function getRandomConnection() {
        if (empty($this->state['connections'])) return null;
        return $this->state['connections'][array_rand($this->state['connections'])];
    }

    public function getRandomTransaction() {
        if (empty($this->state['transactions'])) return null;
        return $this->state['transactions'][array_rand($this->state['transactions'])];
    }

    public function getRandomStatement() {
        if (empty($this->state['statements'])) return null;
        return $this->state['statements'][array_rand($this->state['statements'])];
    }
    
    public function getRandomBlob() {
        if (empty($this->state['blobs'])) return null;
        return $this->state['blobs'][array_rand($this->state['blobs'])];
    }

    /**
     * Get a random token from the loaded dictionary, or null if no dictionary loaded.
     */
    public function getRandomDictionaryToken(): ?string {
        if (empty($this->dictionaryTokens)) {
            return null;
        }
        return $this->dictionaryTokens[array_rand($this->dictionaryTokens)];
    }
}

<?php

class FuzzHarness {
    private array $operations = [];
    private float $totalWeight = 0.0;
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
        $this->registerOperation('pconnect', ConnectionOps::pconnect($this), 1.0);
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

    private function cleanup(): void {
        // Close all resources
        foreach ($this->state['blobs'] as $blob) {
            if (is_resource($blob)) @fbird_blob_close($blob);
        }
        foreach ($this->state['statements'] as $stmt) {
            if (is_resource($stmt)) @fbird_free_query($stmt);
        }
        foreach ($this->state['transactions'] as $trans) {
            if (is_resource($trans)) @fbird_rollback($trans);
        }
        foreach ($this->state['connections'] as $conn) {
            if (is_resource($conn)) @fbird_close($conn);
        }
        
        // Reset state
        $this->state = [
            'connections' => [],
            'transactions' => [],
            'statements' => [],
            'blobs' => [],
        ];
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
}

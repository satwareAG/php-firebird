<?php

class FuzzResult {
    public int $iterations = 0;
    public int $passed = 0;
    public int $failed = 0;
    public array $errors = [];
    public array $coverage = [];
    public float $duration = 0.0;
    public float $startTime;

    public function __construct() {
        $this->startTime = microtime(true);
    }

    public function recordSuccess(string $operation): void {
        $this->iterations++;
        $this->passed++;
        $this->recordCoverage($operation);
    }

    public function recordFailure(string $operation, array $error): void {
        $this->iterations++;
        $this->failed++;
        $this->errors[] = [
            'operation' => $operation,
            'error' => $error,
            'timestamp' => microtime(true)
        ];
        $this->recordCoverage($operation);
    }

    private function recordCoverage(string $operation): void {
        if (!isset($this->coverage[$operation])) {
            $this->coverage[$operation] = 0;
        }
        $this->coverage[$operation]++;
    }

    public function finish(): void {
        $this->duration = microtime(true) - $this->startTime;
    }

    public function toArray(): array {
        return [
            'iterations' => $this->iterations,
            'passed' => $this->passed,
            'failed' => $this->failed,
            'errors' => $this->errors,
            'coverage' => $this->coverage,
            'duration' => $this->duration
        ];
    }
}

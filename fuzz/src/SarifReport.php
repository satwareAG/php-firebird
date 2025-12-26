<?php

class SarifReport {
    private string $schema = 'https://json.schemastore.org/sarif-2.1.0.json';
    private string $version = '2.1.0';
    private array $runs = [];

    public function __construct(string $toolName = 'php-firebird-fuzzer', string $toolVersion = '1.0.0') {
        $this->addRun($toolName, $toolVersion);
    }

    public function addRun(string $toolName, string $toolVersion): int {
        $this->runs[] = [
            'tool' => [
                'driver' => [
                    'name' => $toolName,
                    'version' => $toolVersion,
                    'rules' => []
                ]
            ],
            'results' => [],
            'invocations' => []
        ];
        return count($this->runs) - 1;
    }

    public function addResult(int $runIndex, string $ruleId, string $level, string $message, ?array $location = null, ?array $stack = null): void {
        if (!isset($this->runs[$runIndex])) {
            return;
        }

        $result = [
            'ruleId' => $ruleId,
            'level' => $level,
            'message' => [
                'text' => $message
            ]
        ];

        if ($location) {
            $result['locations'] = [[
                'physicalLocation' => [
                    'artifactLocation' => [
                        'uri' => $location['file']
                    ],
                    'region' => [
                        'startLine' => $location['line']
                    ]
                ]
            ]];
        }

        if ($stack) {
            $frames = [];
            foreach ($stack as $frame) {
                $frames[] = [
                    'location' => [
                        'message' => [
                            'text' => $frame['function'] ?? 'unknown'
                        ],
                        'physicalLocation' => [
                            'artifactLocation' => [
                                'uri' => $frame['file'] ?? 'unknown'
                            ],
                            'region' => [
                                'startLine' => $frame['line'] ?? 0
                            ]
                        ]
                    ]
                ];
            }
            $result['stacks'] = [[
                'frames' => $frames
            ]];
        }

        // Generate fingerprint for deduplication
        $result['fingerprints'] = [
            'contentHash/v1' => md5($ruleId . $message . ($location['file'] ?? '') . ($location['line'] ?? ''))
        ];

        $this->runs[$runIndex]['results'][] = $result;
    }

    public function setInvocation(int $runIndex, bool $success, ?string $exitCode = null): void {
        if (!isset($this->runs[$runIndex])) {
            return;
        }

        $invocation = [
            'executionSuccessful' => $success,
            'endTimeUtc' => gmdate('Y-m-d\TH:i:s\Z')
        ];

        if ($exitCode !== null) {
            $invocation['exitCode'] = (int)$exitCode;
        }

        $this->runs[$runIndex]['invocations'][] = $invocation;
    }

    public function toJson(int $options = JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES): string {
        return json_encode($this->toArray(), $options);
    }

    public function toArray(): array {
        return [
            '$schema' => $this->schema,
            'version' => $this->version,
            'runs' => $this->runs
        ];
    }
}

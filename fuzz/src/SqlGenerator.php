<?php
/**
 * SqlGenerator - Random SQL Query Generator for Fuzzing
 *
 * Generates random SQL queries to hit unexpected paths in firebird_utils.cpp.
 * Used by the fuzzer to exercise edge cases in query parsing, binding, and execution.
 *
 * @see https://github.com/satwareAG/php-firebird/issues/32
 */

declare(strict_types=1);

class SqlGenerator
{
    // System tables available in all Firebird databases
    private const SYSTEM_TABLES = [
        'RDB$DATABASE',
        'RDB$RELATIONS',
        'RDB$FIELDS',
        'RDB$RELATION_FIELDS',
        'RDB$TYPES',
        'RDB$FORMATS',
        'RDB$INDICES',
        'RDB$INDEX_SEGMENTS',
        'RDB$GENERATORS',
        'RDB$TRIGGERS',
        'RDB$PROCEDURES',
        'RDB$PROCEDURE_PARAMETERS',
        'RDB$DEPENDENCIES',
        'RDB$FUNCTIONS',
        'RDB$CHARACTER_SETS',
        'RDB$COLLATIONS',
    ];

    // Column types for random value generation
    private const VALUE_TYPES = [
        'null',
        'integer',
        'bigint',
        'smallint',
        'float',
        'double',
        'string',
        'unicode',
        'binary',
        'date',
        'time',
        'timestamp',
        'boolean',
    ];

    // SQL comparison operators
    private const OPERATORS = [
        '=',
        '<>',
        '<',
        '>',
        '<=',
        '>=',
        'LIKE',
        'IS NULL',
        'IS NOT NULL',
        'STARTING WITH',
        'CONTAINING',
    ];

    /**
     * Generate a random SELECT query against system tables
     *
     * @return array{sql: string, params: array<mixed>}
     */
    public function generateSelect(): array
    {
        $table = $this->randomElement(self::SYSTEM_TABLES);
        $limit = rand(1, 100);

        // Randomly decide query complexity
        $complexity = rand(1, 4);

        switch ($complexity) {
            case 1:
                // Simple: SELECT * with FIRST
                return [
                    'sql' => "SELECT FIRST {$limit} * FROM {$table}",
                    'params' => [],
                ];

            case 2:
                // Medium: SELECT with WHERE clause using placeholder
                $whereCol = $this->getSystemTableColumn($table);
                $operator = $this->randomElement(['=', '<>', 'LIKE', 'STARTING WITH']);
                $param = $this->generateRandomValue('string');
                
                if ($operator === 'LIKE') {
                    $param = '%' . $param . '%';
                }
                
                return [
                    'sql' => "SELECT FIRST {$limit} * FROM {$table} WHERE {$whereCol} {$operator} ?",
                    'params' => [$param],
                ];

            case 3:
                // Complex: Multiple WHERE conditions
                $whereCol1 = $this->getSystemTableColumn($table);
                $whereCol2 = $this->getSystemTableColumn($table);
                $param1 = $this->generateRandomValue('string');
                $param2 = $this->generateRandomValue('integer');
                
                return [
                    'sql' => "SELECT FIRST {$limit} * FROM {$table} WHERE {$whereCol1} LIKE ? OR {$whereCol2} IS NOT NULL",
                    'params' => ['%' . $param1 . '%'],
                ];

            case 4:
            default:
                // Edge case: Empty result set query
                return [
                    'sql' => "SELECT FIRST 1 * FROM {$table} WHERE 1=0",
                    'params' => [],
                ];
        }
    }

    /**
     * Generate a random INSERT statement (uses temp table created by fuzzer)
     *
     * @return array{sql: string, params: array<mixed>}
     */
    public function generateInsert(): array
    {
        $columnCount = rand(1, 5);
        $columns = [];
        $placeholders = [];
        $params = [];
        $types = ['integer', 'string', 'float', 'null', 'date'];

        for ($i = 0; $i < $columnCount; $i++) {
            $columns[] = "COL" . ($i + 1);
            $placeholders[] = '?';
            $type = $this->randomElement($types);
            $params[] = $this->generateRandomValue($type);
        }

        return [
            'sql' => "INSERT INTO FUZZ_TEST (" . implode(', ', $columns) . ") VALUES (" . implode(', ', $placeholders) . ")",
            'params' => $params,
        ];
    }

    /**
     * Generate random parameter set with mixed types
     *
     * @param int $count Number of parameters to generate
     * @return array<mixed>
     */
    public function generateRandomParams(int $count): array
    {
        $params = [];
        for ($i = 0; $i < $count; $i++) {
            $type = $this->randomElement(self::VALUE_TYPES);
            $params[] = $this->generateRandomValue($type);
        }
        return $params;
    }

    /**
     * Generate a random value of the specified type
     *
     * @param string $type One of VALUE_TYPES
     * @return mixed
     */
    public function generateRandomValue(string $type): mixed
    {
        switch ($type) {
            case 'null':
                return null;

            case 'integer':
                // Include edge cases
                $edgeCases = [0, -1, 1, PHP_INT_MIN, PHP_INT_MAX, 2147483647, -2147483648];
                if (rand(1, 10) <= 3) {
                    return $this->randomElement($edgeCases);
                }
                return rand(-2147483648, 2147483647);

            case 'bigint':
                // 64-bit integers
                if (rand(1, 10) <= 3) {
                    return PHP_INT_MAX;
                }
                return (int)(rand() * rand());

            case 'smallint':
                return rand(-32768, 32767);

            case 'float':
                $edgeCases = [0.0, -0.0, 1.0, -1.0, PHP_FLOAT_MIN, PHP_FLOAT_MAX, INF, -INF, NAN];
                if (rand(1, 10) <= 3) {
                    return $this->randomElement($edgeCases);
                }
                return (float)rand() / (float)rand(1, PHP_INT_MAX);

            case 'double':
                return (float)rand() / (float)rand(1, PHP_INT_MAX) * pow(10, rand(-10, 10));

            case 'string':
                return $this->generateRandomString(rand(0, 255));

            case 'unicode':
                return $this->generateRandomUnicodeString(rand(0, 100));

            case 'binary':
                return $this->generateRandomBinary(rand(0, 1024));

            case 'date':
                // Random date between 1900 and 2100
                $year = rand(1900, 2100);
                $month = rand(1, 12);
                $day = rand(1, 28);
                return sprintf('%04d-%02d-%02d', $year, $month, $day);

            case 'time':
                return sprintf('%02d:%02d:%02d', rand(0, 23), rand(0, 59), rand(0, 59));

            case 'timestamp':
                $year = rand(1900, 2100);
                $month = rand(1, 12);
                $day = rand(1, 28);
                return sprintf('%04d-%02d-%02d %02d:%02d:%02d',
                    $year, $month, $day,
                    rand(0, 23), rand(0, 59), rand(0, 59)
                );

            case 'boolean':
                return rand(0, 1) === 1;

            default:
                return $this->generateRandomString(rand(1, 50));
        }
    }

    /**
     * Generate a random ASCII string
     */
    public function generateRandomString(int $length): string
    {
        if ($length === 0) {
            return '';
        }

        // Include edge case characters
        $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        $edgeChars = "'\"\\\0\n\r\t";

        $result = '';
        for ($i = 0; $i < $length; $i++) {
            if (rand(1, 20) === 1) {
                // 5% chance of edge case character
                $result .= $edgeChars[rand(0, strlen($edgeChars) - 1)];
            } else {
                $result .= $chars[rand(0, strlen($chars) - 1)];
            }
        }

        return $result;
    }

    /**
     * Generate a random Unicode string with various scripts
     */
    public function generateRandomUnicodeString(int $length): string
    {
        if ($length === 0) {
            return '';
        }

        // Unicode ranges for various scripts
        $unicodeRanges = [
            [0x0041, 0x005A], // Latin uppercase
            [0x0061, 0x007A], // Latin lowercase
            [0x00C0, 0x00FF], // Latin Extended-A
            [0x0100, 0x017F], // Latin Extended-B
            [0x0391, 0x03C9], // Greek
            [0x0410, 0x044F], // Cyrillic
            [0x4E00, 0x4EFF], // CJK subset
            [0x3040, 0x309F], // Hiragana
            [0x1F600, 0x1F64F], // Emoticons
        ];

        $result = '';
        for ($i = 0; $i < $length; $i++) {
            $range = $this->randomElement($unicodeRanges);
            $codepoint = rand($range[0], $range[1]);
            $result .= mb_chr($codepoint, 'UTF-8');
        }

        return $result;
    }

    /**
     * Generate random binary data
     */
    public function generateRandomBinary(int $length): string
    {
        if ($length === 0) {
            return '';
        }
        return random_bytes($length);
    }

    /**
     * Get a known column name for system tables
     */
    private function getSystemTableColumn(string $table): string
    {
        $columns = [
            'RDB$DATABASE' => ['RDB$DESCRIPTION', 'RDB$SECURITY_CLASS'],
            'RDB$RELATIONS' => ['RDB$RELATION_NAME', 'RDB$OWNER_NAME', 'RDB$DESCRIPTION'],
            'RDB$FIELDS' => ['RDB$FIELD_NAME', 'RDB$DESCRIPTION'],
            'RDB$RELATION_FIELDS' => ['RDB$FIELD_NAME', 'RDB$RELATION_NAME'],
            'RDB$TYPES' => ['RDB$TYPE_NAME', 'RDB$FIELD_NAME'],
            'RDB$FORMATS' => ['RDB$RELATION_ID'],
            'RDB$INDICES' => ['RDB$INDEX_NAME', 'RDB$RELATION_NAME'],
            'RDB$INDEX_SEGMENTS' => ['RDB$INDEX_NAME', 'RDB$FIELD_NAME'],
            'RDB$GENERATORS' => ['RDB$GENERATOR_NAME', 'RDB$DESCRIPTION'],
            'RDB$TRIGGERS' => ['RDB$TRIGGER_NAME', 'RDB$RELATION_NAME'],
            'RDB$PROCEDURES' => ['RDB$PROCEDURE_NAME', 'RDB$OWNER_NAME'],
            'RDB$PROCEDURE_PARAMETERS' => ['RDB$PARAMETER_NAME', 'RDB$PROCEDURE_NAME'],
            'RDB$DEPENDENCIES' => ['RDB$DEPENDENT_NAME', 'RDB$DEPENDED_ON_NAME'],
            'RDB$FUNCTIONS' => ['RDB$FUNCTION_NAME'],
            'RDB$CHARACTER_SETS' => ['RDB$CHARACTER_SET_NAME'],
            'RDB$COLLATIONS' => ['RDB$COLLATION_NAME'],
        ];

        $tableCols = $columns[$table] ?? ['RDB$DESCRIPTION'];
        return $this->randomElement($tableCols);
    }

    /**
     * Select a random element from an array
     *
     * @template T
     * @param array<T> $array
     * @return T
     */
    private function randomElement(array $array): mixed
    {
        return $array[array_rand($array)];
    }
}
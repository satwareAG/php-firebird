# Firebird Client Library (libfbclient) Backwards Compatibility Research

**Date**: 2025-12-30  
**Author**: AI Research (Deep Research via Perplexity)  
**Purpose**: Determine if php-firebird distribution releases can bundle only the latest Firebird client library

## Executive Summary

**CONCLUSION: YES** - Distribution releases can bundle only the latest Firebird 5.x client library (`libfbclient`). The Firebird wire protocol negotiation mechanism ensures backwards compatibility with older Firebird servers (4.x, 3.x, 2.5.x) when using TCP/IP connections.

## Key Findings

### 1. Wire Protocol Backwards Compatibility

Firebird uses a **versioned wire protocol** system where client and server negotiate the highest common protocol version during connection establishment.

| Protocol Version | Firebird Version | Key Features |
|-----------------|------------------|--------------|
| 10 | 1.0+ | Baseline (inherited from InterBase 6.0) |
| 11 | 2.1 | Message batching, lazy responses |
| 12 | 2.5 | Asynchronous cancellation |
| 13 | 3.0 | Auth plugins, encryption, compression |
| 14-15 | 3.0.1-3.0.2 | DB encryption key callback fixes |
| 16 | 4.0 | Statement timeouts |
| 17 | 4.0.1 | Minor improvements |
| 18 | 5.0 | Scrollable cursors |
| 19 | 5.0.3 | Inline blobs |

### 2. How Protocol Negotiation Works

1. Client sends identification message with list of supported protocol versions
2. Server examines the list and selects the **highest version both sides support**
3. Connection proceeds using that protocol version

**Example**: Firebird 5.0 client (supports protocols 10-19) connecting to Firebird 2.5 server (supports up to protocol 12) → Connection uses Protocol 12.

### 3. Confirmed Compatibility

✅ **Firebird 5.0 client → Firebird 2.5 server**: Works via TCP/IP  
✅ **Firebird 5.0 client → Firebird 3.0 server**: Works via TCP/IP  
✅ **Firebird 5.0 client → Firebird 4.0 server**: Works via TCP/IP  
✅ **Firebird 5.0 client → Firebird 5.0 server**: Full feature support

### 4. Limitations (Important to Document)

#### 4.1 Asymmetric Compatibility

- **Newer client → Older server**: ✅ Works well
- **Older client → Newer server**: ⚠️ May have issues with new data types

If using an old client (e.g., Firebird 2.5) to connect to a newer server (e.g., Firebird 5.0), problems occur with:
- DECFLOAT data type (Firebird 4.0+)
- INT128 data type (Firebird 4.0+)
- TIME/TIMESTAMP WITH TIME ZONE (Firebird 4.0+)
- Extended precision NUMERIC types (Firebird 4.0+)
- Longer metadata names (Firebird 3.0+)

#### 4.2 Windows XNET Exception

**Critical**: Windows local connections via XNET protocol require **exact version matching** between client and server binaries.

- XNET uses shared memory communication with binary-specific layouts
- Workaround: Use TCP/IP (`localhost` loopback) instead of XNET for cross-version connections

**For php-firebird**: This is acceptable since:
1. Most users connect via TCP/IP (network or localhost)
2. We document this limitation for Windows local connections

#### 4.3 Authentication Considerations

When connecting to Firebird 2.5 servers:
- Must use `Legacy_Auth` authentication plugin
- Server must have `WireCrypt = Enabled` or `Disabled` (not `Required`)
- Firebird 5.0 client includes `Legacy_Auth` support by default

### 5. Feature Availability Matrix

| Feature | Protocol | Available on 2.5 | Available on 3.0 | Available on 4.0 | Available on 5.0 |
|---------|----------|-----------------|-----------------|-----------------|-----------------|
| Basic SQL | 10 | ✅ | ✅ | ✅ | ✅ |
| Message batching | 11 | ✅ | ✅ | ✅ | ✅ |
| Async cancel | 12 | ✅ | ✅ | ✅ | ✅ |
| Wire encryption | 13 | ❌ | ✅ | ✅ | ✅ |
| Wire compression | 13 | ❌ | ✅ | ✅ | ✅ |
| Statement timeouts | 16 | ❌ | ❌ | ✅ | ✅ |
| Scrollable cursors | 18 | ❌ | ❌ | ❌ | ✅ |
| Inline blobs | 19 | ❌ | ❌ | ❌ | ✅ (5.0.3+) |

### 6. ODS vs Wire Protocol (Important Distinction)

**ODS (On-Disk Structure)**: Database file format - determines which server can **open** a database file.

| Firebird Version | ODS Version |
|-----------------|-------------|
| 2.5 | 11.2 |
| 3.0 | 12.0 |
| 4.0 | 13.0 |
| 5.0 | 13.1 |

**Wire Protocol**: Client-server communication - determines which client can **connect** to a server.

These are **independent**:
- A Firebird 5.0 client can connect to a Firebird 2.5 server via protocol negotiation
- But only a Firebird 2.5 server can open ODS 11.2 databases natively

## Implications for php-firebird Distribution

### Recommended Approach: Single Bundle Strategy

**Bundle only the latest Firebird 5.x client library** in all distribution releases.

#### Benefits

1. **Simplified maintenance**: One bundle per PHP version per platform
2. **Reduced download sizes**: No need for multiple Firebird version bundles
3. **Future-proof**: Latest client supports all protocols
4. **Security**: Latest client has newest security fixes and SRP authentication

#### Bundle Matrix (Simplified)

| Platform | PHP Version | Bundled libfbclient |
|----------|-------------|---------------------|
| Linux x86_64 | 8.1 | Firebird 5.0.x |
| Linux x86_64 | 8.2 | Firebird 5.0.x |
| Linux x86_64 | 8.3 | Firebird 5.0.x |
| Linux x86_64 | 8.4 | Firebird 5.0.x |

### Documentation Requirements

Users must be informed:

1. **Supported server versions**: Firebird 2.5, 3.0, 4.0, 5.0 (all supported via TCP/IP)
2. **Windows XNET limitation**: Use TCP/IP for cross-version local connections
3. **Feature availability**: Some features require minimum server version
4. **Authentication**: Firebird 2.5 connections use Legacy_Auth

### Testing Requirements

**Testing completed on 2025-12-30** with the following results:

#### Connection Tests ✅
| Server | Version | Protocol | Status |
|--------|---------|----------|--------|
| FB 2.5 | 2.5.9 | 10-12 | ✅ Connected |
| FB 3.0 | 3.0.13 | 13-15 | ✅ Connected |
| FB 4.0 | 4.0.6 | 16-17 | ✅ Connected |
| FB 5.0 | 5.0.3 | 18-19 | ✅ Connected |

#### CRUD Operations ✅
| Server | INSERT | SELECT | UPDATE | DELETE |
|--------|--------|--------|--------|--------|
| FB 2.5 | ✅ | ✅ | ✅ | ✅ |
| FB 3.0 | ✅ | ✅ | ✅ | ✅ |
| FB 4.0 | ✅ | ✅ | ✅ | ✅ |
| FB 5.0 | ✅ | ✅ | ✅ | ✅ |

#### Transaction Handling ✅
| Server | COMMIT | ROLLBACK |
|--------|--------|----------|
| FB 2.5 | ✅ | ✅ |
| FB 3.0 | ✅ | ✅ |
| FB 4.0 | ✅ | ✅ |
| FB 5.0 | ✅ | ✅ |

#### Test Environment
- **Client**: PHP 8.5.1 with php-firebird extension
- **Client Library**: Firebird 5.0.3 (libfbclient)
- **Container**: `php85-fb5-dev` (Docker)
- **Protocol**: TCP/IP via Docker networking

#### Test Script
See `scripts/test-server-compatibility.sh` for automated testing.

#### Conclusion
**The single-bundle distribution strategy is validated.** The Firebird 5.x client library successfully connects to and operates with all tested server versions (2.5, 3.0, 4.0, 5.0) via wire protocol negotiation.

## Sources

1. Firebird Wire Protocol Documentation (firebird.sourceforge.net)
2. Firebird 5.0 Release Notes
3. Firebird 5.0 Quick Start Guide
4. Firebird Community Forums (April 2024 discussion confirming 5.0→2.5 compatibility)
5. Jaybird JDBC Driver Documentation
6. Firebird Release Policy Documentation

## Appendix: Wire Protocol Technical Details

### Protocol Version Masking

Protocol versions > 10 are transmitted with a bitmask: `version | 0x8000` (FB_PROTOCOL_FLAG)

Example: Protocol 13 → `0x8000 | 13` = `32781` (0x800D)

This distinguishes Firebird protocols from InterBase protocols with the same numeric value.

### Protocol Negotiation Message Flow

```
Client → Server: CONNECT (protocols: [19, 18, 16, 15, 14, 13, 12, 11, 10])
Server → Client: ACCEPT (selected_protocol: 12)  // For Firebird 2.5 server
                                                  // or higher for newer servers
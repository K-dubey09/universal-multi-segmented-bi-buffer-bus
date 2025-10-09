/*
 * Universal Multi-Segmented Bi-Buffer Bus (UMSBB) - TypeScript Definitions
 * 
 * Copyright (c) 2025 Kushagra Dubey
 * Licensed under the MIT License - see LICENSE file for details.
 */

/**
 * UMSBB JavaScript Connector TypeScript Definitions
 * High-performance WebAssembly interface for UMSBB ring buffer system
 */

declare namespace UMSBB {
    interface BufferStats {
        totalMessages: number;
        pendingMessages: number;
        useMockMode: boolean;
        bufferId?: number;
        totalBytes?: number;
        averageMessageSize?: number;
    }

    interface PerformanceTestResult {
        messagesProcessed: number;
        duration: number;
        throughputMsgPerSec: number;
    }
}

/**
 * Custom error class for UMSBB operations
 */
export declare class UMSBBError extends Error {
    code: number | null;
    
    constructor(message: string, code?: number | null);
}

/**
 * Main UMSBB Buffer class
 */
export declare class UMSBBBuffer {
    static readonly SUCCESS: 0;
    static readonly ERROR_BUFFER_EMPTY: -3;
    
    readonly sizeMB: number;
    readonly isInitialized: boolean;
    readonly useMockMode: boolean;

    /**
     * Create a new UMSBB buffer
     * @param sizeMB Buffer size in megabytes (1-64)
     * @param numSegments Number of segments (1-16)
     */
    constructor(sizeMB?: number, numSegments?: number);

    /**
     * Initialize the buffer and WebAssembly module
     */
    initialize(): Promise<void>;

    /**
     * Write data to the buffer
     * @param data Data to write (string, ArrayBuffer, or Uint8Array)
     */
    write(data: string | ArrayBuffer | Uint8Array): Promise<number>;

    /**
     * Read data from the buffer
     * @returns Message data or null if buffer is empty
     */
    read(): Promise<string | Uint8Array | null>;

    /**
     * Get buffer statistics
     */
    getStats(): Promise<UMSBB.BufferStats>;

    /**
     * Close the buffer and cleanup resources
     */
    close(): Promise<void>;
}

/**
 * Factory function to create and initialize a buffer
 * @param sizeMB Buffer size in megabytes
 * @param numSegments Number of segments
 */
export declare function createBuffer(sizeMB?: number, numSegments?: number): Promise<UMSBBBuffer>;

/**
 * Run performance test
 * @param messageCount Number of messages to test
 * @param sizeMB Buffer size in megabytes
 */
export declare function performanceTest(messageCount?: number, sizeMB?: number): Promise<void>;

// Global exports for browser environment
declare global {
    interface Window {
        UMSBBBuffer: typeof UMSBBBuffer;
        UMSBBError: typeof UMSBBError;
        createBuffer: typeof createBuffer;
        performanceTest: typeof performanceTest;
    }
}
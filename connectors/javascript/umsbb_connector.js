// UMSBB JavaScript Connector v4.0
class UMSBBError extends Error {
    constructor(message, code = null) {
        super(message);
        this.name = 'UMSBBError';
        this.code = code;
    }
}

class UMSBBBuffer {
    static SUCCESS = 0;
    static ERROR_BUFFER_EMPTY = -3;

    constructor(sizeMB = 16) {
        this.sizeMB = sizeMB;
        this.isInitialized = false;
        this.useMockMode = false;
        this.mockData = { messages: [], totalWritten: 0 };
    }

    async initialize() {
        if (this.isInitialized) return;
        console.log('Initializing UMSBB buffer in mock mode');
        this.useMockMode = true;
        this.isInitialized = true;
    }

    async write(data) {
        if (!this.isInitialized) await this.initialize();
        this.mockData.messages.push(data);
        this.mockData.totalWritten++;
        return 0;
    }

    async read() {
        if (!this.isInitialized) await this.initialize();
        if (this.mockData.messages.length === 0) return null;
        return this.mockData.messages.shift();
    }

    async getStats() {
        if (!this.isInitialized) await this.initialize();
        return {
            totalMessages: this.mockData.totalWritten,
            pendingMessages: this.mockData.messages.length,
            useMockMode: this.useMockMode
        };
    }

    async close() {
        this.isInitialized = false;
        console.log('Buffer closed');
    }
}

async function createBuffer(sizeMB = 16) {
    const buffer = new UMSBBBuffer(sizeMB);
    await buffer.initialize();
    return buffer;
}

async function performanceTest(messageCount = 100) {
    console.log('UMSBB Performance Test');
    console.log('===========================');
    
    const buffer = await createBuffer();
    const start = Date.now();
    
    for (let i = 0; i < messageCount; i++) {
        await buffer.write('Message ' + i);
    }
    
    let readCount = 0;
    while (readCount < messageCount) {
        const msg = await buffer.read();
        if (msg) readCount++;
    }
    
    const duration = (Date.now() - start) / 1000;
    const stats = await buffer.getStats();
    
    console.log('Results:');
    console.log('Messages: ' + stats.totalMessages);
    console.log('Duration: ' + duration.toFixed(3) + 's');
    console.log('Throughput: ' + Math.round(stats.totalMessages / duration) + ' msg/s');
    
    await buffer.close();
}

// Exports
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { UMSBBBuffer, UMSBBError, createBuffer, performanceTest };
} else if (typeof window !== 'undefined') {
    window.UMSBBBuffer = UMSBBBuffer;
    window.UMSBBError = UMSBBError;
    window.createBuffer = createBuffer;
    window.performanceTest = performanceTest;
}
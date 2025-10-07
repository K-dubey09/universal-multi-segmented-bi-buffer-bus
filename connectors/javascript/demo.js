/**
 * UMSBB JavaScript Connector Demo
 * Demonstrates basic usage and capabilities
 */

const { createBuffer, UMSBBBuffer, UMSBBError, performanceTest } = require('./umsbb_connector.js');

async function basicDemo() {
    console.log('🚀 UMSBB JavaScript Connector Demo');
    console.log('================================');

    try {
        // Create a 32MB buffer with 4 segments
        console.log('\n📦 Creating buffer (32MB, 4 segments)...');
        const buffer = await createBuffer(32, 4);
        console.log('✅ Buffer created successfully');

        // Write some messages
        console.log('\n📝 Writing messages...');
        const messages = [
            'Hello, UMSBB!',
            'This is a test message',
            { type: 'json', data: { id: 1, value: 'test' } },
            'Final message'
        ];

        for (let i = 0; i < messages.length; i++) {
            const msg = typeof messages[i] === 'object' ? JSON.stringify(messages[i]) : messages[i];
            await buffer.write(msg);
            console.log(`   ✓ Written: "${msg}"`);
        }

        // Check stats after writing
        console.log('\n📊 Buffer stats after writing:');
        const statsAfterWrite = await buffer.getStats();
        console.log('   Total messages:', statsAfterWrite.totalMessages);
        console.log('   Pending messages:', statsAfterWrite.pendingMessages);
        console.log('   Mock mode:', statsAfterWrite.useMockMode);

        // Read messages back
        console.log('\n📖 Reading messages...');
        let readCount = 0;
        while (true) {
            const message = await buffer.read();
            if (message === null) {
                console.log('   📭 No more messages');
                break;
            }
            readCount++;
            console.log(`   ✓ Read #${readCount}: "${message}"`);
        }

        // Final stats
        console.log('\n📊 Final buffer stats:');
        const finalStats = await buffer.getStats();
        console.log('   Total messages:', finalStats.totalMessages);
        console.log('   Pending messages:', finalStats.pendingMessages);

        // Clean up
        await buffer.close();
        console.log('\n🔒 Buffer closed');

    } catch (error) {
        console.error('❌ Error:', error.message);
        if (error instanceof UMSBBError) {
            console.error('   Error code:', error.code);
        }
    }
}

async function errorHandlingDemo() {
    console.log('\n🛡️ Error Handling Demo');
    console.log('====================');

    try {
        // Test invalid buffer size
        console.log('\n🔍 Testing invalid buffer size...');
        try {
            new UMSBBBuffer(100); // Should throw error
        } catch (error) {
            console.log('   ✅ Caught expected error:', error.message);
        }

        // Test reading from empty buffer
        console.log('\n🔍 Testing empty buffer read...');
        const buffer = await createBuffer(16);
        const emptyRead = await buffer.read();
        console.log('   ✅ Empty read result:', emptyRead);

        await buffer.close();

    } catch (error) {
        console.error('❌ Unexpected error:', error.message);
    }
}

async function concurrencyDemo() {
    console.log('\n⚡ Concurrency Demo');
    console.log('=================');

    const buffer = await createBuffer(64);
    const messageCount = 100;

    console.log(`\n🔄 Producer/Consumer with ${messageCount} messages...`);

    // Producer function
    const producer = async () => {
        for (let i = 0; i < messageCount; i++) {
            await buffer.write(`Message-${i}`);
            if (i % 20 === 0) {
                console.log(`   📤 Produced ${i} messages`);
            }
        }
        console.log('   ✅ Producer finished');
    };

    // Consumer function
    const consumer = async () => {
        let consumed = 0;
        while (consumed < messageCount) {
            const msg = await buffer.read();
            if (msg) {
                consumed++;
                if (consumed % 20 === 0) {
                    console.log(`   📥 Consumed ${consumed} messages`);
                }
            } else {
                // Brief pause if no message available
                await new Promise(resolve => setTimeout(resolve, 1));
            }
        }
        console.log('   ✅ Consumer finished');
    };

    // Run producer and consumer concurrently
    const start = Date.now();
    await Promise.all([producer(), consumer()]);
    const duration = (Date.now() - start) / 1000;

    console.log(`\n⏱️ Completed in ${duration.toFixed(3)}s`);
    console.log(`📈 Throughput: ${Math.round(messageCount / duration)} msg/s`);

    await buffer.close();
}

// Main demo function
async function runDemo() {
    await basicDemo();
    await errorHandlingDemo();
    await concurrencyDemo();
    
    console.log('\n🎯 Performance Test');
    console.log('==================');
    await performanceTest(5000, 64);
    
    console.log('\n🎉 Demo completed successfully!');
    console.log('\nTo integrate UMSBB into your project:');
    console.log('1. const { createBuffer } = require("./umsbb_connector.js");');
    console.log('2. const buffer = await createBuffer(sizeMB, numSegments);');
    console.log('3. await buffer.write("your message");');
    console.log('4. const message = await buffer.read();');
    console.log('5. await buffer.close();');
}

// Run demo if called directly
if (require.main === module) {
    runDemo().catch(console.error);
}

module.exports = { runDemo };
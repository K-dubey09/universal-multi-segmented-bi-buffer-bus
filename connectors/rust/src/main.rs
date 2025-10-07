use umsbb_connector::*;

fn main() -> UMSBBResult<()> {
    println!("UMSBB Rust Connector Example");
    println!("{}", "=".repeat(40));

    // Create buffer
    let buffer = create_buffer(32)?;
    println!("Created 32MB buffer");

    // Write some test messages
    for i in 0..5 {
        let message = format!("Test message {}", i);
        buffer.write_string(&message)?;
        println!("Wrote: {}", message);
    }

    // Read messages back
    println!("\nReading messages:");
    while let Some(message) = buffer.read_string()? {
        println!("Read: {}", message);
    }

    // Show statistics
    let stats = buffer.get_stats();
    println!("\nBuffer Statistics:");
    println!("Total messages: {}", stats.total_messages);
    println!("Total bytes: {}", stats.total_bytes);
    println!("Pending messages: {}", stats.pending_messages);

    // Run performance test
    println!("\nRunning performance test...");
    performance_test(5000, 32)?;

    Ok(())
}
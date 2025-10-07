import UniversalMultiSegmentedBiBufferBusModule from './universal_multi_segmented_bi_buffer_bus.js';

UniversalMultiSegmentedBiBufferBusModule().then(Module => {
  const init = Module._umsbb_init;
  const submit = Module._umsbb_submit;
  const drain = Module._umsbb_drain;
  const free = Module._umsbb_free;

  const busPtr = init(1024, 2048);

  const encoder = new TextEncoder();
  const msg = encoder.encode("Capsule from JS");

  const msgPtr = Module._malloc(msg.length);
  Module.HEAPU8.set(msg, msgPtr);

  submit(busPtr, msgPtr, msg.length);
  drain(busPtr);
  free(busPtr);
});
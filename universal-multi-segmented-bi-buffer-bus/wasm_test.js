const UniversalMultiSegmentedBiBufferBusModule = require('./universal_multi_segmented_bi_buffer_bus.js');
UniversalMultiSegmentedBiBufferBusModule().then(Module => {
  const init = Module._umsbb_init;
  const ptr = init(1024, 2048);
  console.log('umsbb_init returned', ptr);
  const m = Module._malloc(16);
  console.log('malloc returned', m);
  Module._free(m);
  console.log('free ok');
}).catch(e => { console.error(e); process.exit(1); });

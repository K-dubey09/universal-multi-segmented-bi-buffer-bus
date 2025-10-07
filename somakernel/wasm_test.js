const SomakernelModule = require('./somakernel.js');
SomakernelModule().then(Module => {
  const init = Module._somakernel_init;
  const ptr = init(1024, 2048);
  console.log('somakernel_init returned', ptr);
  const m = Module._malloc(16);
  console.log('malloc returned', m);
  Module._free(m);
  console.log('free ok');
}).catch(e => { console.error(e); process.exit(1); });
